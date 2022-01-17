#include "dcb.h"
#include "rnd.h"


struct RndG : Module {
  enum ParamId {
    MULTI_PARAM,CHANNELS_PARAM,PARAMS_LEN
  };
  enum InputId {
    CLK_INPUT,RST_INPUT,SEED_INPUT,PROB_INPUT,

    INPUTS_LEN
  };
  enum OutputId {
    GATE_OUTPUT,TRIG_OUTPUT,CLK_OUTPUT,OUTPUTS_LEN
  };
  enum LightId {
    LIGHTS_LEN
  };

  RND rnd;
  dsp::SchmittTrigger clockTrigger[16];
  dsp::SchmittTrigger rstTrigger;
  dsp::PulseGenerator trgPulseGenerators[16];
  bool lastCoins[16]={};

  RndG() {
    config(PARAMS_LEN,INPUTS_LEN,OUTPUTS_LEN,LIGHTS_LEN);
    configParam(MULTI_PARAM,0.f,1.f,1.f,"Multi Mode");
    configParam(CHANNELS_PARAM,1.f,16.f,8.f,"Polyphonic Channels");
    getParamQuantity(CHANNELS_PARAM)->snapEnabled=true;
    configInput(SEED_INPUT,"Random Seed");
    configInput(CLK_INPUT,"Clock");
    configInput(RST_INPUT,"Reset");
    configOutput(GATE_OUTPUT,"GATE");
    configOutput(TRIG_OUTPUT,"TRIG");
    configOutput(CLK_OUTPUT,"CLK");
  }

  void onAdd(const AddEvent &e) override {
    Module::onAdd(e);
    float seedParam=0;
    if(inputs[SEED_INPUT].isConnected())
      seedParam=inputs[SEED_INPUT].getVoltage()/10.f;
    rnd.reset((unsigned long long)(floor((double)seedParam*(double)ULONG_MAX)));
  }

  void nextMulti(int k) {
    float prob=-clamp(inputs[PROB_INPUT].getNormalPolyVoltage(0,k),-5.f,5.f)/10.f+0.5f;
    bool coin=rnd.nextCoin(prob);
    if(coin) {
      outputs[GATE_OUTPUT].setVoltage(10.f,k);
      if(coin!=lastCoins[k]) {
        trgPulseGenerators[k].trigger(0.01f);
      }
    } else {
      outputs[GATE_OUTPUT].setVoltage(0.f,k);
    }
    lastCoins[k]=coin;
  }

  void nextSingle(int channels) {
    float sum=0.f;
    float props[16];
    for(int k=0;k<channels;k++) {
      props[k]=clamp(inputs[PROB_INPUT].getNormalPolyVoltage(0,k),-5.f,5.f)/10.f+0.5f;
      sum+=props[k];
    }
    float r=float(rnd.nextDouble())*sum;
    float start=0.f;
    float end=0.f;
    for(int k=0;k<channels;k++) {
      end+=props[k];
      bool coin=(props[k]>0.f)&&(r>start)&&(r<=end);
      if(coin) {
        outputs[GATE_OUTPUT].setVoltage(10.f,k);
        //clkPulseGenerators[k].trigger(0.01f);
        if(coin!=lastCoins[k]) {
          trgPulseGenerators[k].trigger(0.01f);
        }
      } else {
        outputs[GATE_OUTPUT].setVoltage(0.f,k);
      }
      lastCoins[k]=coin;
      start=end;
    }
  }

  void process(const ProcessArgs &args) override {
    int channels=params[CHANNELS_PARAM].getValue();
    outputs[TRIG_OUTPUT].setChannels(channels);
    outputs[CLK_OUTPUT].setChannels(channels);
    outputs[GATE_OUTPUT].setChannels(channels);
    if(rstTrigger.process(inputs[RST_INPUT].getVoltage())) {
      float seedParam=0;
      if(inputs[SEED_INPUT].isConnected())
        seedParam=inputs[SEED_INPUT].getVoltage()/10.f;
      rnd.reset((unsigned long long)(floor((double)seedParam*(double)ULONG_MAX)));
    }
    if(inputs[CLK_INPUT].isConnected()) {
      bool multi=params[MULTI_PARAM].getValue()>0.f;
      if(multi) {
        if(inputs[CLK_INPUT].isMonophonic()) {
          for(int k=0;k<channels;k++) {
            if(clockTrigger[k].process(inputs[CLK_INPUT].getVoltage())) {
              nextMulti(k);
            }
          }

        } else {
          channels=inputs[CLK_INPUT].getChannels();
          for(int k=0;k<channels;k++) {
            if(clockTrigger[k].process(inputs[CLK_INPUT].getVoltage(k))) {
              nextMulti(k);
            }
          }
        }
      } else {
        if(clockTrigger[0].process(inputs[CLK_INPUT].getVoltage(0))) {
          nextSingle(channels);
        }
      }
    }
    for(int k=0;k<channels;k++) {
      outputs[CLK_OUTPUT].setVoltage(((outputs[GATE_OUTPUT].getVoltage(k)==10.f)?inputs[CLK_INPUT].getPolyVoltage(k):0.0f),k);
      bool trigger=trgPulseGenerators[k].process(1.0/args.sampleRate);
      outputs[TRIG_OUTPUT].setVoltage((trigger?10.0f:0.0f),k);
    }
  }
};


struct RndGWidget : ModuleWidget {
  RndGWidget(RndG *module) {
    setModule(module);
    setPanel(createPanel(asset::plugin(pluginInstance,"res/RndG.svg")));

    addChild(createWidget<ScrewSilver>(Vec(RACK_GRID_WIDTH,0)));
    addChild(createWidget<ScrewSilver>(Vec(box.size.x-2*RACK_GRID_WIDTH,0)));
    addChild(createWidget<ScrewSilver>(Vec(RACK_GRID_WIDTH,RACK_GRID_HEIGHT-RACK_GRID_WIDTH)));
    addChild(createWidget<ScrewSilver>(Vec(box.size.x-2*RACK_GRID_WIDTH,RACK_GRID_HEIGHT-RACK_GRID_WIDTH)));

    float xpos=1.9f;
    addInput(createInput<SmallPort>(mm2px(Vec(xpos,MHEIGHT-115.f)),module,RndG::CLK_INPUT));
    addInput(createInput<SmallPort>(mm2px(Vec(xpos,MHEIGHT-103.f)),module,RndG::RST_INPUT));
    addInput(createInput<SmallPort>(mm2px(Vec(xpos,MHEIGHT-91.f)),module,RndG::SEED_INPUT));
    addInput(createInput<SmallPort>(mm2px(Vec(xpos,MHEIGHT-79.f)),module,RndG::PROB_INPUT));


    auto multiButton=createParam<SmallButtonWithLabel>(mm2px(Vec(1.5,MHEIGHT-63.f)),module,RndG::MULTI_PARAM);
    multiButton->setLabel("Mlt");
    addParam(multiButton);
    addOutput(createOutput<SmallPort>(mm2px(Vec(xpos,MHEIGHT-55.f)),module,RndG::GATE_OUTPUT));
    addOutput(createOutput<SmallPort>(mm2px(Vec(xpos,MHEIGHT-43.f)),module,RndG::TRIG_OUTPUT));
    addOutput(createOutput<SmallPort>(mm2px(Vec(xpos,MHEIGHT-31.f)),module,RndG::CLK_OUTPUT));
    addParam(createParam<TrimbotWhite>(mm2px(Vec(2,MHEIGHT-19.f)),module,RndG::CHANNELS_PARAM));
  }
};


Model *modelRndG=createModel<RndG,RndGWidget>("RndG");