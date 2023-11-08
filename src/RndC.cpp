#include "dcb.h"
#include "rnd.h"

struct RndC : Module {
  enum ParamIds {
    SEED_PARAM,BI_PARAM,FREQ_PARAM,FM_PARAM,STRENGTH_PARAM,CHANNELS_PARAM,NUM_PARAMS
  };
  enum InputIds {
    RST_INPUT,FM_INPUT,NUM_INPUTS
  };
  enum OutputIds {
    MIN_OUTPUT,WEIBULL_OUTPUT,TRI_OUTPUT,NUM_OUTPUTS
  };
  enum LightIds {
    NUM_LIGHTS
  };

  RndC() {
    config(NUM_PARAMS,NUM_INPUTS,NUM_OUTPUTS,NUM_LIGHTS);
    configParam(FM_PARAM,0.f,1.f,0.f,"FM Amount","%",0,100);
    configParam(SEED_PARAM,0.f,1.f,0.f,"SEED");
    configParam(BI_PARAM,0.f,1.f,1.f,"BI-Polar");
    configParam(FREQ_PARAM,-8.f,12.f,0.f,"Frequency"," Hz",2,1);
    configSwitch(STRENGTH_PARAM,1.f,20.f,1.f,"STRENGTH",{"Uniform","2","3","4","5","6","7","8","9","10","11","12","13","14","15","16","17","18","19","20"});
    configSwitch(CHANNELS_PARAM,1.f,16.f,8.f,"Polyphonic Channels",{"1","2","3","4","5","6","7","8","9","10","11","12","13","14","15","16"});
    configInput(RST_INPUT,"Reset");
    configInput(FM_INPUT,"FM");
    configOutput(MIN_OUTPUT,"Min Distribution");
    configOutput(WEIBULL_OUTPUT,"Weibull Distribution");
    configOutput(TRI_OUTPUT,"Triangular Distribution");
  }

  dsp::SchmittTrigger rstTrigger;

  RND rnd;
  float min[4][16];
  float max[4][16];
  float tri[4][16];
  float amp=1.f;
  float base=0.f;
  float phs[16]={};
  bool wbCoin[16];
  bool minCoin[16];

  void onAdd(const AddEvent &e) override {
    float seedParam=params[SEED_PARAM].getValue();
    rnd.reset((unsigned long long)(floor((double)seedParam*(double)ULONG_MAX)));
    int strengthParam=(int)params[STRENGTH_PARAM].getValue();
    init(strengthParam);
    //printf("onAdd seed=%f",seedParam);
  }

  void init(int strength) {
    for(int k=0;k<4;k++) {
      for(int j=0;j<16;j++) {
        min[k][j]=(float)rnd.nextMin(strength);
        phs[j]=0;
      }
    }
    for(int k=0;k<4;k++) {
      for(int j=0;j<16;j++) {
        max[k][j]=(float)rnd.nextWeibull(strength);
        phs[j]=0;
      }
    }
    for(int k=0;k<4;k++) {
      for(int j=0;j<16;j++) {
        tri[k][j]=(float)rnd.nextTri(strength);
        phs[j]=0;
      }
    }

  }

  float processMin(int k) {
    float a0=min[3][k]-min[2][k]-min[0][k]+min[1][k];
    float a1=min[0][k]-min[1][k]-a0;
    float a2=min[2][k]-min[0][k];
    float a3=min[1][k];
    return base+(((a0*phs[k]+a1)*phs[k]+a2)*phs[k]+a3)*amp;
  }

  float processWeibull(int k) {
    float a0=max[3][k]-max[2][k]-max[0][k]+max[1][k];
    float a1=max[0][k]-max[1][k]-a0;
    float a2=max[2][k]-max[0][k];
    float a3=max[1][k];
    return base+(((a0*phs[k]+a1)*phs[k]+a2)*phs[k]+a3)*amp;
  }

  float processTri(int k) {
    float a0=tri[3][k]-tri[2][k]-tri[0][k]+tri[1][k];
    float a1=tri[0][k]-tri[1][k]-a0;
    float a2=tri[2][k]-tri[0][k];
    float a3=tri[1][k];
    return base+(((a0*phs[k]+a1)*phs[k]+a2)*phs[k]+a3)*amp;
  }

  void nextRandomMin(int k,int strength,bool biPolar) {
    min[0][k]=min[1][k];
    min[1][k]=min[2][k];
    min[2][k]=min[3][k];
    if(biPolar) {
      min[3][k]=rnd.nextCoin()?float(rnd.nextMin(strength)):float(-rnd.nextMin(strength));
    } else {
      min[3][k]=float(rnd.nextMin(strength));
    }
  }

  void nextRandomWeibull(int k,int strength,bool biPolar) {

    max[0][k]=max[1][k];
    max[1][k]=max[2][k];
    max[2][k]=max[3][k];
    if(biPolar) {
      max[3][k]=rnd.nextCoin()?float(rnd.nextWeibull(strength)):float(-rnd.nextWeibull(strength));
    } else {
      max[3][k]=float(rnd.nextWeibull(strength));
    }
  }

  void nextRandomTri(int k,int strength) {
    tri[0][k]=tri[1][k];
    tri[1][k]=tri[2][k];
    tri[2][k]=tri[3][k];
    tri[3][k]=(float)rnd.nextTri(strength);
  }

  float adjustBiPolar(float value,bool biPolar) {
    if(biPolar) {
      return value*5.f;
    } else {
      return value*10.f;
    }
  }

  void process(const ProcessArgs &args) override {
    float bi=params[BI_PARAM].getValue();
    float pitch=params[FREQ_PARAM].getValue();
    float fmParam=params[FM_PARAM].getValue();

    int strength=(int)params[STRENGTH_PARAM].getValue();
    if(rstTrigger.process(inputs[RST_INPUT].getVoltage())) {
      float seedParam=params[SEED_PARAM].getValue();
      rnd.reset((unsigned long long)(floor((double)seedParam*(double)ULONG_MAX)));
      init(strength);
    }
    int channels=(int)params[CHANNELS_PARAM].getValue();
    if(inputs[FM_INPUT].isConnected()) {
      int inChannels=inputs[FM_INPUT].getChannels();
      if(inChannels>1) {
        channels=inChannels;
      }
    }
    for(int k=0;k<channels;k++) {
      pitch += inputs[FM_INPUT].getPolyVoltage(k) * fmParam;
      float freq = dsp::approxExp2_taylor5(pitch + 30.f) / std::pow(2.f, 30.f);
      float nextMin=processMin(k);
      float nextWeibull=processWeibull(k);
      float nextTri=processTri(k);
      phs[k]+=(args.sampleTime*freq);
      if(phs[k]>1.f) {
        nextRandomMin(k,strength,bi>0.f);
        nextRandomWeibull(k,strength,bi>0.f);
        nextRandomTri(k,strength);
        phs[k]-=1.f;
      }
      float outMin=adjustBiPolar(nextMin,bi>0.f);
      float outWb=adjustBiPolar(nextWeibull,bi>0.f);
      float outTri=nextTri*10.f-(bi>0.f?5.f:0.f);
      outputs[MIN_OUTPUT].setVoltage(outMin,k);
      outputs[WEIBULL_OUTPUT].setVoltage(outWb,k);
      outputs[TRI_OUTPUT].setVoltage(outTri,k);
    }

    outputs[MIN_OUTPUT].setChannels(channels);
    outputs[WEIBULL_OUTPUT].setChannels(channels);
    outputs[TRI_OUTPUT].setChannels(channels);
  }
};


struct RndCWidget : ModuleWidget {
  RndCWidget(RndC *module) {
    setModule(module);
    setPanel(APP->window->loadSvg(asset::plugin(pluginInstance,"res/RndC.svg")));

    float x=1.9f;
    addParam(createParam<TrimbotWhite>(mm2px(Vec(x,9)),module,RndC::FREQ_PARAM));
    addInput(createInput<SmallPort>(mm2px(Vec(x,17)),module,RndC::FM_INPUT));
    addParam(createParam<TrimbotWhite>(mm2px(Vec(x,25)),module,RndC::FM_PARAM));
    addInput(createInput<SmallPort>(mm2px(Vec(x,38)),module,RndC::RST_INPUT));
    addParam(createParam<TrimbotWhite>(mm2px(Vec(x,50)),module,RndC::SEED_PARAM));
    addParam(createParam<TrimbotWhiteSnap>(mm2px(Vec(x,62)),module,RndC::STRENGTH_PARAM));

    auto biPolarButton=createParam<SmallButtonWithLabel>(mm2px(Vec(1.5,71)),module,RndC::BI_PARAM);
    biPolarButton->setLabel("BiP");
    addParam(biPolarButton);
    addOutput(createOutput<SmallPort>(mm2px(Vec(x,80)),module,RndC::MIN_OUTPUT));
    addOutput(createOutput<SmallPort>(mm2px(Vec(x,92)),module,RndC::WEIBULL_OUTPUT));
    addOutput(createOutput<SmallPort>(mm2px(Vec(x,104)),module,RndC::TRI_OUTPUT));

    addParam(createParam<TrimbotWhite>(mm2px(Vec(x,116)),module,RndC::CHANNELS_PARAM));
  }
};


Model *modelRndC=createModel<RndC,RndCWidget>("RndC");