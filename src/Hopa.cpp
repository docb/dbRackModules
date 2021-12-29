#include "dcb.h"
#include <Gamma/Delay.h>
#include <Gamma/Filter.h>

#include <cmath>
//template <typename T> int sgn(T val) {
//    return (T(0) < val) - (val < T(0));
//}

struct Hopa : Module {
  enum ParamIds {
    SX_PARAM,SY_PARAM,A_PARAM,B_PARAM,C_PARAM,OFS_PARAM,NUM_PARAMS
  };
  enum InputIds {
    CLOCK_INPUT,RST_INPUT,NUM_INPUTS
  };
  enum OutputIds {
    X_OUTPUT,Y_OUTPUT,NUM_OUTPUTS
  };
  enum LightIds {
    NUM_LIGHTS
  };
  double x=0.5;
  double y=0.5;
  gam::BlockDC<> blockDC;
  dsp::SchmittTrigger clockTrigger;
  dsp::SchmittTrigger rstTrigger;
  bool onReset = false;
  Hopa() {
    config(NUM_PARAMS,NUM_INPUTS,NUM_OUTPUTS,NUM_LIGHTS);
    configParam(SX_PARAM,0.f,1.f,0.5f,"SX","",0,1);
    configParam(SY_PARAM,0.f,1.f,0.5f,"SY","",0,1);
    configParam(A_PARAM,0.f,1.f,0.001f,"A","",0,1);
    configParam(B_PARAM,0.f,1.f,0.002f,"B","",0,1);
    configParam(C_PARAM,0.f,1.f,0.009f,"C","",0,1);
    configParam(OFS_PARAM,0.f,100.f,0.0f,"C","s");
    getParamQuantity(OFS_PARAM)->randomizeEnabled = false;
    configInput(CLOCK_INPUT,"Clock");
    configInput(RST_INPUT,"Reset");
    configOutput(X_OUTPUT,"X");
    configOutput(Y_OUTPUT,"Y");
  }

  void nextHopa() {
    double x0=x;
    float a=params[A_PARAM].getValue();
    float b=params[B_PARAM].getValue();
    float c=params[C_PARAM].getValue();
    x=y-sgn(x0)*sqrt(fabs(b*x0-c));
    y=a-x0;

  }

  void next(bool rt=false) {
    nextHopa();
    if(rt) {
      float xf = blockDC((float)x);
      float yf = blockDC((float)y);
      outputs[X_OUTPUT].setVoltage((float)xf);
      outputs[Y_OUTPUT].setVoltage((float)yf);
    } else {
      outputs[X_OUTPUT].setVoltage((float)x);
      outputs[Y_OUTPUT].setVoltage((float)y);
    }

  }

  void reset(int offset) {
    x=params[SX_PARAM].getValue();
    y=params[SY_PARAM].getValue();
    for(int k=0;k<offset;k++) nextHopa();
    INFO("reset %f %f",x,y);
  }

  void process(const ProcessArgs &args) override {
    if(rstTrigger.process(inputs[RST_INPUT].getVoltage()) || onReset) {
      onReset = false;
      int offset = std::floor(params[OFS_PARAM].getValue()*args.sampleRate);
      reset(offset);
    }
    if(inputs[CLOCK_INPUT].isConnected()) {
      if(clockTrigger.process(inputs[CLOCK_INPUT].getVoltage())) {
        next();
      }
    } else {
      next(true);
    }
  }
  void onRandomize(const RandomizeEvent& e) override {
    Module::onRandomize(e);
    onReset = true;
  }
};


struct HopaWidget : ModuleWidget {
  HopaWidget(Hopa *module) {
    setModule(module);
    setPanel(APP->window->loadSvg(asset::plugin(pluginInstance,"res/Hopa.svg")));

    addChild(createWidget<ScrewSilver>(Vec(RACK_GRID_WIDTH,0)));
    addChild(createWidget<ScrewSilver>(Vec(box.size.x-2*RACK_GRID_WIDTH,0)));
    addChild(createWidget<ScrewSilver>(Vec(RACK_GRID_WIDTH,RACK_GRID_HEIGHT-RACK_GRID_WIDTH)));
    addChild(createWidget<ScrewSilver>(Vec(box.size.x-2*RACK_GRID_WIDTH,RACK_GRID_HEIGHT-RACK_GRID_WIDTH)));

    float xpos = 1.9f;
    addInput(createInput<SmallPort>(mm2px(Vec(xpos,MHEIGHT-115.f)),module,Hopa::CLOCK_INPUT));
    addInput(createInput<SmallPort>(mm2px(Vec(xpos,MHEIGHT-103.f)),module,Hopa::RST_INPUT));
    addParam(createParam<TrimbotWhite>(mm2px(Vec(2,MHEIGHT-91.5f)),module,Hopa::SX_PARAM));
    addParam(createParam<TrimbotWhite>(mm2px(Vec(2,MHEIGHT-79.0f)),module,Hopa::SY_PARAM));
    addParam(createParam<TrimbotWhite>(mm2px(Vec(2,MHEIGHT-67.f)),module,Hopa::A_PARAM));
    addParam(createParam<TrimbotWhite>(mm2px(Vec(2,MHEIGHT-55.5f)),module,Hopa::B_PARAM));
    addParam(createParam<TrimbotWhite>(mm2px(Vec(2,MHEIGHT-43.5f)),module,Hopa::C_PARAM));
    addParam(createParam<TrimbotWhite>(mm2px(Vec(2,MHEIGHT-31.f)),module,Hopa::OFS_PARAM));
    addOutput(createOutput<SmallPort>(mm2px(Vec(xpos,MHEIGHT-19.f)),module,Hopa::X_OUTPUT));
  }
};


Model *modelHopa=createModel<Hopa,HopaWidget>("Hopa");
