#include "dcb.h"
#include "filter.hpp"

template<size_t S>
struct Rungler {
  std::vector<bool> v;
  bool xr=false;
  float out=0.f;

  Rungler() {
    v.resize(S);
    for(auto dataReg:v) {
      dataReg=false;
    }
  }

  void step(bool in) {
    for(int i=S-1;i>0;i--) {
      v[i]=v[i-1];
    }
    if(xr)
      v[0]=(in!=v[S-1]);
    else
      v[0]=in;
    calc();
  }

  void loopStep() {
    for(int i=S-1;i>0;i--) {
      v[i]=v[i-1];
    }
      v[0]=v[S-1];
    calc();
  }

  void calc() {
    float next=(v[S-3]?32.0f:0.0f);
    next=next+(v[S-2]?64.0f:0.0f);
    next=next+(v[S-1]?128.0f:0.0f);
    next=(next/255.0f);
    out=next;
  }

};

template<typename T>
struct TriOsc {
  T phs=0.f;

  void updatePhs(T fms) {
    phs+=simd::fmin(fms,0.5f);
    phs-=simd::floor(phs);
  }

  T process(T ofs,bool bi=false) {
    T po=phs+ofs-simd::floor(phs+ofs);
    T ret=2.f*simd::fabs(po-simd::round(po));
    return bi?ret*2-1:ret;
  }

  void reset(T trigger) {
    phs=simd::ifelse(trigger,0.f,phs);
  }
};

template<typename T>
struct SquareOsc {
  T phs=0.f;

  void updatePhs(T fms) {
    phs+=simd::fmin(fms,0.5f);
    phs-=simd::floor(phs);
  }

  T process(T ofs,T pw=0.5f,bool bi=false) {
    T po=phs+ofs-simd::floor(phs+ofs);
    return simd::ifelse(po<pw,1.f,bi?-1.f:0.f);
  }

  void reset(T trigger) {
    phs=simd::ifelse(trigger,0.f,phs);
  }
};

struct Benny : Module {
  enum ParamId {
    FM1_PARAM,FM2_PARAM,FREQ1_PARAM,FREQ2_PARAM,R1_PARAM,R2_PARAM,CMP_PARAM,LOOP_PARAM,R1_CV_PARAM,R2_CV_PARAM,FM1_CV_PARAM,FM2_CV_PARAM,PARAMS_LEN
  };
  enum InputId {
    VOCT1_INPUT,VOCT2_INPUT,FM1_INPUT,FM2_INPUT,LOOP_INPUT,RST_INPUT,FM1_CV_INPUT, FM2_CV_INPUT, R1_CV_INPUT, R2_CV_INPUT,INPUTS_LEN
  };
  enum OutputId {
    TRI1_OUTPUT,TRI2_OUTPUT,SQR1_OUTPUT,SQR2_OUTPUT,R_OUTPUT,CMP_OUTPUT,XOR_OUTPUT,OUTPUTS_LEN
  };
  enum LightId {
    LIGHTS_LEN
  };

  TriOsc<float> triOsc[2];
  SquareOsc<float> sqrOsc[2];
  Cheby1_32_BandFilter<float> filterSQ1;
  Cheby1_32_BandFilter<float> filterSQ2;
  Cheby1_32_BandFilter<float> filterCmp;
  DCBlocker<float> dcBlocker[2];
  Rungler<8> rungler;
  dsp::SchmittTrigger trig;
  dsp::SchmittTrigger rstTrig;

  bool oversample=true;
  bool additive=true;
  float osc2=0;
  float osc1=0;
  float sq1=0;
  float sq2=0;

  Benny() {
    config(PARAMS_LEN,INPUTS_LEN,OUTPUTS_LEN,LIGHTS_LEN);
    configParam(CMP_PARAM,-1,1,0,"Cmp");
    configButton(LOOP_PARAM,"Loop");
    configInput(LOOP_INPUT,"Loop");
    configInput(RST_INPUT,"Reset");
    for(int k=0;k<2;k++) {
      std::string strNr=std::to_string(k+1);
      configInput(VOCT1_INPUT+k,"V/Oct "+strNr);
      configParam(FREQ1_PARAM+k,-14.f,4.f,0.f,"Freq "+strNr," Hz",2,dsp::FREQ_C4);
      configParam(FM1_PARAM+k,0,1,0,"FM "+strNr);
      configInput(FM1_INPUT+k,"FM Extern "+strNr);
      configInput(FM1_CV_INPUT+k,"FM CV "+strNr);
      configParam(FM1_CV_PARAM+k,0,1,0,"FM "+strNr,"%",0,100);
      configParam(R1_CV_PARAM+k,0,1,0,"Rungler CV "+strNr,"%",0,100);
      configParam(R1_PARAM+k,0,1,0,"Rungler "+strNr);
      configInput(R1_CV_INPUT+k,"Rungler CV"+strNr);

      configOutput(TRI1_OUTPUT+k,"Tri "+strNr);
      configOutput(SQR1_OUTPUT+k,"Sqr "+strNr);
    }
    configOutput(CMP_OUTPUT,"Pwm");
    configOutput(XOR_OUTPUT,"Xor");
    configOutput(R_OUTPUT,"Rungler");
  }

  void process(const ProcessArgs &args) override {
    if(inputs[LOOP_INPUT].isConnected()) {
      getParamQuantity(LOOP_PARAM)->setValue(inputs[LOOP_INPUT].getVoltage());
    }
    if(rstTrig.process(inputs[RST_INPUT].getVoltage())) {
        sqrOsc[0].reset(true);
    }
    bool loop=params[LOOP_PARAM].getValue()>=0.5f;

    float freqParam1=params[FREQ1_PARAM].getValue();
    float freqParam2=params[FREQ2_PARAM].getValue();
    float fmParam1=params[FM1_PARAM].getValue();
    float fmParam2=params[FM2_PARAM].getValue();
    float fmParamCV1=params[FM1_CV_PARAM].getValue();
    float fmParamCV2=params[FM2_CV_PARAM].getValue();
    float runglerParam1=params[R1_PARAM].getValue();
    float runglerParam2=params[R2_PARAM].getValue();
    float runglerParamCV1=params[R1_CV_PARAM].getValue();
    float runglerParamCV2=params[R2_CV_PARAM].getValue();
    float cmp=params[CMP_PARAM].getValue();
    float pitch1=freqParam1+inputs[VOCT1_INPUT].getVoltage();
    float pitch2=freqParam2+inputs[VOCT2_INPUT].getVoltage();
    float sq1Out=0;
    float sq2Out=0;
    float cmpOut=0;
    int count=oversample?16:1;//&&(outputs[SQR1_OUTPUT].isConnected()||outputs[SQR2_OUTPUT].isConnected()||outputs[CMP_OUTPUT].isConnected())?16:1;
    float p1=pitch1;
    float p2=pitch2;
    bool fm1Ext=inputs[FM1_INPUT].isConnected();
    bool fm2Ext=inputs[FM2_INPUT].isConnected();
    float runglerCVIn1=inputs[R1_CV_INPUT].getVoltage();
    float runglerCVIn2=inputs[R2_CV_INPUT].getVoltage();
    float fmCVIn1=inputs[FM1_CV_INPUT].getVoltage()*fmParamCV1;
    float fmCVIn2=inputs[FM2_CV_INPUT].getVoltage()*fmParamCV2;
    float fmIn1=inputs[FM1_INPUT].getVoltage();
    float fmIn2=inputs[FM2_INPUT].getVoltage();
    float rungler1=clamp(runglerParam1+runglerCVIn1*runglerParamCV1);
    float rungler2=clamp(runglerParam2+runglerCVIn2*runglerParamCV2);
    float fm1=clamp(fmParam1+fmCVIn1*fmParamCV1);
    float fm2=clamp(fmParam2+fmCVIn2*fmParamCV2);
    for(int k=0;k<count;k++) {
      float freq1;
      float freq2;
      float fac=5.f;
      if(!additive) {
        p1=pitch1;
        p2=pitch2;
      } else {
        if(oversample) fac=1.f;
      }
      float in1=fm1Ext?fmIn1:osc2;
      float in2=fm2Ext?fmIn2:osc1;
      p1+=in1*fm1*3;
      p1+=rungler.out*rungler1*fac;
      freq1=dsp::FREQ_C4*dsp::approxExp2_taylor5(p1+30.f)/std::pow(2.f,30.f);
      p2+=in2*fm2*3;
      p2+=rungler.out*rungler2*fac;
      freq2=dsp::FREQ_C4*dsp::approxExp2_taylor5(p2+30.f)/std::pow(2.f,30.f);
      freq1=simd::fmin(freq1,args.sampleRate/2)/float(count);
      freq2=simd::fmin(freq2,args.sampleRate/2)/float(count);
      triOsc[0].updatePhs(freq1*args.sampleTime);
      osc1=triOsc[0].process(0,true);
      triOsc[1].updatePhs(freq2*args.sampleTime);
      osc2=triOsc[1].process(0,true);
      sqrOsc[0].updatePhs(freq1*args.sampleTime);
      sq1=sqrOsc[0].process(0.f);
      sqrOsc[1].updatePhs(freq2*args.sampleTime);
      sq2=sqrOsc[1].process(0.f);
      if(trig.process(sq1*2)) {
        if(loop)
          rungler.loopStep();
        else
          rungler.step(sq2>0);
      }
      float c=osc1<osc2+cmp?-5.f:5.f;
      if(count>1) {
        sq1Out=filterSQ1.process(sq1*10.f-5);
        sq2Out=filterSQ2.process(sq2*10.f-5);
        cmpOut=filterCmp.process(c);
      } else {
        sq1Out=sq1*10.f-5;
        sq2Out=sq2*10.f-5;
        cmpOut=c;
      }
    }
    outputs[TRI1_OUTPUT].setVoltage(osc1*5);
    outputs[TRI2_OUTPUT].setVoltage(osc2*5);
    outputs[SQR1_OUTPUT].setVoltage(sq1Out);
    outputs[SQR2_OUTPUT].setVoltage(sq2Out);
    outputs[R_OUTPUT].setVoltage(rungler.out*5);
    outputs[XOR_OUTPUT].setVoltage(sq1!=sq2?10.f:0.f);
    outputs[CMP_OUTPUT].setVoltage(cmpOut);
  }

  json_t *dataToJson() override {
    json_t *data=json_object();
    json_object_set_new(data,"oversample",json_boolean(oversample));
    json_object_set_new(data,"additive",json_boolean(additive));
    json_object_set_new(data,"xor",json_boolean(rungler.xr));
    return data;
  }

  void dataFromJson(json_t *rootJ) override {
    json_t *jOverSample=json_object_get(rootJ,"oversample");
    if(jOverSample!=nullptr)
      oversample=json_boolean_value(jOverSample);
    json_t *jAdditive=json_object_get(rootJ,"additive");
    if(jAdditive!=nullptr)
      additive=json_boolean_value(jAdditive);
    json_t *jXor=json_object_get(rootJ,"xor");
    if(jXor!=nullptr)
      rungler.xr=json_boolean_value(jXor);
  }
};


struct BennyWidget : ModuleWidget {
  BennyWidget(Benny *module) {
    setModule(module);
    setPanel(createPanel(asset::plugin(pluginInstance,"res/Benny.svg")));
    float y=13.5;
    addParam(createParam<TrimbotWhite9>(mm2px(Vec(9.5,y)),module,Benny::FREQ1_PARAM));
    addParam(createParam<TrimbotWhite9>(mm2px(Vec(33,y)),module,Benny::FREQ2_PARAM));
    y=29;
    addParam(createParam<TrimbotWhite>(mm2px(Vec(14.5,y)),module,Benny::FM1_CV_PARAM));
    addParam(createParam<TrimbotWhite>(mm2px(Vec(30.5,y)),module,Benny::FM2_CV_PARAM));
    addParam(createParam<TrimbotWhite9>(mm2px(Vec(4.5,y+2)),module,Benny::FM1_PARAM));
    addParam(createParam<TrimbotWhite9>(mm2px(Vec(38,y+2)),module,Benny::FM2_PARAM));
    addInput(createInput<SmallPort>(mm2px(Vec(14.5,y+8)),module,Benny::FM1_CV_INPUT));
    addInput(createInput<SmallPort>(mm2px(Vec(30.5,y+8)),module,Benny::FM2_CV_INPUT));
    y=49;
    addInput(createInput<SmallPort>(mm2px(Vec(10,y)),module,Benny::FM1_INPUT));
    addInput(createInput<SmallPort>(mm2px(Vec(34.5,y)),module,Benny::FM2_INPUT));
    y=63;
    addParam(createParam<TrimbotWhite>(mm2px(Vec(14.5,y)),module,Benny::R1_CV_PARAM));
    addParam(createParam<TrimbotWhite>(mm2px(Vec(30.5,y)),module,Benny::R2_CV_PARAM));
    addParam(createParam<TrimbotWhite9>(mm2px(Vec(4.5,y+3)),module,Benny::R1_PARAM));
    addParam(createParam<TrimbotWhite9>(mm2px(Vec(38,y+3)),module,Benny::R2_PARAM));
    addInput(createInput<SmallPort>(mm2px(Vec(14.5,y+8)),module,Benny::R1_CV_INPUT));
    addInput(createInput<SmallPort>(mm2px(Vec(30.5,y+8)),module,Benny::R2_CV_INPUT));
    y=86;
    addParam(createParam<MLED>(mm2px(Vec(4.5,y)),module,Benny::LOOP_PARAM));
    addInput(createInput<SmallPort>(mm2px(Vec(14.5,y)),module,Benny::LOOP_INPUT));
    addInput(createInput<SmallPort>(mm2px(Vec(30,y)),module,Benny::VOCT1_INPUT));
    addInput(createInput<SmallPort>(mm2px(Vec(40,y)),module,Benny::VOCT2_INPUT));

    y=104;
    addOutput(createOutput<SmallPort>(mm2px(Vec(30,y)),module,Benny::TRI1_OUTPUT));
    addOutput(createOutput<SmallPort>(mm2px(Vec(40,y)),module,Benny::TRI2_OUTPUT));
    addOutput(createOutput<SmallPort>(mm2px(Vec(4.5,y)),module,Benny::R_OUTPUT));
    addOutput(createOutput<SmallPort>(mm2px(Vec(14.5,y)),module,Benny::XOR_OUTPUT));

    y=116;
    addOutput(createOutput<SmallPort>(mm2px(Vec(30,y)),module,Benny::SQR1_OUTPUT));
    addOutput(createOutput<SmallPort>(mm2px(Vec(40,y)),module,Benny::SQR2_OUTPUT));
    addParam(createParam<TrimbotWhite>(mm2px(Vec(4.5,y)),module,Benny::CMP_PARAM));
    addOutput(createOutput<SmallPort>(mm2px(Vec(14.5,y)),module,Benny::CMP_OUTPUT));
    addInput(createInput<SmallPort>(mm2px(Vec(22,110.5)),module,Benny::RST_INPUT));
  }

  void appendContextMenu(Menu *menu) override {
    auto *module=dynamic_cast<Benny *>(this->module);
    assert(module);

    menu->addChild(new MenuSeparator);

    menu->addChild(createBoolPtrMenuItem("Oversample","",&module->oversample));
    menu->addChild(createBoolPtrMenuItem("Additive Oversample","",&module->additive));
    menu->addChild(createBoolPtrMenuItem("Xor mode","",&module->rungler.xr));
  }
};


Model *modelBenny=createModel<Benny,BennyWidget>("Benny");