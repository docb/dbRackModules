#include "dcb.h"
#include "FDProcessor.hpp"

struct BrickWall {
  float minFreq=20;
  float maxFreq=12000;

  void process(gam::STFT &stft) {
    if(maxFreq>=12000)
      maxFreq=24000;
    for(unsigned k=0;k<stft.numBins();++k) {
      float freq=k*stft.binFreq();
      if(freq<minFreq||freq>maxFreq) {
        stft.bin(k)=0;
      }
    }
  }
};

struct BWF : Module {
  enum ParamId {
    LO_PARAM,HI_PARAM,PARAMS_LEN
  };
  enum InputId {
    V_INPUT,LO_INPUT,HI_INPUT,INPUTS_LEN
  };
  enum OutputId {
    V_OUTPUT,OUTPUTS_LEN
  };
  enum LightId {
    LIGHTS_LEN
  };

  enum FFTLEN {
    FL1024,FL2048,FL4096
  };
  int fftLen=1;
  BrickWall bw;
  FDProcessor<BrickWall,256,gam::HAMMING,gam::COMPLEX,0> fd256[16];
  FDProcessor<BrickWall,512,gam::HAMMING,gam::COMPLEX,0> fd512[16];
  FDProcessor<BrickWall,1024,gam::HAMMING,gam::COMPLEX,0> fd1024[16];
  FDProcessor<BrickWall,2048,gam::HAMMING,gam::COMPLEX,0> fd2048[16];
  FDProcessor<BrickWall,4096,gam::HAMMING,gam::COMPLEX,0> fd4096[16];
  dsp::ClockDivider paramDivider;

  BWF() {
    config(PARAMS_LEN,INPUTS_LEN,OUTPUTS_LEN,LIGHTS_LEN);
    configParam(LO_PARAM,0.f,1,0.f,"Min Frequency"," Hz",0,12000);
    configParam(HI_PARAM,0.f,1.f,1.f,"Max Frequency"," Hz",0,12000);
    configOutput(V_OUTPUT,"V");
    configInput(V_INPUT,"V");
    configInput(LO_INPUT,"Min Frequency (0/10V");
    configInput(HI_INPUT,"Max Frequency (0/10V)");
    configBypass(V_INPUT,V_OUTPUT);
    paramDivider.setDivision(32);
  }

  void onSampleRateChange(const SampleRateChangeEvent &e) override {
    gam::sampleRate(APP->engine->getSampleRate());
  }

  void onAdd(const AddEvent &e) override {
    gam::sampleRate(APP->engine->getSampleRate());
  }

  void process(const ProcessArgs &args) override {
    int channels=inputs[V_INPUT].getChannels();
    if(paramDivider.process()) {
      if(inputs[LO_INPUT].isConnected())
        setImmediateValue(getParamQuantity(LO_PARAM),clamp(inputs[LO_INPUT].getVoltage(),0.f,10.f)*0.1f);
      if(inputs[HI_INPUT].isConnected())
        setImmediateValue(getParamQuantity(HI_PARAM),clamp(inputs[HI_INPUT].getVoltage(),0.f,10.f)*0.1f);
    }
    float minF=params[LO_PARAM].getValue()*12000;
    float maxF=params[HI_PARAM].getValue()*12000;
    for(int c=0;c<channels;c++) {
      bw.minFreq=minF;
      bw.maxFreq=maxF;
      float in=inputs[V_INPUT].getVoltage(c)/5.f;
      float out=0.f;
      switch(fftLen) {
        case FL1024:
          out=fd1024[c].process(in,bw);
          break;
        case FL2048:
          out=fd2048[c].process(in,bw);
          break;
        case FL4096:
          out=fd4096[c].process(in,bw);
          break;
        default:
          break;
      }
      outputs[V_OUTPUT].setVoltage(out*5.f,c);
    }
    outputs[V_OUTPUT].setChannels(channels);
  }
};


struct BWFWidget : ModuleWidget {
  std::vector<std::string> labels={"1024","2048","4096"};

  BWFWidget(BWF *module) {
    setModule(module);
    setPanel(createPanel(asset::plugin(pluginInstance,"res/BWF.svg")));

    addChild(createWidget<ScrewSilver>(Vec(RACK_GRID_WIDTH,0)));
    addChild(createWidget<ScrewSilver>(Vec(box.size.x-2*RACK_GRID_WIDTH,0)));
    addChild(createWidget<ScrewSilver>(Vec(RACK_GRID_WIDTH,RACK_GRID_HEIGHT-RACK_GRID_WIDTH)));
    addChild(createWidget<ScrewSilver>(Vec(box.size.x-2*RACK_GRID_WIDTH,RACK_GRID_HEIGHT-RACK_GRID_WIDTH)));
    float x=1.9f;
    addInput(createInput<SmallPort>(mm2px(Vec(x,MHEIGHT-108-6.287f)),module,BWF::LO_INPUT));
    addParam(createParam<TrimbotWhite>(mm2px(Vec(x,MHEIGHT-99-6.3)),module,BWF::LO_PARAM));
    addInput(createInput<SmallPort>(mm2px(Vec(x,MHEIGHT-84-6.287f)),module,BWF::HI_INPUT));
    addParam(createParam<TrimbotWhite>(mm2px(Vec(x,MHEIGHT-75-6.3)),module,BWF::HI_PARAM));

    addInput(createInput<SmallPort>(mm2px(Vec(x,MHEIGHT-31)),module,BWF::V_INPUT));
    addOutput(createOutput<SmallPort>(mm2px(Vec(x,MHEIGHT-19)),module,BWF::V_OUTPUT));
  }

  void appendContextMenu(Menu *menu) override {
    BWF *module=dynamic_cast<BWF *>(this->module);
    assert(module);
    menu->addChild(new MenuSeparator);

    auto fftSelect=new LabelIntSelectItem(&module->fftLen,labels);
    fftSelect->text="FFT Length";
    fftSelect->rightText=labels[module->fftLen]+"  "+RIGHT_ARROW;
    menu->addChild(fftSelect);
  }
};


Model *modelBWF=createModel<BWF,BWFWidget>("BWF");