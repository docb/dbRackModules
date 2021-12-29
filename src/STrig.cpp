#include "dcb.h"


struct STrig : Module {
  enum ParamId {
    HIGH_PARAM,LOW_PARAM,PARAMS_LEN
  };
  enum InputId {
    CV_INPUT,HIGH_INPUT,LOW_INPUT,INPUTS_LEN
  };
  enum OutputId {
    GATE_OUTPUT,OUTPUTS_LEN
  };
  enum LightId {
    LIGHTS_LEN
  };

  struct MSchmittTrigger {
    bool state=true;

    bool process(float in,float low,float high) {
      if(state) {
        if(in<=low) {
          state=false;
        }
      } else {
        if(in>=high) {
          state=true;
        }
      }
      return state;
    }

  };

  MSchmittTrigger sm[16];

  STrig() {
    config(PARAMS_LEN,INPUTS_LEN,OUTPUTS_LEN,LIGHTS_LEN);
    configParam(HIGH_PARAM,-10.f,10.f,1.f,"High");
    configInput(HIGH_INPUT,"High Threshold");
    configParam(LOW_PARAM,-10.f,10.f,1.f,"Low");
    configInput(LOW_INPUT,"Low Threshold");
    configOutput(CV_INPUT,"CV");
    configOutput(GATE_OUTPUT,"Gate");
  }

  void process(const ProcessArgs &args) override {
    int channels=inputs[CV_INPUT].getChannels();
    float low = params[LOW_PARAM].getValue();
    float high = params[HIGH_PARAM].getValue();
    for(int k=0;k<channels;k++) {
      if(inputs[LOW_INPUT].isConnected()) low = inputs[LOW_INPUT].getPolyVoltage(k);
      if(inputs[HIGH_INPUT].isConnected()) high = inputs[HIGH_INPUT].getPolyVoltage(k);
      outputs[GATE_OUTPUT].setVoltage(sm[k].process(inputs[CV_INPUT].getVoltage(k),low,high)?10.f:0.f,k);
    }
    outputs[GATE_OUTPUT].setChannels(channels);
  }
};


struct STrigWidget : ModuleWidget {
  STrigWidget(STrig *module) {
    setModule(module);
    setPanel(createPanel(asset::plugin(pluginInstance,"res/STrig.svg")));

    addChild(createWidget<ScrewSilver>(Vec(RACK_GRID_WIDTH,0)));
    addChild(createWidget<ScrewSilver>(Vec(box.size.x-2*RACK_GRID_WIDTH,0)));
    addChild(createWidget<ScrewSilver>(Vec(RACK_GRID_WIDTH,RACK_GRID_HEIGHT-RACK_GRID_WIDTH)));
    addChild(createWidget<ScrewSilver>(Vec(box.size.x-2*RACK_GRID_WIDTH,RACK_GRID_HEIGHT-RACK_GRID_WIDTH)));
    addInput(createInput<SmallPort>(mm2px(Vec(2,MHEIGHT-115.5f)),module,STrig::CV_INPUT));
    addParam(createParam<TrimbotWhite>(mm2px(Vec(2,MHEIGHT-103.5f)),module,STrig::HIGH_PARAM));
    addInput(createInput<SmallPort>(mm2px(Vec(2,MHEIGHT-91.5f)),module,STrig::HIGH_INPUT));
    addParam(createParam<TrimbotWhite>(mm2px(Vec(2,MHEIGHT-79.5f)),module,STrig::LOW_PARAM));
    addInput(createInput<SmallPort>(mm2px(Vec(2,MHEIGHT-67.5f)),module,STrig::LOW_INPUT));
    addOutput(createOutput<SmallPort>(mm2px(Vec(2,MHEIGHT-55.5f)),module,STrig::GATE_OUTPUT));
  }
};


Model *modelSTrig=createModel<STrig,STrigWidget>("STrig");