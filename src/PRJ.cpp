#include "dcb.h"

struct PRJ : Module {
  enum ParamId {
    PARAMS_LEN=4
  };

  enum InputId {
    CV_INPUT, CHN_INPUT, INPUTS_LEN=CHN_INPUT+4
  };

  enum OutputId {
    OUTPUTS_LEN=4
  };

  enum LightId {
    LIGHTS_LEN
  };

  PRJ() {
    config(PARAMS_LEN, INPUTS_LEN, OUTPUTS_LEN, LIGHTS_LEN);
    configInput(CV_INPUT, "Poly");
    for(int k=0; k<4; k++) {
      std::string nr=std::to_string(k+1);
      configParam(k, 0, 15, 0, "Chn "+nr);
      getParamQuantity(k)->snapEnabled=true;
      configInput(CHN_INPUT+k, "Chn "+nr);
      configOutput(k, "CV "+nr);
    }
  }

  void process(const ProcessArgs& args) override {
    for(int k=0; k<4; k++) {
      if(inputs[CHN_INPUT+k].isConnected()) {
        getParamQuantity(k)->setValue(inputs[CHN_INPUT+k].getVoltage()*1.6f);
      }
      if(inputs[CV_INPUT].isConnected()) {
        int channel=static_cast<int>(params[k].getValue());
        int channels=inputs[CV_INPUT].getChannels();
        if(channel<channels) {
          outputs[k].setVoltage(inputs[CV_INPUT].getVoltage(channel));
        } else {
          outputs[k].setVoltage(0.f);
        }
      }
    }
  }
};

struct PRJWidget : ModuleWidget {
  PRJWidget(PRJ* module) {
    setModule(module);
    setPanel(createPanel(asset::plugin(pluginInstance, "res/PRJ.svg")));
    float x=1.9;
    float y=9;
    float y2=95;
    for(int k=0; k<4; k++) {
      addParam(createParam<TrimbotWhite>(mm2px(Vec(x, y)), module, k));
      addInput(createInput<SmallPort>(mm2px(Vec(x, y+7)), module, PRJ::CHN_INPUT+k));
      addOutput(createOutput<SmallPort>(mm2px(Vec(x, y2)), module, k));
      y+=19;
      y2+=7;
    }
    addInput(createInput<SmallPort>(mm2px(Vec(x, 84)), module, PRJ::CV_INPUT));
  }
};

Model* modelPRJ=createModel<PRJ, PRJWidget>("PRJ");
