#include "dcb.h"
using simd::float_4;

struct AUX : Module {
  enum ParamId {
    LEVEL_PARAM=16,PARAMS_LEN
  };
  enum InputId {
    L_INPUT,R_INPUT,LVL_INPUT,MAIN_INPUT,INPUTS_LEN
  };
  enum OutputId {
    L_OUTPUT,R_OUTPUT,OUTPUTS_LEN
  };
  enum LightId {
    VU_LIGHTS_L,VU_LIGHTS_R=8,LIGHTS_LEN=16
  };
  dsp::VuMeter2 vuMeterL;
  dsp::VuMeter2 vuMeterR;
  dsp::ClockDivider vuDivider;
  dsp::ClockDivider lightDivider;
  dsp::SlewLimiter slewLimiter[16];
  dsp::SlewLimiter mainSlewLimiter;

  float slew=0.04;
  AUX() {
    config(PARAMS_LEN,INPUTS_LEN,OUTPUTS_LEN,LIGHTS_LEN);
    for(int k=0;k<16;k++) {
      configParam(k,0,1,1,"chn "+std::to_string(k+1));
    }
    configParam(LEVEL_PARAM,0,1,1,"Main Level","%",0,100);
    configInput(L_INPUT,"Left");
    configInput(R_INPUT,"Right");
    configOutput(L_OUTPUT,"Left");
    configOutput(R_OUTPUT,"Right");
    configInput(LVL_INPUT,"Channel Level");
    configInput(MAIN_INPUT,"Main Level");
    vuMeterL.lambda = 1 / 0.1f;
    vuMeterR.lambda = 1 / 0.1f;
    vuDivider.setDivision(16);
    lightDivider.setDivision(512);
  }

  void process(const ProcessArgs &args) override {
    float outL=0.f;
    float outR=0.f;
    int channels=std::max(inputs[L_INPUT].getChannels(),inputs[R_INPUT].getChannels());

    for(int k=0;k<channels;k++) {
      float cLvl=params[k].getValue();
      if(inputs[LVL_INPUT].isConnected()) {
        float sl=(slew+0.001f)*1000.f;
        slewLimiter[k].setRiseFall(1.f/sl,1.f/sl);
        float lvl=slewLimiter[k].process(1.f,clamp(inputs[LVL_INPUT].getVoltage(k),0.f,10.f))*0.1f;
        cLvl*=lvl;
      }
      outL+=cLvl*inputs[L_INPUT].getVoltage(k);
      outR+=cLvl*inputs[R_INPUT].getVoltage(k);
    }
    float lvl=params[LEVEL_PARAM].getValue();
    if(inputs[MAIN_INPUT].isConnected()) {
      float sl=(slew+0.001f)*1000.f;
      mainSlewLimiter.setRiseFall(1.f/sl,1.f/sl);
      float mainLvl=mainSlewLimiter.process(1.f,clamp(inputs[MAIN_INPUT].getVoltage(),0.f,10.f))*0.1f;
      lvl*=mainLvl;
    }
    outL*=lvl;
    outR*=lvl;
    outputs[L_OUTPUT].setVoltage(outL);
    outputs[R_OUTPUT].setVoltage(outR);
    if(vuDivider.process()) {
      vuMeterL.process(args.sampleTime*vuDivider.getDivision(),outL/10.f);
      vuMeterR.process(args.sampleTime*vuDivider.getDivision(),outR/10.f);
    }

    // Set channel lights infrequently
    if(lightDivider.process()) {
      lights[VU_LIGHTS_L+0].setBrightness(vuMeterL.getBrightness(0,0));
      lights[VU_LIGHTS_L+1].setBrightness(vuMeterL.getBrightness(-3,0));
      lights[VU_LIGHTS_L+2].setBrightness(vuMeterL.getBrightness(-6,-3));
      lights[VU_LIGHTS_L+3].setBrightness(vuMeterL.getBrightness(-12,-6));
      lights[VU_LIGHTS_L+4].setBrightness(vuMeterL.getBrightness(-24,-12));
      lights[VU_LIGHTS_L+5].setBrightness(vuMeterL.getBrightness(-36,-24));
      lights[VU_LIGHTS_L+6].setBrightness(vuMeterL.getBrightness(-48,-36));
      lights[VU_LIGHTS_L+7].setBrightness(vuMeterL.getBrightness(-60,-48));
      lights[VU_LIGHTS_R+0].setBrightness(vuMeterR.getBrightness(0,0));
      lights[VU_LIGHTS_R+1].setBrightness(vuMeterR.getBrightness(-3,0));
      lights[VU_LIGHTS_R+2].setBrightness(vuMeterR.getBrightness(-6,-3));
      lights[VU_LIGHTS_R+3].setBrightness(vuMeterR.getBrightness(-12,-6));
      lights[VU_LIGHTS_R+4].setBrightness(vuMeterR.getBrightness(-24,-12));
      lights[VU_LIGHTS_R+5].setBrightness(vuMeterR.getBrightness(-36,-24));
      lights[VU_LIGHTS_R+6].setBrightness(vuMeterR.getBrightness(-48,-36));
      lights[VU_LIGHTS_R+7].setBrightness(vuMeterR.getBrightness(-60,-48));
    }
  }

  json_t *dataToJson() override {
    json_t *data=json_object();
    json_object_set_new(data,"slew",json_real(slew));
    return data;
  }

  void dataFromJson(json_t *rootJ) override {
    json_t *jSlew = json_object_get(rootJ,"slew");
    if(jSlew!=nullptr) slew = json_real_value(jSlew);
  }

};


struct AUXWidget : ModuleWidget {
  AUXWidget(AUX *module) {
    setModule(module);
    setPanel(createPanel(asset::plugin(pluginInstance,"res/AX.svg")));

    addChild(createWidget<ScrewSilver>(Vec(0, 0)));
    addChild(createWidget<ScrewSilver>(Vec(box.size.x - 15, 0)));
    addChild(createWidget<ScrewSilver>(Vec(0, 365)));
    addChild(createWidget<ScrewSilver>(Vec(box.size.x - 15, 365)));
    float y=9;
    float x=6.5f;
    float x1=17;
    for(int k=0;k<16;k++) {
      addParam(createParam<TrimbotWhite>(mm2px(Vec(x,y)),module,k));
      y+=7;
    }

    addInput(createInput<SmallPort>(mm2px(Vec(x1,9)),module,AUX::LVL_INPUT));
    addParam(createParam<TrimbotWhite>(mm2px(Vec(x1,57)),module,AUX::LEVEL_PARAM));
    addInput(createInput<SmallPort>(mm2px(Vec(x1,65)),module,AUX::MAIN_INPUT));
    addInput(createInput<SmallPort>(mm2px(Vec(x1,86)),module,AUX::L_INPUT));
    addInput(createInput<SmallPort>(mm2px(Vec(x1,93)),module,AUX::R_INPUT));
    addOutput(createOutput<SmallPort>(mm2px(Vec(x1,107)),module,AUX::L_OUTPUT));
    addOutput(createOutput<SmallPort>(mm2px(Vec(x1,114)),module,AUX::R_OUTPUT));
    float xL=18;
    float xR=20;
    y=35;
    float yh=2;
    addChild(createLight<SmallSimpleLight<RedLight>>(mm2px(Vec(xL,y)),module,AUX::VU_LIGHTS_L+0));
    addChild(createLight<SmallSimpleLight<RedLight>>(mm2px(Vec(xR,y)),module,AUX::VU_LIGHTS_R+0));
    y+=yh;
    addChild(createLight<SmallSimpleLight<YellowLight>>(mm2px(Vec(xL,y)),module,AUX::VU_LIGHTS_L+1));
    addChild(createLight<SmallSimpleLight<YellowLight>>(mm2px(Vec(xR,y)),module,AUX::VU_LIGHTS_R+1));
    y+=yh;
    addChild(createLight<SmallSimpleLight<GreenLight>>(mm2px(Vec(xL,y)),module,AUX::VU_LIGHTS_L+2));
    addChild(createLight<SmallSimpleLight<GreenLight>>(mm2px(Vec(xR,y)),module,AUX::VU_LIGHTS_R+2));
    y+=yh;
    addChild(createLight<SmallSimpleLight<GreenLight>>(mm2px(Vec(xL,y)),module,AUX::VU_LIGHTS_L+3));
    addChild(createLight<SmallSimpleLight<GreenLight>>(mm2px(Vec(xR,y)),module,AUX::VU_LIGHTS_R+3));
    y+=yh;
    addChild(createLight<SmallSimpleLight<GreenLight>>(mm2px(Vec(xL,y)),module,AUX::VU_LIGHTS_L+4));
    addChild(createLight<SmallSimpleLight<GreenLight>>(mm2px(Vec(xR,y)),module,AUX::VU_LIGHTS_R+4));
    y+=yh;
    addChild(createLight<SmallSimpleLight<GreenLight>>(mm2px(Vec(xL,y)),module,AUX::VU_LIGHTS_L+5));
    addChild(createLight<SmallSimpleLight<GreenLight>>(mm2px(Vec(xR,y)),module,AUX::VU_LIGHTS_R+5));
    y+=yh;
    addChild(createLight<SmallSimpleLight<GreenLight>>(mm2px(Vec(xL,y)),module,AUX::VU_LIGHTS_L+6));
    addChild(createLight<SmallSimpleLight<GreenLight>>(mm2px(Vec(xR,y)),module,AUX::VU_LIGHTS_R+6));
    y+=yh;
    addChild(createLight<SmallSimpleLight<GreenLight>>(mm2px(Vec(xL,y)),module,AUX::VU_LIGHTS_L+7));
    addChild(createLight<SmallSimpleLight<GreenLight>>(mm2px(Vec(xR,y)),module,AUX::VU_LIGHTS_R+7));
  }

  void appendContextMenu(Menu* menu) override {
    AUX *module=dynamic_cast<AUX *>(this->module);
    assert(module);

    menu->addChild(new MenuSeparator);

    menu->addChild(new SlewMenuItem<AUX>(module));
  }

};


Model *modelAUX=createModel<AUX,AUXWidget>("AUX");