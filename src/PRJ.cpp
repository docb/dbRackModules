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

  bool setCV=true;

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
      float chnIn=inputs[CHN_INPUT+k].getVoltage()*1.6f;
      if(inputs[CHN_INPUT+k].isConnected()) {
        if(setCV) getParamQuantity(k)->setValue(chnIn);
      }
      if(inputs[CV_INPUT].isConnected()) {
        int channels=inputs[CV_INPUT].getChannels();
        int channel=clamp(static_cast<int>(params[k].getValue()+(setCV?0.f:chnIn)),0,channels-1);
        outputs[k].setVoltage(inputs[CV_INPUT].getVoltage(channel));
      }
    }
  }
  void dataFromJson(json_t *root) override {
    json_t *jSetCV=json_object_get(root,"setCV");
    if(jSetCV) {
      setCV=json_boolean_value(jSetCV);
    }
  }

  json_t *dataToJson() override {
    json_t *root=json_object();
    json_object_set_new(root,"setCV",json_boolean(setCV));
    return root;
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
  void appendContextMenu(Menu* menu) override {
    PRJ *module=dynamic_cast<PRJ *>(this->module);
    assert(module);
    menu->addChild(new MenuSeparator);
    menu->addChild(createBoolPtrMenuItem("Set CV","",&module->setCV));
  }
};

Model* modelPRJ=createModel<PRJ, PRJWidget>("PRJ");
