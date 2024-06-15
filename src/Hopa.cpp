#include "dcb.h"

#include <cmath>

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
  float a=0.f;
  float b=0.f;
  float c=0.f;
  double dx=0.5;
  double dy=0.5;
  DCBlocker<float> blockDC;
  dsp::SchmittTrigger clockTrigger;
  dsp::SchmittTrigger rstTrigger;
  dsp::PulseGenerator rstPulse;
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
    x=y-sgn(x0)*sqrt(fabs(b*x0-c));
    y=a-x0;

  }

  void next(bool rt=false) {
    nextHopa();
    if(rt) {
      float xf = blockDC.process(float(x));
      float yf = blockDC.process(float(y));
      outputs[X_OUTPUT].setVoltage(xf);
      outputs[Y_OUTPUT].setVoltage(yf);
    } else {
      outputs[X_OUTPUT].setVoltage(float(x));
      outputs[Y_OUTPUT].setVoltage(float(y));
    }

  }
  void nextReset() {
    dx=params[SX_PARAM].getValue();
    dy=params[SY_PARAM].getValue();
    a=params[A_PARAM].getValue();
    b=params[B_PARAM].getValue();
    c=params[C_PARAM].getValue();
    int offset = std::floor(params[OFS_PARAM].getValue()*APP->engine->getSampleRate());
    for(int k=0;k<offset;k++) {
      double x0=dx;
      dx=dy-sgn(x0)*sqrt(fabs(b*x0-c));
      dy=a-x0;
    }
    INFO("next reset %f %f",dx,dy);
  }
  void reset() {
    //dx=params[SX_PARAM].getValue();
    //dy=params[SY_PARAM].getValue();
    //for(int k=0;k<offset;k++) nextHopa();
    x=dx;
    y=dy;
    INFO("reset %f %f",x,y);
  }

  void process(const ProcessArgs &args) override {
    a=params[A_PARAM].getValue();
    b=params[B_PARAM].getValue();
    c=params[C_PARAM].getValue();
    if(rstTrigger.process(inputs[RST_INPUT].getVoltage()) || onReset) {
      onReset = false;
      rstPulse.trigger(0.001f);
      reset();
    }
    bool rstGate=rstPulse.process(args.sampleTime);
    if(inputs[CLOCK_INPUT].isConnected()) {
      if(clockTrigger.process(inputs[CLOCK_INPUT].getVoltage())&&!rstGate) {
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

struct HopaUpdateKnob : TrimbotWhite {
  Hopa *module=nullptr;
  void onChange(const ChangeEvent &e) override {
    if(module) {
      module->nextReset();
    }
    SvgKnob::onChange(e);
  }
};

struct HopaWidget : ModuleWidget {
  HopaWidget(Hopa *module) {
    setModule(module);
    setPanel(APP->window->loadSvg(asset::plugin(pluginInstance,"res/Hopa.svg")));

    float xpos = 1.9f;
    addInput(createInput<SmallPort>(mm2px(Vec(xpos,9)),module,Hopa::CLOCK_INPUT));
    addInput(createInput<SmallPort>(mm2px(Vec(xpos,21)),module,Hopa::RST_INPUT));
    auto sxParam=createParam<HopaUpdateKnob>(mm2px(Vec(2,38)),module,Hopa::SX_PARAM);
    sxParam->module=module;
    addParam(sxParam);
    auto syParam=createParam<HopaUpdateKnob>(mm2px(Vec(2,50)),module,Hopa::SY_PARAM);
    syParam->module=module;
    addParam(syParam);
    addParam(createParam<TrimbotWhite>(mm2px(Vec(2,62)),module,Hopa::A_PARAM));
    addParam(createParam<TrimbotWhite>(mm2px(Vec(2,74)),module,Hopa::B_PARAM));
    addParam(createParam<TrimbotWhite>(mm2px(Vec(2,86)),module,Hopa::C_PARAM));
    auto ofsParam=createParam<HopaUpdateKnob>(mm2px(Vec(2,98)),module,Hopa::OFS_PARAM);
    ofsParam->module=module;
    addParam(ofsParam);
    addOutput(createOutput<SmallPort>(mm2px(Vec(xpos,116)),module,Hopa::X_OUTPUT));
  }
};


Model *modelHopa=createModel<Hopa,HopaWidget>("Hopa");
