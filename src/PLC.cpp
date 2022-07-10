#include "dcb.h"
extern Model *modelFaders;

struct PLC : Module {
  enum ParamId {
    PARAMS_LEN=16
  };
  enum InputId {
    INPUTS_LEN
  };
  enum OutputId {
    CV_OUTPUT,OUTPUTS_LEN
  };
  enum LightId {
    LIGHTS_LEN=16
  };

  int maxChannels=16;
  float min=-10;
  float max=10;
  int dirty=0;
  dsp::ClockDivider divider;
  Module *fadersModule=nullptr;

  PLC() {
    config(PARAMS_LEN,INPUTS_LEN,OUTPUTS_LEN,LIGHTS_LEN);
    for(int k=0;k<16;k++) {
      configParam(k,min,max,0,"chn "+std::to_string(k+1));
    }
    configOutput(CV_OUTPUT,"CV");
    divider.setDivision(64);
  }

  void copyFromFaders(int row) {
    if(fadersModule) {
      for(int k=0;k<16;k++) {
        getParamQuantity(k)->setValue(fadersModule->params[row*16+k].getValue());
      }
    }
  }

  void process(const ProcessArgs &args) override {
    if(leftExpander.module) {
      if(leftExpander.module->model==modelFaders) {
        fadersModule=leftExpander.module;
      } else {
        fadersModule=nullptr;
      }
    }
    if(divider.process()) {
      for(int k=0;k<16;k++) {
        outputs[CV_OUTPUT].setVoltage(k<maxChannels?params[k].getValue():0.f,k);
        lights[k].setBrightness(k<maxChannels?1.f:0.f);
      }
      outputs[CV_OUTPUT].setChannels(maxChannels);
    }
  }
  void fromJson(json_t *root) override {
    min=-10.f;
    max=10.f;
    reconfig();
    Module::fromJson(root);
  }
  json_t* dataToJson() override {
    json_t *root=json_object();
    json_object_set_new(root,"min",json_real(min));
    json_object_set_new(root,"max",json_real(max));
    json_object_set_new(root,"maxChannels",json_integer(maxChannels));
    return root;
  }

  void dataFromJson(json_t *root) override {
    json_t *jMin=json_object_get(root,"min");
    if(jMin) {
      min=json_real_value(jMin);
    }

    json_t *jMax=json_object_get(root,"max");
    if(jMax) {
      max=json_real_value(jMax);
    }
    json_t *jMaxChannels=json_object_get(root,"maxChannels");
    if(jMaxChannels) {
      maxChannels=json_integer_value(jMaxChannels);
    }
    reconfig();
  }

  void reconfig() {
    for(int k=0;k<16;k++) {
      float value = getParamQuantity(k)->getValue();
      if(value>max) value=max;
      if(value<min) value=min;
      configParam(k,min,max,0,"chn "+std::to_string(k+1));
      getParamQuantity(k)->setValue(value);
      dirty=16;
    }

  }

};

struct PLCWidget : ModuleWidget {
  PLCWidget(PLC *module) {
    setModule(module);
    setPanel(createPanel(asset::plugin(pluginInstance,"res/PLC.svg")));

    float y=MHEIGHT-118-6.287;
    float x=1.4;
    for(int k=0;k<16;k++) {
      auto param=createParam<MKnob<PLC>>(mm2px(Vec(x,y)),module,k);
      param->module=module;
      addParam(param);
      addChild(createLight<TinySimpleLight<GreenLight>>(mm2px(Vec(x+6.5,y+4.5)),module,k));
      y+=7;
    }
    addOutput(createOutput<SmallPort>(mm2px(Vec(x,MHEIGHT-5.5-6.287)),module,PLC::CV_OUTPUT));
  }

  void appendContextMenu(Menu *menu) override {
    PLC *module=dynamic_cast<PLC *>(this->module);
    assert(module);
    menu->addChild(new MenuSeparator);
    auto channelSelect=new IntSelectItem(&module->maxChannels,1,PORT_MAX_CHANNELS);
    channelSelect->text="Polyphonic Channels";
    channelSelect->rightText=string::f("%d",module->maxChannels)+"  "+RIGHT_ARROW;
    menu->addChild(channelSelect);
    std::vector<MinMaxRange> ranges={{-10,10},{-5,5},{-3,3},{-1,1},{0,10},{0,5},{0,3},{0,1}};
    auto rangeSelectItem = new RangeSelectItem<PLC>(module,ranges);
    rangeSelectItem->text="Range";
    rangeSelectItem->rightText=string::f("%d/%dV",(int)module->min,(int)module->max)+"  "+RIGHT_ARROW;
    menu->addChild(rangeSelectItem);
  }
  void onHoverKey(const event::HoverKey &e) override {
    if(e.action==GLFW_PRESS) {
      int k=e.key-48;
      if(k>=1 && k<4) {
        auto *plcModule=dynamic_cast<PLC *>(this->module);
        plcModule->copyFromFaders(k-1);
      }
    }
    ModuleWidget::onHoverKey(e);
  }
};


Model *modelPLC=createModel<PLC,PLCWidget>("PLC");