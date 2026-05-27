#include "dcb.h"
#include <cmath>
#include <algorithm>
#define BUF_SIZE 1024

struct Compressor {
  // Dual-stage envelopes
  float envFastDB = -120.0f;
  float envSlowDB = -120.0f;

  std::vector<float> bufL;
  std::vector<float> bufR;
  unsigned writeIndex = 0;

  float attFastMs=0.004;
  float relFastMs=0.050;
  float attSlowMs=0.020;
  float relSlowMs=0.250;
  float delay= 0.005;

  Compressor() {
    bufL.resize(BUF_SIZE);
    bufR.resize(BUF_SIZE);
  }

  void process(float inL, float inR, float scIn, float& outL, float& outR,
               float thresholdDB, float ratio, float makeupDB, int mode, float scMix, float sampleRate) {

    // Calculate Peak (Scaled for 5V)
    float internalPeak = std::max(std::abs(inL), std::abs(inR)) / 5.0f;
    float sidechainPeak = std::abs(scIn) / 5.0f;

    // Blend internal and sidechain based on the menu ratio
    float peak = (internalPeak * (1.0f - scMix)) + (sidechainPeak * scMix);
    float currentDB = (peak < 0.000001f) ? -120.0f : 20.0f * std::log10(peak);

    // Dual-Stage Envelope (Anti-Pumping)
    float attFastCoef = std::exp(-1.0f / (attFastMs * sampleRate)); // 4ms
    float relFastCoef = std::exp(-1.0f / (relFastMs * sampleRate)); // 50ms
    float attSlowCoef = std::exp(-1.0f / (attSlowMs * sampleRate)); // 20ms
    float relSlowCoef = std::exp(-1.0f / (relSlowMs * sampleRate)); // 250ms

    // Fast Envelope
    if (currentDB > envFastDB) {
      envFastDB = attFastCoef * envFastDB + (1.0f - attFastCoef) * currentDB;
    } else {
      envFastDB = relFastCoef * envFastDB + (1.0f - relFastCoef) * currentDB;
    }

    // Slow Envelope
    if (currentDB > envSlowDB) {
      envSlowDB = attSlowCoef * envSlowDB + (1.0f - attSlowCoef) * currentDB;
    } else {
      envSlowDB = relSlowCoef * envSlowDB + (1.0f - relSlowCoef) * currentDB;
    }

    float finalEnvDB = envFastDB;
    if (mode == 2) {
      finalEnvDB = std::max(envFastDB, envSlowDB);
    } else if (mode == 0) {
      finalEnvDB = envSlowDB;
    }

    float gainReductionDB = 0.0f;
    float actualRatio = (mode == 1 || mode == 2) ? 1000.0f : ratio;

    if (finalEnvDB > thresholdDB) {
      float overshoot = finalEnvDB - thresholdDB;
      gainReductionDB = -(overshoot - (overshoot / actualRatio));
    }

    float totalGainDB = gainReductionDB + makeupDB;
    float linearGain = std::pow(10.0f, totalGainDB / 20.0f);

    unsigned d = std::min(static_cast<int>(delay * sampleRate), BUF_SIZE);
    unsigned readIndexL = (writeIndex - d + BUF_SIZE) % BUF_SIZE;
    unsigned readIndexR = (writeIndex - d + BUF_SIZE) % BUF_SIZE;

    outL = bufL[readIndexL] * linearGain;
    outR = bufR[readIndexR] * linearGain;

    bufL[writeIndex] = inL;
    bufR[writeIndex] = inR;
    writeIndex = (writeIndex + 1) % BUF_SIZE;
  }
};

struct C7 : Module {
  enum ParamId {
    THRESHOLD_PARAM,RATIO_PARAM,MAKE_UP_PARAM,MODE_PARAM,IN_GAIN_PARAM,PARAMS_LEN
  };

  enum InputId {
    L_INPUT, R_INPUT, SC_INPUT, INPUTS_LEN
  };

  enum OutputId {
    L_OUTPUT, R_OUTPUT, OUTPUTS_LEN
  };

  enum LightId {
    LIGHTS_LEN
  };

  Compressor compressor;
  C7() {
    config(PARAMS_LEN, INPUTS_LEN, OUTPUTS_LEN, LIGHTS_LEN);
    configInput(L_INPUT, "Left");
    configInput(R_INPUT, "Right");
    configInput(SC_INPUT, "Side");
    configOutput(L_OUTPUT, "Left");
    configOutput(R_OUTPUT, "Right");
    configParam(RATIO_PARAM, 1.f, 8.f, 2.0f, "Ratio");
    configParam(THRESHOLD_PARAM, -36.f, 6.f, 0.0f, "Threshold", "dB");
    configParam(MAKE_UP_PARAM, 0.f, 12.f, 0.0f, "Makeup", "dB");
    configParam(IN_GAIN_PARAM, 0.f, 4.f, 1.0f, "In-Gain");
    configSwitch(MODE_PARAM,0,2,0,"Mode",{"Compressor","Limiter","Adaptive Limiter"});
    configBypass(L_INPUT,L_OUTPUT);
    configBypass(R_INPUT,R_OUTPUT);
  }

  void process(const ProcessArgs& args) override {
    float inL=inputs[L_INPUT].getVoltage()*params[IN_GAIN_PARAM].getValue();
    float inR=inputs[R_INPUT].getVoltage()*params[IN_GAIN_PARAM].getValue();
    float inSc=inputs[SC_INPUT].getVoltage()*params[IN_GAIN_PARAM].getValue();
    float scMix=inputs[SC_INPUT].isConnected()?1.f:0.f;

    float ratio=params[RATIO_PARAM].getValue();
    float makeup=params[MAKE_UP_PARAM].getValue();
    float threshold=params[THRESHOLD_PARAM].getValue();
    int mode=params[MODE_PARAM].getValue();
    float outL=0.f;
    float outR=0.f;
    compressor.process(inL,inR,inSc, outL,outR,threshold,ratio,makeup,mode,scMix,args.sampleRate);
    outputs[L_OUTPUT].setVoltage(outL);
    outputs[R_OUTPUT].setVoltage(outR);
  }
};

struct C7MenuSliderQuantity : Quantity {
  float *value;
  float min;
  float max;
  C7MenuSliderQuantity(float* v,const float min_, const float max_) : value(v),min(min_),max(max_) {}
  void setValue(float v) override {
    v = clamp(v, getMinValue(), getMaxValue());
    if (value) {
      *value=v;;
    }
  }
  float getValue() override {
    if (value) {
      return *value;
    }
    return 0;
  }

  float getMinValue() override { return min; }
  float getMaxValue() override { return max; }
  float getDefaultValue() override { return 0.f; }
  std::string getLabel() override { return ""; }
};

struct C7MenuSlider : ui::Slider {
  C7MenuSlider(float* value, float min, float max) {
    quantity = new C7MenuSliderQuantity(value,min,max);
    box.size.x = 200.0f;
  }
  virtual ~C7MenuSlider() {
    delete quantity;
  }
};

struct C7MenuItem : MenuItem {
  float min;
  float max;
  float *value;
  C7MenuItem(std::string label,float *value,float min, float max): min(min),max(max),value(value)  {
    this->text = label;
    this->rightText = "▸";
  }

  Menu* createChildMenu() override {
    Menu* menu = new Menu;
    menu->addChild(new C7MenuSlider(value,min,max));
    return menu;
  }
};



struct C7Widget : ModuleWidget {
  C7Widget(C7* module) {
    setModule(module);
    setPanel(createPanel(asset::plugin(pluginInstance, "res/C7.svg")));
    float x=1.9;
    float y=9;
    addParam(createParam<TrimbotWhite>(mm2px(Vec(x, y)), module, C7::THRESHOLD_PARAM));
    y+=11;
    addParam(createParam<TrimbotWhite>(mm2px(Vec(x, y)), module, C7::RATIO_PARAM));
    y+=11;
    addParam(createParam<TrimbotWhite>(mm2px(Vec(x, y)), module, C7::MAKE_UP_PARAM));
    y+=11;
    addParam(createParam<TrimbotWhite>(mm2px(Vec(x, y)), module, C7::IN_GAIN_PARAM));
    auto selectParam=createParam<SelectParam>(mm2px(Vec(1.7, 51)), module, C7::MODE_PARAM);
    selectParam->box.size=Vec(20, 30);
    selectParam->init({"Cmp","Lmt", "AdL"});
    addParam(selectParam);
    addInput(createInput<SmallPort>(mm2px(Vec(x, 68)), module, C7::SC_INPUT));
    addInput(createInput<SmallPort>(mm2px(Vec(x, 80)), module, C7::L_INPUT));
    addInput(createInput<SmallPort>(mm2px(Vec(x, 92)), module, C7::R_INPUT));
    addOutput(createOutput<SmallPort>(mm2px(Vec(x, 104)), module, C7::L_OUTPUT));
    addOutput(createOutput<SmallPort>(mm2px(Vec(x, 116)), module, C7::R_OUTPUT));
  }
  void appendContextMenu(Menu* menu) override {
    C7 *module=dynamic_cast<C7 *>(this->module);
    assert(module);

    menu->addChild(new MenuSeparator);

    menu->addChild(new C7MenuItem("Delay",&module->compressor.delay,0.001,0.021));
    menu->addChild(new C7MenuItem("AttFast (Lim,ALim)",&module->compressor.attFastMs,0.001,0.020));
    menu->addChild(new C7MenuItem("RelFast (Lim,ALim)",&module->compressor.relFastMs,0.001,0.100));
    menu->addChild(new C7MenuItem("AttSlow (Comp,ALim)",&module->compressor.attSlowMs,0.001,0.200));
    menu->addChild(new C7MenuItem("RelSlow (Comp,ALim)",&module->compressor.relSlowMs,0.001,1.000));

  }

};


Model* modelC7=createModel<C7, C7Widget>("C7");
