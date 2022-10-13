#include "dcb.h"


using simd::float_4;

struct Ratio : Module {
  enum ParamId {
    RATIO_PARAM,FINE_PARAM,INV_PARAM,PARAMS_LEN
  };
  enum InputId {
    VOCT_INPUT,RATIO_INPUT,FINE_INPUT,INPUTS_LEN
  };
  enum OutputId {
    VOCT_OUTPUT,OUTPUTS_LEN
  };
  enum LightId {
    LIGHTS_LEN
  };
  dsp::ClockDivider divider;

  Ratio() {
    config(PARAMS_LEN,INPUTS_LEN,OUTPUTS_LEN,LIGHTS_LEN);
    configButton(INV_PARAM,"1/x");
    configParam(RATIO_PARAM,1,32,1,"Ratio");
    getParamQuantity(RATIO_PARAM)->snapEnabled=true;
    configParam(FINE_PARAM,-1,1,0,"Fine");
    configInput(VOCT_INPUT,"V/Oct");
    configInput(RATIO_INPUT,"Ratio");
    configInput(VOCT_INPUT,"Fine");
    configOutput(VOCT_OUTPUT,"V/Oct");
    divider.setDivision(32);
  }

  void process(const ProcessArgs &args) override {
    int channels=std::max(inputs[VOCT_INPUT].getChannels(),1);
    if(divider.process()) {
      if(inputs[RATIO_INPUT].isConnected()) {
        float _r=clamp(inputs[RATIO_INPUT].getVoltage(),-10.f,10.f);
        float r=std::abs(_r);
        getParamQuantity(RATIO_PARAM)->setValue(r*3.2f);
        if(_r<0)
          getParamQuantity(INV_PARAM)->setValue(1.f);
        else
          getParamQuantity(INV_PARAM)->setValue(0.f);
      }
      if(inputs[FINE_INPUT].isConnected()) {
        float f=clamp(inputs[FINE_INPUT].getVoltage(),-10.f,10.f);
        getParamQuantity(FINE_PARAM)->setValue(f*0.1);
      }
    }
    float ratio=params[RATIO_PARAM].getValue();
    float fine=params[FINE_PARAM].getValue();
    if(ratio<=1) {
      fine=clamp(fine,0.f,1.f);
    }
    ratio+=fine;
    if(params[INV_PARAM].getValue()>0) {
      ratio=1.f/ratio;
    }
    for(int c=0;c<channels;c+=4) {
      float_4 in=inputs[VOCT_INPUT].getVoltageSimd<float_4>(c);
      outputs[VOCT_OUTPUT].setVoltageSimd(in+log2f(ratio),c);
    }
    outputs[VOCT_OUTPUT].setChannels(channels);
  }
};


struct RatioWidget : ModuleWidget {
  RatioWidget(Ratio *module) {
    setModule(module);
    setPanel(createPanel(asset::plugin(pluginInstance,"res/Ratio.svg")));
    float x=1.9;
    float y=9;
    addParam(createParam<TrimbotWhite>(mm2px(Vec(x,y)),module,Ratio::RATIO_PARAM));
    addInput(createInput<SmallPort>(mm2px(Vec(x,y+8)),module,Ratio::RATIO_INPUT));
    y=26;
    auto invParam=createParam<SmallButtonWithLabel>(mm2px(Vec(1.5,y)),module,Ratio::INV_PARAM);
    invParam->setLabel("1/x");
    addParam(invParam);


    y=38;
    addParam(createParam<TrimbotWhite>(mm2px(Vec(x,y)),module,Ratio::FINE_PARAM));
    addInput(createInput<SmallPort>(mm2px(Vec(x,y+8)),module,Ratio::FINE_INPUT));
    y=104;
    addInput(createInput<SmallPort>(mm2px(Vec(x,y)),module,Ratio::VOCT_INPUT));
    y+=12;
    addOutput(createOutput<SmallPort>(mm2px(Vec(x,y)),module,Ratio::VOCT_OUTPUT));
  }
};


Model *modelRatio=createModel<Ratio,RatioWidget>("Ratio");