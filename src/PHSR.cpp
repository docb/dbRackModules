#include "dcb.h"

using simd::float_4;

template<typename T>
struct PhasorOsc {
  T phs=0.f;

  void updatePhs(float sampleTime,T freq) {
    phs+=freq*sampleTime;
    phs-=simd::floor(phs);
  }

  T process() {
    return phs;
  }

  void reset(T trigger) {
    phs=simd::ifelse(trigger,0.f,phs);
  }
};

template<typename T>
struct SinOsc {
  T phs=0.f;

  void updatePhs(float sampleTime,T freq) {
    phs+=simd::fmin(freq*sampleTime,0.5f);
    phs-=simd::floor(phs);
  }

  T process() {
    return simd::sin(phs*2*M_PI)*0.5f+0.5f;
  }

  void reset(T trigger) {
    phs=simd::ifelse(trigger,0.f,phs);
  }
};

template<typename T>
struct TriOsc {
  T phs=0.f;

  void updatePhs(float sampleTime,T freq) {
    phs+=simd::fmin(freq*sampleTime,0.5f);
    phs-=simd::floor(phs);
  }

  T process() {
    return 4.f*simd::fabs(phs-simd::round(phs))-1.f;
  }

  void reset(T trigger) {
    phs=simd::ifelse(trigger,0.f,phs);
  }
};

template<typename T>
struct SquareOsc {
  T phs=0.f;

  void updatePhs(float sampleTime,T freq) {
    phs+=simd::fmin(freq*sampleTime,0.5f);
    phs-=simd::floor(phs);
  }

  T process() {
    return simd::ifelse(phs<0.5f,0.f,1.f);
  }

  void reset(T trigger) {
    phs=simd::ifelse(trigger,0.f,phs);
  }
};

struct PHSR : Module {
  enum ParamId {
    FREQ_PARAM,FM_PARAM,LIN_PARAM,FINE_PARAM,PARAMS_LEN
  };
  enum InputId {
    VOCT_INPUT,RST_INPUT,FM_INPUT,INPUTS_LEN
  };
  enum OutputId {
    PHSR_OUTPUT,TRI_OUTPUT,SIN_OUTPUT,OUTPUTS_LEN
  };
  enum LightId {
    LIGHTS_LEN
  };

  bool square=false;
  SinOsc<float_4> sinOsc[4];
  PhasorOsc<float_4> phsOsc[4];
  TriOsc<float_4> triOsc[4];
  SquareOsc<float_4> sqOsc[4];
  DCBlocker<float_4> dcbSin[4];
  DCBlocker<float_4> dcbTri[4];
  DCBlocker<float_4> dcbPhsr[4];
  bool dcBlock=false;
  dsp::TSchmittTrigger<float_4> rstTriggers4[4];

  PHSR() {
    config(PARAMS_LEN,INPUTS_LEN,OUTPUTS_LEN,LIGHTS_LEN);
    configParam(FREQ_PARAM,-14.f,4.f,0.f,"Frequency"," Hz",2,dsp::FREQ_C4);
    configParam(FM_PARAM,0,1,0,"FM Amount","%",0,100);
    configParam(FINE_PARAM,-100,100,0,"Fine tune"," cents");
    configInput(FM_INPUT,"FM");
    configButton(LIN_PARAM,"Linear");
    configInput(VOCT_INPUT,"V/Oct");
    configInput(RST_INPUT,"Rst");
    configOutput(PHSR_OUTPUT,"Phasor");
    configOutput(SIN_OUTPUT,"Sin");
    configOutput(TRI_OUTPUT,"Tri");
  }

  float_4 getFreq(int c,float sampleRate) {
    float freqParam=params[FREQ_PARAM].getValue();
    float fmParam=params[FM_PARAM].getValue();
    float fineTune=params[FINE_PARAM].getValue();
    bool linear=params[LIN_PARAM].getValue()>0;
    float_4 pitch=freqParam+inputs[VOCT_INPUT].getPolyVoltageSimd<float_4>(c)+fineTune/1200.f;
    float_4 freq;
    if(linear) {
      freq=dsp::FREQ_C4*dsp::approxExp2_taylor5(pitch+30.f)/std::pow(2.f,30.f);
      freq+=dsp::FREQ_C4*inputs[FM_INPUT].getPolyVoltageSimd<float_4>(c)*fmParam;
    } else {
      pitch+=inputs[FM_INPUT].getPolyVoltageSimd<float_4>(c)*fmParam;
      freq=dsp::FREQ_C4*dsp::approxExp2_taylor5(pitch+30.f)/std::pow(2.f,30.f);
    }
    return simd::fmin(freq,sampleRate/2);
  }

  void process(const ProcessArgs &args) override {
    int channels=std::max(inputs[VOCT_INPUT].getChannels(),1);
    if(outputs[PHSR_OUTPUT].isConnected()) {
      if(square) {
        for(int c=0;c<channels;c+=4) {
          float_4 freq=getFreq(c,args.sampleRate);
          sqOsc[c/4].updatePhs(args.sampleTime,freq);
          float_4 rst=inputs[RST_INPUT].getPolyVoltageSimd<float_4>(c);
          float_4 resetTriggered=rstTriggers4[c/4].process(rst,0.1f,2.f);
          sqOsc[c/4].reset(resetTriggered);
          float_4 out=sqOsc[c/4].process()*10.f;
          outputs[PHSR_OUTPUT].setVoltageSimd(out,c);
        }
      } else {
        for(int c=0;c<channels;c+=4) {
          float_4 freq=getFreq(c,args.sampleRate);
          phsOsc[c/4].updatePhs(args.sampleTime,freq);
          float_4 rst=inputs[RST_INPUT].getPolyVoltageSimd<float_4>(c);
          float_4 resetTriggered=rstTriggers4[c/4].process(rst,0.1f,2.f);
          phsOsc[c/4].reset(resetTriggered);
          float_4 out=phsOsc[c/4].process()*10.f-5.f;
          outputs[PHSR_OUTPUT].setVoltageSimd(out,c);
        }
      }
      outputs[PHSR_OUTPUT].setChannels(channels);
    }

    if(outputs[SIN_OUTPUT].isConnected()) {
      for(int c=0;c<channels;c+=4) {
        float_4 freq=getFreq(c,args.sampleRate);
        sinOsc[c/4].updatePhs(args.sampleTime,freq);
        float_4 rst=inputs[RST_INPUT].getPolyVoltageSimd<float_4>(c);
        float_4 resetTriggered=rstTriggers4[c/4].process(rst,0.1f,2.f);
        sinOsc[c/4].reset(resetTriggered);
        float_4 out=sinOsc[c/4].process()*10.f-5.f;
        outputs[SIN_OUTPUT].setVoltageSimd(dcBlock?dcbSin[c].process(out):out,c);
      }
      outputs[SIN_OUTPUT].setChannels(channels);
    }

    if(outputs[TRI_OUTPUT].isConnected()) {
      for(int c=0;c<channels;c+=4) {
        float_4 freq=getFreq(c,args.sampleRate);
        triOsc[c/4].updatePhs(args.sampleTime,freq);
        float_4 rst=inputs[RST_INPUT].getPolyVoltageSimd<float_4>(c);
        float_4 resetTriggered=rstTriggers4[c/4].process(rst,0.1f,2.f);
        triOsc[c/4].reset(resetTriggered);
        float_4 out=triOsc[c/4].process()*5.f;
        outputs[TRI_OUTPUT].setVoltageSimd(dcBlock?dcbTri[c].process(out):out,c);
      }
      outputs[TRI_OUTPUT].setChannels(channels);
    }
  }

  json_t *dataToJson() override {
    json_t *data=json_object();
    json_object_set_new(data,"dcBlock",json_boolean(dcBlock));
    json_object_set_new(data,"square",json_boolean(square));
    return data;
  }

  void dataFromJson(json_t *rootJ) override {
    json_t *jDcBlock=json_object_get(rootJ,"dcBlock");
    if(jDcBlock!=nullptr)
      dcBlock=json_boolean_value(jDcBlock);
    json_t *jSquare=json_object_get(rootJ,"square");
    if(jSquare!=nullptr)
      square=json_boolean_value(jSquare);
  }

};


struct PHSRWidget : ModuleWidget {
  PHSRWidget(PHSR *module) {
    setModule(module);
    setPanel(createPanel(asset::plugin(pluginInstance,"res/PHSR.svg")));

    float x=1.9;
    addParam(createParam<TrimbotWhite>(mm2px(Vec(x,9)),module,PHSR::FREQ_PARAM));
    addParam(createParam<TrimbotWhite>(mm2px(Vec(x,21)),module,PHSR::FINE_PARAM));
    addInput(createInput<SmallPort>(mm2px(Vec(x,33)),module,PHSR::VOCT_INPUT));
    addInput(createInput<SmallPort>(mm2px(Vec(x,46)),module,PHSR::FM_INPUT));
    addParam(createParam<TrimbotWhite>(mm2px(Vec(x,53)),module,PHSR::FM_PARAM));
    addParam(createParam<MLED>(mm2px(Vec(x,65)),module,PHSR::LIN_PARAM));
    addInput(createInput<SmallPort>(mm2px(Vec(x,77)),module,PHSR::RST_INPUT));
    addOutput(createOutput<SmallPort>(mm2px(Vec(x,92)),module,PHSR::TRI_OUTPUT));
    addOutput(createOutput<SmallPort>(mm2px(Vec(x,104)),module,PHSR::SIN_OUTPUT));
    addOutput(createOutput<SmallPort>(mm2px(Vec(x,116)),module,PHSR::PHSR_OUTPUT));
  }

  void appendContextMenu(Menu *menu) override {
    PHSR *module=dynamic_cast<PHSR *>(this->module);
    assert(module);
    menu->addChild(new MenuSeparator);
    menu->addChild(createBoolPtrMenuItem("Block DC","",&module->dcBlock));
    menu->addChild(createBoolPtrMenuItem("Square","",&module->square));
  }
};


Model *modelPHSR=createModel<PHSR,PHSRWidget>("PHSR");