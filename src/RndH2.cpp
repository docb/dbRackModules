#include "dcb.h"
#include "rnd.h"

struct UniH {
  float val[16]={};
  void next(RND &rnd,bool bi,int chn) {
    val[chn]=float(rnd.nextDouble()*10-(bi?5:0));
  }
  float get(int chn) {
    return val[chn];
  }
};

struct MinH {
  float val[16]={};
  void next(RND &rnd,bool bi,int chn,float strength) {
    if(strength==1.f) {
      val[chn]=float(rnd.nextDouble()*10-(bi?5:0));
    } else {
      double outm=rnd.nextMin((int)strength);
      if(bi) {
        val[chn]=rnd.nextCoin()?(float)outm*5.f:(float)-outm*5.f;
      } else {
        val[chn]=(float)outm*10.f;
      }
    }
  }
  float get(int chn) {
    return val[chn];
  }
};

struct WBH {
  float val[16]={};
  void next(RND &rnd,bool bi,int chn,float strength) {
    if(strength==1.f) {
      val[chn]=float(rnd.nextDouble()*10-(bi?5:0));
    } else {
      double outw=rnd.nextWeibull(strength);
      if(bi) {
        val[chn]=rnd.nextCoin()?(float)outw*5.f:(float)-outw*5.f;
      } else {
        val[chn]=(float)outw*10.f;
      }
    }
  }
  float get(int chn) {
    return val[chn];
  }
};

struct TriH {
  float val[16]={};
  void next(RND &rnd,bool bi,int chn,float strength) {
    if(strength==1.f) {
      val[chn]=float(rnd.nextDouble()*10-(bi?5:0));
    } else {
      double outt=rnd.nextTri((int)strength);
      val[chn]=(float)outt*10.f-(bi?5.f:0.f);
    }
  }
  float get(int chn) {
    return val[chn];
  }
};


struct RndH2 : Module {
  enum ParamIds {
    BI_PARAM,STRENGTH_PARAM,CHANNELS_PARAM,SEED_PARAM,RANGE_PARAM,RANGE_CV_PARAM,OFFSET_PARAM,OFFSET_CV_PARAM,SLEW_PARAM,NUM_PARAMS
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
  dsp::SlewLimiter slewLimiterUni[16];
  dsp::SlewLimiter slewLimiterWB[16];
  dsp::SlewLimiter slewLimiterTri[16];
  dsp::SlewLimiter slewLimiterMin[16];
  UniH uniH;
  WBH wbH;
  MinH minH;
  TriH triH;
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
    configParam(CHANNELS_PARAM,1.f,16.f,1.f,"Polyphonic Channels");
    configParam(SEED_PARAM,0,1,0,"Random Seed");
    configParam(SLEW_PARAM,0.0f,1.f,0.f,"Slew");
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

  void next(int chn,bool bi=false) {
    float strength=params[STRENGTH_PARAM].getValue();
    strength=clamp(inputs[STRENGTH_INPUT].getNormalPolyVoltage(strength*.5f,chn)*2.f,1.f,20.f);
    if(outputs[MIN_OUTPUT].isConnected())
      minH.next(rnd,bi,chn,strength);
    if(outputs[WB_OUTPUT].isConnected())
      wbH.next(rnd,bi,chn,strength);
    if(outputs[TRI_OUTPUT].isConnected())
      triH.next(rnd,bi,chn,strength);
    if(outputs[UNI_OUTPUT].isConnected())
      uniH.next(rnd,bi,chn);
  }

  void process(const ProcessArgs &args) override {
    float slew=(params[SLEW_PARAM].getValue()+0.001f)*1000;
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
    for(int c=0;c<channels;c++) {
      slewLimiterUni[c].setRiseFall(1/slew,1/slew);
      slewLimiterWB[c].setRiseFall(1/slew,1/slew);
      slewLimiterTri[c].setRiseFall(1/slew,1/slew);
      slewLimiterMin[c].setRiseFall(1/slew,1/slew);
    }
    if(rstTrigger.process(inputs[RST_INPUT].getVoltage())) {
      if(inputs[SEED_INPUT].isConnected()) {
        setImmediateValue(getParamQuantity(SEED_PARAM),inputs[SEED_INPUT].getVoltage()*0.1);
      }
      float seedParam=params[SEED_PARAM].getValue();
      seedParam=floorf(seedParam*10000)/10000;
      auto seedInput=(unsigned long long)(floor((double)seedParam*(double)ULONG_MAX));
      //INFO("%.8f %lld",seedParam,seedInput);
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
      for(int c=0;c<channels;c++) {
        if(outputs[UNI_OUTPUT].isConnected())
          outputs[UNI_OUTPUT].setVoltage(slewLimiterUni[c].process(1.f,modify(uniH.get(c),c)),c);
        if(outputs[MIN_OUTPUT].isConnected())
          outputs[MIN_OUTPUT].setVoltage(slewLimiterMin[c].process(1.f,modify(minH.get(c),c)),c);
        if(outputs[WB_OUTPUT].isConnected())
          outputs[WB_OUTPUT].setVoltage(slewLimiterWB[c].process(1.f,modify(wbH.get(c),c)),c);
        if(outputs[TRI_OUTPUT].isConnected())
          outputs[TRI_OUTPUT].setVoltage(slewLimiterTri[c].process(1.f,modify(triH.get(c),c)),c);
      }
    } else {
      for(int c=0;c<channels;c++) {
        next(c,true);
        if(outputs[UNI_OUTPUT].isConnected())
          outputs[UNI_OUTPUT].setVoltage(uniH.get(c),c);
        if(outputs[MIN_OUTPUT].isConnected())
          outputs[MIN_OUTPUT].setVoltage(minH.get(c),c);
        if(outputs[WB_OUTPUT].isConnected())
          outputs[WB_OUTPUT].setVoltage(wbH.get(c),c);
        if(outputs[TRI_OUTPUT].isConnected())
          outputs[TRI_OUTPUT].setVoltage(triH.get(c),c);
      }
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
    y+=9;
    auto biPolarButton=createParam<SmallButtonWithLabel>(mm2px(Vec(6.8,y)),module,RndH2::BI_PARAM);
    biPolarButton->setLabel("BiP");
    addParam(biPolarButton);
    y=64;
    addParam(createParam<TrimbotWhite>(mm2px(Vec(x,y)),module,RndH2::RANGE_PARAM));
    addParam(createParam<TrimbotWhite>(mm2px(Vec(x1,y)),module,RndH2::OFFSET_PARAM));
    y+=8;
    addInput(createInput<SmallPort>(mm2px(Vec(x,y)),module,RndH2::RANGE_INPUT));
    addInput(createInput<SmallPort>(mm2px(Vec(x1,y)),module,RndH2::OFFSET_INPUT));
    y+=8;
    addParam(createParam<TrimbotWhite>(mm2px(Vec(x,y)),module,RndH2::RANGE_CV_PARAM));
    addParam(createParam<TrimbotWhite>(mm2px(Vec(x1,y)),module,RndH2::OFFSET_CV_PARAM));
    y=92;
    addParam(createParam<TrimbotWhite>(mm2px(Vec(6.9,y)),module,RndH2::SLEW_PARAM));

    y=104;
    addOutput(createOutput<SmallPort>(mm2px(Vec(x,y)),module,RndH2::WB_OUTPUT));
    addOutput(createOutput<SmallPort>(mm2px(Vec(x1,y)),module,RndH2::MIN_OUTPUT));
    y+=12;
    addOutput(createOutput<SmallPort>(mm2px(Vec(x,y)),module,RndH2::TRI_OUTPUT));
    addOutput(createOutput<SmallPort>(mm2px(Vec(x1,y)),module,RndH2::UNI_OUTPUT));

  }
};


Model *modelRndH2=createModel<RndH2,RndH2Widget>("RndH2");