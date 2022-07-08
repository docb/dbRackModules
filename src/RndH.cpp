#include "dcb.h"
#include "rnd.h"
#include <climits>


struct RndH : Module {
  enum ParamIds {
    BI_PARAM,STRENGTH_PARAM,CHANNELS_PARAM,NUM_PARAMS
  };
  enum InputIds {
    CLOCK_INPUT,RST_INPUT,SEED_INPUT,STRENGTH_INPUT,NUM_INPUTS
  };
  enum OutputIds {
    MIN_OUTPUT,WB_OUTPUT,TRI_OUTPUT,NUM_OUTPUTS
  };
  enum LightIds {
    NUM_LIGHTS
  };
  RND rnd;
  dsp::SchmittTrigger clockTrigger;
  dsp::SchmittTrigger rstTrigger;
  bool roundSeed = false;

  RndH() {
    config(NUM_PARAMS,NUM_INPUTS,NUM_OUTPUTS,NUM_LIGHTS);
    configInput(SEED_INPUT,"SEED");
    configInput(STRENGTH_INPUT,"STRENGTH");
    configParam(BI_PARAM,0.f,1.f,1.f,"BI-Polar");
    configParam(STRENGTH_PARAM,1.f,20.f,1.f,"Strength");
    configParam(CHANNELS_PARAM,1.f,16.f,8.f,"Polyphonic Channels");
    configInput(SEED_INPUT,"Random Seed");
    configInput(CLOCK_INPUT,"Clock");
    configInput(RST_INPUT,"Reset");
    configOutput(MIN_OUTPUT,"Min Distribution");
    configOutput(WB_OUTPUT,"Weibull Distribution");
    configOutput(TRI_OUTPUT,"Triangular Distribution");
    getParamQuantity(CHANNELS_PARAM)->snapEnabled=true;
  }

  void onAdd(const AddEvent &e) override {
    Module::onAdd(e);
    float seedParam=0;
    if(inputs[SEED_INPUT].isConnected())
      seedParam=inputs[SEED_INPUT].getVoltage()/10.f;
    rnd.reset((unsigned long long)(floor((double)seedParam*(double)ULONG_MAX)));
  }

  void nextMin(float strength,bool bi,int chn) {
    if(strength==1.f) {
      auto out=float(rnd.nextDouble()*10-(bi?5:0));
      outputs[MIN_OUTPUT].setVoltage(out,chn);
    } else {
      double outm=rnd.nextMin((int)strength);
      if(bi) {
        float out=rnd.nextCoin()?(float)outm*5.f:(float)-outm*5.f;
        outputs[MIN_OUTPUT].setVoltage(out,chn);
      } else {
        outputs[MIN_OUTPUT].setVoltage((float)outm*10.f,chn);
      }
    }
  }

  void nextWB(float strength,bool bi,int chn) {
    if(strength==1.f) {
      auto out=float(rnd.nextDouble()*10-(bi?5:0));
      outputs[WB_OUTPUT].setVoltage(out,chn);
    } else {
      double outw=rnd.nextWeibull(strength);
      if(bi) {
        auto out=rnd.nextCoin()?(float)outw*5.f:(float)-outw*5.f;
        outputs[WB_OUTPUT].setVoltage(out,chn);
      } else {
        outputs[WB_OUTPUT].setVoltage((float)outw*10.f,chn);
      }
    }
  }

  void nextTri(float strength,bool bi,int chn) {
    if(strength==1.f) {
      auto out=float(rnd.nextDouble()*10-(bi?5:0));
      outputs[TRI_OUTPUT].setVoltage(out,chn);
    } else {
      double outt=rnd.nextTri((int)strength);
      auto out=(float)outt*10.f-(bi?5.f:0.f);
      outputs[TRI_OUTPUT].setVoltage(out,chn);
    }
  }

  void next(bool bi=false) {
    float strength=params[STRENGTH_PARAM].getValue();
    int channels=(int)params[CHANNELS_PARAM].getValue();
    for(int k=0;k<channels;k++) {
      strength=clamp(inputs[STRENGTH_INPUT].getNormalPolyVoltage(strength*.5f,k)*2.f,1.f,20.f);
      if(outputs[MIN_OUTPUT].isConnected())
        nextMin(strength,bi,k);
      if(outputs[WB_OUTPUT].isConnected())
        nextWB(strength,bi,k);
      if(outputs[TRI_OUTPUT].isConnected())
        nextTri(strength,bi,k);
    }

    if(outputs[MIN_OUTPUT].isConnected())
      outputs[MIN_OUTPUT].setChannels(channels);
    if(outputs[WB_OUTPUT].isConnected())
      outputs[WB_OUTPUT].setChannels(channels);
    if(outputs[TRI_OUTPUT].isConnected())
      outputs[TRI_OUTPUT].setChannels(channels);
  }

  void process(const ProcessArgs &args) override {
    float bi=params[BI_PARAM].getValue();

    if(rstTrigger.process(inputs[RST_INPUT].getVoltage())) {
      float seedParam=0;
      if(inputs[SEED_INPUT].isConnected()) {
        seedParam=inputs[SEED_INPUT].getVoltage();
        if(roundSeed) seedParam = floorf(seedParam*10000)/10000;
        seedParam/=10.f;
      }
      auto seedInput = (unsigned long long)(floor((double)seedParam*(double)ULONG_MAX));
      INFO("%.8f %lld",seedParam,seedInput);
      rnd.reset(seedInput);
      next(bi>0.f);
    }
    if(inputs[CLOCK_INPUT].isConnected()) {
      if(clockTrigger.process(inputs[CLOCK_INPUT].getVoltage())) {
        next(bi>0.f);
      }
    } else {
      next(true);
    }
  }

  json_t *dataToJson() override {
    json_t *data=json_object();
    json_object_set_new(data,"roundSeed",json_boolean(roundSeed));
    return data;
  }
  void dataFromJson(json_t *rootJ) override {
    json_t *jRoundSeed = json_object_get(rootJ,"roundSeed");
    if(jRoundSeed!=nullptr) roundSeed = json_boolean_value(jRoundSeed);
  }

};


struct RndHWidget : ModuleWidget {
  explicit RndHWidget(RndH *module) {
    setModule(module);
    setPanel(APP->window->loadSvg(asset::plugin(pluginInstance,"res/RndH.svg")));

    addChild(createWidget<ScrewSilver>(Vec(RACK_GRID_WIDTH,0)));
    addChild(createWidget<ScrewSilver>(Vec(box.size.x-2*RACK_GRID_WIDTH,0)));
    addChild(createWidget<ScrewSilver>(Vec(RACK_GRID_WIDTH,RACK_GRID_HEIGHT-RACK_GRID_WIDTH)));
    addChild(createWidget<ScrewSilver>(Vec(box.size.x-2*RACK_GRID_WIDTH,RACK_GRID_HEIGHT-RACK_GRID_WIDTH)));
    float xpos=1.9f;
    addInput(createInput<SmallPort>(mm2px(Vec(xpos,MHEIGHT-115.f)),module,RndH::CLOCK_INPUT));
    addInput(createInput<SmallPort>(mm2px(Vec(xpos,MHEIGHT-103.f)),module,RndH::RST_INPUT));
    addInput(createInput<SmallPort>(mm2px(Vec(xpos,MHEIGHT-91.5f)),module,RndH::SEED_INPUT));
    addParam(createParam<TrimbotWhite>(mm2px(Vec(2,MHEIGHT-79.0f)),module,RndH::STRENGTH_PARAM));
    addInput(createInput<SmallPort>(mm2px(Vec(xpos,MHEIGHT-71.5f)),module,RndH::STRENGTH_INPUT));
    auto biPolarButton=createParam<SmallButtonWithLabel>(mm2px(Vec(1.5,MHEIGHT-63.f)),module,RndH::BI_PARAM);
    biPolarButton->setLabel("BiP");
    addParam(biPolarButton);
    addOutput(createOutput<SmallPort>(mm2px(Vec(xpos,MHEIGHT-55.f)),module,RndH::MIN_OUTPUT));
    addOutput(createOutput<SmallPort>(mm2px(Vec(xpos,MHEIGHT-43.f)),module,RndH::WB_OUTPUT));
    addOutput(createOutput<SmallPort>(mm2px(Vec(xpos,MHEIGHT-31.f)),module,RndH::TRI_OUTPUT));

    addParam(createParam<TrimbotWhite>(mm2px(Vec(2,MHEIGHT-19.f)),module,RndH::CHANNELS_PARAM));

  }

  void appendContextMenu(Menu* menu) override {
    RndH *module=dynamic_cast<RndH *>(this->module);
    assert(module);

    menu->addChild(new MenuSeparator);

    menu->addChild(createBoolPtrMenuItem("Round Seed","",&module->roundSeed));
  }

};


Model *modelRndH=createModel<RndH,RndHWidget>("RndH");