#include "dcb.h"
#include "rnd.h"

#define MAX_POINTS 16

struct Distribution {
  enum Dist {
    UNI,CAUCHY,LOGIST,HYPERBCOS,ARCSINE,EXP,EXT,NUM_DISTS
  };
  RND ampRnd;
  RND durRnd;
  std::vector<std::string> labels={"UNI","CAUCHY","LOGIST","HYPERBCOS","ARCSINE","EXP","EXT"};

  float processAmp(int dist,float a) {
    return process(dist,a,(float)ampRnd.nextDouble());
  }

  float processDur(int dist,float a) {
    return process(dist,a,(float)durRnd.nextDouble());
  }

  float process(int dist,float a,float rnd) {
    if (a > 1.0f)
      a = 1.0f;
    else if (a < 0.0001f)
      a = 0.0001f;

    switch(dist) {
      case CAUCHY: {
        return (1.f/a)*tanf(atanf(10.f*a)*(rnd*2.f-1.f))*0.1f;
      }
      case LOGIST: {
        float c=0.5f+(0.499f*a);
        c=logf((1.0f-c)/c);
        float r=((rnd-0.5f)*0.998f*a)+0.5f;
        return logf((1.0f-r)/r)/c;
      }
      case HYPERBCOS : {
        float a1 = 0.95f + a*0.05f; // out of the range 0.95-1 noting noticeable
        float c=tanf(1.5692255f*a1);
        float r=tanf(1.5692255f*a1*rnd)/c;
        return (logf(r*0.999f+0.001f)*-0.1447648f)*2.0f-1.f;
      }
      case ARCSINE:
        //return a/2.f*(1.f-sin((0.5-rnd)*M_PI));
        return sinf(M_PI*(rnd-0.5f)*a)/(sinf(1.5707963f*a));
      case EXP: {
        float c=logf(1.0f-(0.999f*a));
        float r=rnd*0.999f*a;
        return (logf(1.0f-r)/c)*2.0f-1.0f;
      }
      case EXT: {
        return a*2.f-1.f;
      }
      default:
        return rnd*2.f-1.f;
    }
  }
};

struct DSS {
  Distribution distribution;
  DCBlocker<float> dcBlock;
  float ampY[MAX_POINTS]={};
  float durX[MAX_POINTS]={};
  float phs=1;
  float speed=100;
  float lastAmp=0;
  float nextAmp=0;
  float cdur;
  int lastIndex=0;

  float mirrorAmp(float v) {
    float amp=v;
    if(amp<-1.0f||amp>1.0f) {
      if(amp<0.0f)
        amp+=4.0f;
      amp=fmodf(amp,4.0f);
      if(amp>1.0f) {
        amp=(amp<3.0f?2.0f-amp:amp-4.0f);
      }
    }
    return amp;
  }

  float mirrorDur(float v) {
    float dur=v;
    if(dur>1.0f)
      dur=2.0f-fmodf(dur,2.0f);
    else if(dur<0.0f)
      dur=2.0f-fmodf(dur+2.0f,2.0f);
    return dur;
  }

  float process(float sampleDelta,int numPoints,int distAmp,float distAmpParam,float distAmpScl,int distDur,float distDurParam,float distDurScl,float minFreq,float maxFreq) {
    if(phs>=1.0f) {
      int index=lastIndex;
      phs-=1.0f;

      if(numPoints>MAX_POINTS||numPoints<1)
        numPoints=MAX_POINTS;
      lastIndex=index=(index+1)%numPoints;
      lastAmp=nextAmp;
      float dist=distribution.processAmp(distAmp,distAmpParam);
      nextAmp=ampY[index]+distAmpScl*dist;
      nextAmp=mirrorAmp(nextAmp);
      ampY[index]=nextAmp;

      dist=distribution.processDur(distDur,distDurParam);
      cdur=durX[index]+distDurScl*dist;
      cdur=mirrorDur(cdur);
      durX[index]=cdur;
      speed=(minFreq+(maxFreq-minFreq)*cdur)*sampleDelta*(float)numPoints;
    }
    float out=5.f*((1.0f-phs)*lastAmp+phs*nextAmp);
    phs+=speed;
    return dcBlock.process(out);
  }

};

struct Gendy : Module {
  enum ParamId {
    AMPDIST_PARAM,AMPDISTPARAM_PARAM,AMPDISTSCL_PARAM,AMPDISTPARAM_CV_PARAM,AMPDISTSCL_CV_PARAM,DURDIST_PARAM,DURDISTPARAM_PARAM,DURDISTSCL_PARAM,DURDISTPARAM_CV_PARAM,DURDISTSCL_CV_PARAM,DTUNE_PARAM,SEED_PARAM,NUM_PARAM,PARAMS_LEN
  };
  enum InputId {
    AMPDIST_INPUT,AMPDIST_EXT_INPUT,AMPDISTPARAM_INPUT,AMPDISTSCL_INPUT,DURDIST_INPUT,DURDIST_EXT_INPUT,DURDISTPARAM_INPUT,DURDISTSCL_INPUT,DTUNE_INPUT,SEED_INPUT,NUM_INPUT,VOCT_INPUT,RST_INPUT,INPUTS_LEN
  };
  enum OutputId {
    V_OUTPUT,OUTPUTS_LEN
  };
  enum LightId {
    LIGHTS_LEN
  };

  DSS dss[16];

  Gendy() {
    config(PARAMS_LEN,INPUTS_LEN,OUTPUTS_LEN,LIGHTS_LEN);
    configSwitch(AMPDIST_PARAM,0,Distribution::NUM_DISTS-1,0,"Distribution",dss[0].distribution.labels);
    getParamQuantity(AMPDIST_PARAM)->snapEnabled = true;
    configSwitch(DURDIST_PARAM,0,Distribution::NUM_DISTS-1,0,"Distribution",dss[0].distribution.labels);
    getParamQuantity(DURDIST_PARAM)->snapEnabled = true;
    configParam(AMPDISTPARAM_PARAM,0,1,0.9,"Amp Dist Param");
    configParam(DURDISTPARAM_PARAM,0,1,0.9,"Duration Dist Param");
    configParam(AMPDISTSCL_PARAM,0,1,1,"Amp Dist Scale Factor");
    configParam(DURDISTSCL_PARAM,0,1,1,"Duration Dist Scale Factor");

    configParam(AMPDISTPARAM_CV_PARAM,0,1,0,"Amp Dist Param CV"," %",0,100.f);
    configParam(DURDISTPARAM_CV_PARAM,0,1,0,"Duration Dist Param CV"," %",0,100.f);
    configParam(AMPDISTSCL_CV_PARAM,0,1,0,"Amp Dist Scale CV"," %",0,100.f);
    configParam(DURDISTPARAM_CV_PARAM,0,1,0,"Duration Dist Scale CV"," %",0,100.f);

    configParam(DTUNE_PARAM,0,1,0,"Frequency Spread");
    configParam(SEED_PARAM,0,1,0,"Random Seed");
    configParam(NUM_PARAM,1,MAX_POINTS-1,MAX_POINTS-1,"Number of Points");
    getParamQuantity(NUM_PARAM)->snapEnabled = true;

    configInput(AMPDIST_EXT_INPUT,"Amp Ext");
    configInput(AMPDIST_INPUT,"Amp Distribution");
    configInput(AMPDISTSCL_INPUT,"Amp Scale");
    configInput(AMPDISTPARAM_INPUT,"Amp Distribution Param");
    configInput(DURDIST_EXT_INPUT,"Dur Ext");
    configInput(DURDIST_INPUT,"Duration Distribution");
    configInput(DURDISTSCL_INPUT,"Duration Scale");
    configInput(DURDISTPARAM_INPUT,"Duration Distribution Param");
    configInput(DTUNE_INPUT,"Detune");
    configInput(SEED_INPUT,"Seed");
    configInput(NUM_INPUT,"Number of points");
    configInput(VOCT_INPUT,"V/Oct");
    configInput(RST_INPUT,"Reset");
    configOutput(V_OUTPUT,"Audio");
  }

  void process(const ProcessArgs &args) override {
    //(float sampleDelta,int numPoints,int distAmp,float distAmpParam,float distAmpScl,int distDur,float distDurParam,float distDurScl,float minFreq,float maxFreq)
    int channels=std::max(inputs[VOCT_INPUT].getChannels(),1);
    for(int k=0;k<channels;k++) {
      float voct = inputs[VOCT_INPUT].getVoltage(k);
      float dtune;
      if(inputs[DTUNE_INPUT].isConnected()) {
        dtune = clamp(inputs[DTUNE_INPUT].getPolyVoltage(k),0.f,10.f)*.1f;
      } else {
        dtune = params[DTUNE_PARAM].getValue();
      }

      float minFreq = 261.626f*powf(2.0f,voct-dtune);
      float maxFreq = 261.626f*powf(2.0f,voct+dtune);
      int numPoints;
      if(inputs[NUM_INPUT].isConnected()) {
        numPoints = clamp(inputs[NUM_INPUT].getPolyVoltage(k),1.f,10.f)*0.1*(MAX_POINTS-1);
      } else {
        numPoints = params[NUM_PARAM].getValue();
      }
      int ampDist;
      if(inputs[AMPDIST_INPUT].isConnected()) {
        ampDist = clamp(inputs[AMPDIST_INPUT].getPolyVoltage(k),0.f,10.f)*.1f*Distribution::NUM_DISTS;
      } else {
        ampDist = params[AMPDIST_PARAM].getValue();
      }
      int durDist;
      if(inputs[DURDIST_INPUT].isConnected()) {
        durDist = clamp(inputs[DURDIST_INPUT].getPolyVoltage(k),0.f,10.f)*.1f*Distribution::NUM_DISTS;
      } else {
        durDist = params[DURDIST_PARAM].getValue();
      }

      float ampDistParam = params[AMPDISTPARAM_PARAM].getValue() + clamp(inputs[AMPDISTPARAM_INPUT].getPolyVoltage(k),0.f,10.f)*.1f * params[AMPDISTPARAM_CV_PARAM].getValue();
      if(ampDist == Distribution::EXT) {
        ampDistParam = inputs[AMPDIST_EXT_INPUT].getPolyVoltage(k)*0.1f*ampDistParam;//use param for attenuating
      }

      float durDistParam = params[DURDISTPARAM_PARAM].getValue() + clamp(inputs[DURDISTPARAM_INPUT].getPolyVoltage(k),0.f,10.f)*0.1f * params[DURDISTPARAM_CV_PARAM].getValue();
      if(durDist == Distribution::EXT) {
        durDistParam = inputs[DURDIST_EXT_INPUT].getPolyVoltage(k)*0.1f*durDistParam;
      }
      float ampDistScl = params[AMPDISTSCL_PARAM].getValue() + clamp(inputs[AMPDISTSCL_INPUT].getPolyVoltage(k),0.f,10.f)*0.1f * params[AMPDISTSCL_CV_PARAM].getValue();
      float durDistScl = params[DURDISTSCL_PARAM].getValue() + clamp(inputs[DURDISTSCL_INPUT].getPolyVoltage(k),0.f,10.f)*0.1f * params[DURDISTSCL_CV_PARAM].getValue();
      float out = dss[k].process(args.sampleTime,numPoints,ampDist,ampDistParam,ampDistScl,durDist,durDistParam,durDistScl,minFreq,maxFreq);
      outputs[V_OUTPUT].setVoltage(out,k);
    }
    outputs[V_OUTPUT].setChannels(channels);
  }
};


struct GendyWidget : ModuleWidget {
  GendyWidget(Gendy *module) {
    setModule(module);
    setPanel(createPanel(asset::plugin(pluginInstance,"res/Gendy.svg")));

    addChild(createWidget<ScrewSilver>(Vec(RACK_GRID_WIDTH,0)));
    addChild(createWidget<ScrewSilver>(Vec(box.size.x-2*RACK_GRID_WIDTH,0)));
    addChild(createWidget<ScrewSilver>(Vec(RACK_GRID_WIDTH,RACK_GRID_HEIGHT-RACK_GRID_WIDTH)));
    addChild(createWidget<ScrewSilver>(Vec(box.size.x-2*RACK_GRID_WIDTH,RACK_GRID_HEIGHT-RACK_GRID_WIDTH)));

    addParam(createParam<TrimbotWhite>(mm2px(Vec(2,MHEIGHT-109.f-6.3)),module,Gendy::AMPDIST_PARAM));
    addParam(createParam<TrimbotWhite>(mm2px(Vec(2,MHEIGHT-96.f-6.3)),module,Gendy::AMPDISTPARAM_PARAM));
    addParam(createParam<TrimbotWhite>(mm2px(Vec(22,MHEIGHT-96.f-6.3)),module,Gendy::AMPDISTPARAM_CV_PARAM));
    addParam(createParam<TrimbotWhite>(mm2px(Vec(2,MHEIGHT-84.f-6.3)),module,Gendy::AMPDISTSCL_PARAM));
    addParam(createParam<TrimbotWhite>(mm2px(Vec(22,MHEIGHT-84.f-6.3)),module,Gendy::AMPDISTSCL_CV_PARAM));

    addParam(createParam<TrimbotWhite>(mm2px(Vec(2,MHEIGHT-72.f-6.3)),module,Gendy::DURDIST_PARAM));
    addParam(createParam<TrimbotWhite>(mm2px(Vec(2,MHEIGHT-60.f-6.3)),module,Gendy::DURDISTPARAM_PARAM));
    addParam(createParam<TrimbotWhite>(mm2px(Vec(22,MHEIGHT-60.f-6.3)),module,Gendy::DURDISTPARAM_CV_PARAM));
    addParam(createParam<TrimbotWhite>(mm2px(Vec(2,MHEIGHT-48.f-6.3)),module,Gendy::DURDISTSCL_PARAM));
    addParam(createParam<TrimbotWhite>(mm2px(Vec(22,MHEIGHT-48.f-6.3)),module,Gendy::DURDISTSCL_CV_PARAM));

    addParam(createParam<TrimbotWhite>(mm2px(Vec(2,MHEIGHT-35.f-6.3)),module,Gendy::DTUNE_PARAM));
    addParam(createParam<TrimbotWhite>(mm2px(Vec(11.9,MHEIGHT-35.f-6.3)),module,Gendy::SEED_PARAM));
    addParam(createParam<TrimbotWhite>(mm2px(Vec(22,MHEIGHT-35.f-6.3)),module,Gendy::NUM_PARAM));

    addInput(createInput<SmallPort>(mm2px(Vec(11.9,MHEIGHT-109.f-6.3)),module,Gendy::AMPDIST_INPUT));
    addInput(createInput<SmallPort>(mm2px(Vec(22,MHEIGHT-109.f-6.3)),module,Gendy::AMPDIST_EXT_INPUT));
    addInput(createInput<SmallPort>(mm2px(Vec(11.9,MHEIGHT-96.f-6.3)),module,Gendy::AMPDISTPARAM_INPUT));
    addInput(createInput<SmallPort>(mm2px(Vec(11.9,MHEIGHT-84.f-6.3)),module,Gendy::AMPDISTSCL_INPUT));

    addInput(createInput<SmallPort>(mm2px(Vec(11.9,MHEIGHT-72.f-6.3)),module,Gendy::DURDIST_INPUT));
    addInput(createInput<SmallPort>(mm2px(Vec(22,MHEIGHT-72.f-6.3)),module,Gendy::DURDIST_EXT_INPUT));
    addInput(createInput<SmallPort>(mm2px(Vec(11.9,MHEIGHT-60.f-6.3)),module,Gendy::DURDISTPARAM_INPUT));
    addInput(createInput<SmallPort>(mm2px(Vec(11.9,MHEIGHT-48.f-6.3)),module,Gendy::DURDISTSCL_INPUT));

    addInput(createInput<SmallPort>(mm2px(Vec(2,MHEIGHT-26.5f-6.3)),module,Gendy::DTUNE_INPUT));
    addInput(createInput<SmallPort>(mm2px(Vec(11.9,MHEIGHT-26.5f-6.3)),module,Gendy::SEED_INPUT));
    addInput(createInput<SmallPort>(mm2px(Vec(22,MHEIGHT-26.5f-6.3)),module,Gendy::NUM_INPUT));

    addInput(createInput<SmallPort>(mm2px(Vec(2,MHEIGHT-10-6.3)),module,Gendy::VOCT_INPUT));
    addInput(createInput<SmallPort>(mm2px(Vec(11.9,MHEIGHT-10-6.3)),module,Gendy::RST_INPUT));
    addOutput(createOutput<SmallPort>(mm2px(Vec(22,MHEIGHT-10-6.3)),module,Gendy::V_OUTPUT));

  }
};


Model *modelGendy=createModel<Gendy,GendyWidget>("Gendy");