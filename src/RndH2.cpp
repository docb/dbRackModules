#include "dcb.h"
#include "rnd.h"

struct RndH2 : Module {
  enum ParamIds {
    BI_PARAM,STRENGTH_PARAM,CHANNELS_PARAM,SEED_PARAM,RANGE_PARAM,RANGE_CV_PARAM,OFFSET_PARAM,OFFSET_CV_PARAM,NUM_PARAMS
  };
  enum InputIds {
    CLOCK_INPUT,RST_INPUT,SEED_INPUT,STRENGTH_INPUT,RANGE_INPUT,OFFSET_INPUT,NUM_INPUTS
  };
  enum OutputIds {
    MIN_OUTPUT,WB_OUTPUT,TRI_OUTPUT,UNI_OUTPUT,NUM_OUTPUTS
  };
  enum LightIds {
    NUM_LIGHTS
  };
  RND rnd;
  dsp::SchmittTrigger clockTrigger[16];
  dsp::SchmittTrigger rstTrigger;

  RndH2() {
    config(NUM_PARAMS,NUM_INPUTS,NUM_OUTPUTS,NUM_LIGHTS);
    configInput(SEED_INPUT,"SEED");
    configInput(STRENGTH_INPUT,"STRENGTH");
    configParam(BI_PARAM,0.f,1.f,1.f,"BI-Polar");
    configParam(RANGE_PARAM,0.f,1.f,1.f,"Range");
    configParam(RANGE_CV_PARAM,0.f,1.f,0.f,"Range CV");
    configParam(OFFSET_PARAM,-5.f,5.f,0.f,"Offset");
    configParam(OFFSET_CV_PARAM,0.f,1.f,0.f,"Offset CV");
    configParam(BI_PARAM,0.f,1.f,1.f,"BI-Polar");
    configParam(STRENGTH_PARAM,1.f,20.f,1.f,"Strength");
    configParam(CHANNELS_PARAM,1.f,16.f,8.f,"Polyphonic Channels");
    configParam(SEED_PARAM,0,1,0,"Random Seed");
    configInput(SEED_INPUT,"Random Seed");
    configInput(CLOCK_INPUT,"Clock");
    configInput(RST_INPUT,"Reset");
    configInput(RANGE_INPUT,"Range");
    configInput(OFFSET_INPUT,"Offset");
    configOutput(MIN_OUTPUT,"Min Distribution");
    configOutput(WB_OUTPUT,"Weibull Distribution");
    configOutput(TRI_OUTPUT,"Triangular Distribution");
    configOutput(UNI_OUTPUT,"Uniform Distribution");
    getParamQuantity(CHANNELS_PARAM)->snapEnabled=true;
  }

  void onAdd(const AddEvent &e) override {
    Module::onAdd(e);
    float seedParam=0;
    if(inputs[SEED_INPUT].isConnected())
      seedParam=inputs[SEED_INPUT].getVoltage()/10.f;
    rnd.reset((unsigned long long)(floor((double)seedParam*(double)ULONG_MAX)));
  }

  float modify(float f,int chn) {
    float range=clamp(params[RANGE_PARAM].getValue()+0.1f*inputs[RANGE_INPUT].getPolyVoltage(chn)*params[RANGE_CV_PARAM].getValue(),0.f,1.f);
    float offset=clamp(params[OFFSET_PARAM].getValue()+inputs[OFFSET_INPUT].getPolyVoltage(chn)*params[OFFSET_CV_PARAM].getValue(),-5.f,5.f);
    return f*range+offset;
  }

  void nextMin(float strength,bool bi,int chn) {
    if(strength==1.f) {
      auto out=float(rnd.nextDouble()*10-(bi?5:0));
      outputs[MIN_OUTPUT].setVoltage(modify(out,chn),chn);
    } else {
      double outm=rnd.nextMin((int)strength);
      if(bi) {
        float out=rnd.nextCoin()?(float)outm*5.f:(float)-outm*5.f;
        outputs[MIN_OUTPUT].setVoltage(modify(out,chn),chn);
      } else {
        outputs[MIN_OUTPUT].setVoltage(modify((float)outm*10.f,chn),chn);
      }
    }
  }

  void nextWB(float strength,bool bi,int chn) {
    if(strength==1.f) {
      auto out=float(rnd.nextDouble()*10-(bi?5:0));
      outputs[WB_OUTPUT].setVoltage(modify(out,chn),chn);
    } else {
      double outw=rnd.nextWeibull(strength);
      if(bi) {
        auto out=rnd.nextCoin()?(float)outw*5.f:(float)-outw*5.f;
        outputs[WB_OUTPUT].setVoltage(modify(out,chn),chn);
      } else {
        outputs[WB_OUTPUT].setVoltage(modify((float)outw*10.f,chn),chn);
      }
    }
  }

  void nextTri(float strength,bool bi,int chn) {
    if(strength==1.f) {
      auto out=float(rnd.nextDouble()*10-(bi?5:0));
      outputs[TRI_OUTPUT].setVoltage(modify(out,chn),chn);
    } else {
      double outt=rnd.nextTri((int)strength);
      auto out=(float)outt*10.f-(bi?5.f:0.f);
      outputs[TRI_OUTPUT].setVoltage(modify(out,chn),chn);
    }
  }

  void nextUni(bool bi,int chn) {
    auto out=float(rnd.nextDouble()*10-(bi?5:0));
    outputs[UNI_OUTPUT].setVoltage(modify(out,chn),chn);
  }

  void next(int chn,bool bi=false) {
    float strength=params[STRENGTH_PARAM].getValue();
    strength=clamp(inputs[STRENGTH_INPUT].getNormalPolyVoltage(strength*.5f,chn)*2.f,1.f,20.f);
    if(outputs[MIN_OUTPUT].isConnected())
      nextMin(strength,bi,chn);
    if(outputs[WB_OUTPUT].isConnected())
      nextWB(strength,bi,chn);
    if(outputs[TRI_OUTPUT].isConnected())
      nextTri(strength,bi,chn);
    if(outputs[UNI_OUTPUT].isConnected())
      nextUni(bi,chn);
  }


  void process(const ProcessArgs &args) override {
    float bi=params[BI_PARAM].getValue();
    int channels=params[CHANNELS_PARAM].getValue();
    bool channelParam=false;
    bool clockConnected=inputs[CLOCK_INPUT].isConnected();
    if(clockConnected) {
      int inCh=inputs[CLOCK_INPUT].getChannels();
      if(inCh>1) {
        channels=inCh;
      } else {
        channelParam=true;
      }
    }
    if(rstTrigger.process(inputs[RST_INPUT].getVoltage())) {
      if(inputs[SEED_INPUT].isConnected()) {
        getParamQuantity(SEED_PARAM)->setValue(inputs[SEED_INPUT].getVoltage()*0.1);
      }
      float seedParam=params[SEED_PARAM].getValue();
      seedParam=floorf(seedParam*10000)/10000;
      auto seedInput=(unsigned long long)(floor((double)seedParam*(double)ULONG_MAX));
      INFO("%.8f %lld",seedParam,seedInput);
      rnd.reset(seedInput);
      for(int c=0;c<channels;c++) {
        next(c,bi>0.f);
      }
    }
    if(clockConnected) {
      if(channelParam) {
        if(clockTrigger[0].process(inputs[CLOCK_INPUT].getVoltage())) {
          for(int c=0;c<channels;c++) {
            next(c,bi>0.f);
          }
        }
      } else {
        for(int c=0;c<channels;c++) {
          if(clockTrigger[c].process(inputs[CLOCK_INPUT].getVoltage(c))) {
            next(c,bi>0.f);
          }
        }
      }
    } else {
      for(int k=0;k<channels;k++)
        next(k,true);
    }


    if(outputs[UNI_OUTPUT].isConnected())
      outputs[UNI_OUTPUT].setChannels(channels);
    if(outputs[MIN_OUTPUT].isConnected())
      outputs[MIN_OUTPUT].setChannels(channels);
    if(outputs[WB_OUTPUT].isConnected())
      outputs[WB_OUTPUT].setChannels(channels);
    if(outputs[TRI_OUTPUT].isConnected())
      outputs[TRI_OUTPUT].setChannels(channels);
  }

};


struct RndH2Widget : ModuleWidget {
  RndH2Widget(RndH2 *module) {
    setModule(module);
    setPanel(createPanel(asset::plugin(pluginInstance,"res/RndH2.svg")));
    float x=1.9f;
    float x1=11.9f;
    float y=9;
    addInput(createInput<SmallPort>(mm2px(Vec(x,y)),module,RndH2::CLOCK_INPUT));
    addParam(createParam<TrimbotWhite>(mm2px(Vec(x1,y)),module,RndH2::CHANNELS_PARAM));
    y+=12;
    addInput(createInput<SmallPort>(mm2px(Vec(6.9,y)),module,RndH2::RST_INPUT));
    y+=12;
    addParam(createParam<TrimbotWhite>(mm2px(Vec(x,y)),module,RndH2::SEED_PARAM));
    addInput(createInput<SmallPort>(mm2px(Vec(x1,y)),module,RndH2::SEED_INPUT));
    y+=12;
    addParam(createParam<TrimbotWhite>(mm2px(Vec(x,y)),module,RndH2::STRENGTH_PARAM));
    addInput(createInput<SmallPort>(mm2px(Vec(x1,y)),module,RndH2::STRENGTH_INPUT));
    y+=12;
    auto biPolarButton=createParam<SmallButtonWithLabel>(mm2px(Vec(6.8,y)),module,RndH2::BI_PARAM);
    biPolarButton->setLabel("BiP");
    addParam(biPolarButton);
    y=66;
    addParam(createParam<TrimbotWhite>(mm2px(Vec(x,y)),module,RndH2::RANGE_PARAM));
    addParam(createParam<TrimbotWhite>(mm2px(Vec(x1,y)),module,RndH2::OFFSET_PARAM));
    y+=8;
    addInput(createInput<SmallPort>(mm2px(Vec(x,y)),module,RndH2::RANGE_INPUT));
    addInput(createInput<SmallPort>(mm2px(Vec(x1,y)),module,RndH2::OFFSET_INPUT));
    y+=8;
    addParam(createParam<TrimbotWhite>(mm2px(Vec(x,y)),module,RndH2::RANGE_CV_PARAM));
    addParam(createParam<TrimbotWhite>(mm2px(Vec(x1,y)),module,RndH2::OFFSET_CV_PARAM));

    y=104;
    addOutput(createOutput<SmallPort>(mm2px(Vec(x,y)),module,RndH2::WB_OUTPUT));
    addOutput(createOutput<SmallPort>(mm2px(Vec(x1,y)),module,RndH2::MIN_OUTPUT));
    y+=12;
    addOutput(createOutput<SmallPort>(mm2px(Vec(x,y)),module,RndH2::TRI_OUTPUT));
    addOutput(createOutput<SmallPort>(mm2px(Vec(x1,y)),module,RndH2::UNI_OUTPUT));

  }
};


Model *modelRndH2=createModel<RndH2,RndH2Widget>("RndH2");