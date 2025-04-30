#include "dcb.h"

#include "filter.hpp"

using simd::float_4;

template<typename T>
struct BOsc {
  T phs=0.f;
  float fdp;
  float pih=M_PI/2;

  BOsc() {
    fdp=4/powf(float(M_PI),2);
  }

  float powf_fast_ub(float a,float b) {
    union {
      float d;
      int x;
    } u={a};
    u.x=(int)(b*(u.x-1064631197)+1065353217);
    return u.d;
  }

  float_4 fastPow(float a,float_4 b) {
    float r[4]={};
    for(int k=0;k<4;k++) {
      r[k]=powf_fast_ub(a,b[k]);
    }
    return {r[0],r[1],r[2],r[3]};
  }

  void updatePhs(T fms) {
    phs+=fms;
    phs-=simd::floor(phs);
  }

  T firstHalf(T po) {
    T x=po*TWOPIF-pih;
    return (-fdp*simd::fabs(x*x)+1)*.5f+.5f;
  }

  T secondHalf(T po) {
    T x=(po-1.f)*TWOPIF+pih;
    return (simd::fabs(fdp*x*x)-1)*.5f+.5f;
  }

  T sinp(T ps) {
    return simd::ifelse(ps<0.5f,firstHalf(ps),secondHalf(ps));
  }

  T sin2pi_pade_05_5_4(T x) {
    x-=0.5f;
    T x2=x*x;
    T x3=x2*x;
    T x4=x2*x2;
    T x5=x2*x3;
    return (T(-6.283185307)*x+T(33.19863968)*x3-T(32.44191367)*x5)/
           (1+T(1.296008659)*x2+T(0.7028072946)*x4);
  }

  T process(T p1,T p2) {
    T amt=simd::rescale(p1,0.f,1.f,0,0.999f);
    T s=sin2pi_pade_05_5_4(phs)*0.49f+0.51f;
    T fold=fastPow(2.f,p2);
    //return simd::sin((s*(1-amt)/(amt*(1-s*2)+1))*fold);
    return 2*sinp(simd::fmod((s*(1-amt)/(amt*(1-s*2)+1))*fold,1.f))-1.f;
  }

};

#define OVERSMP 1

struct Osc9 : Module {
  enum ParamId {
    FREQ_PARAM,FM_PARAM,LIN_PARAM,P1_PARAM,P1_CV_PARAM,P2_PARAM,P2_CV_PARAM,PARAMS_LEN
  };
  enum InputId {
    VOCT_INPUT,FM_INPUT,P1_INPUT,P2_INPUT,INPUTS_LEN
  };
  enum OutputId {
    OUTPUT,OUTPUTS_LEN
  };
  enum LightId {
    LIGHTS_LEN
  };

  BOsc<float_4> bosc[4];
  DCBlocker<float_4> dcb[4];
  Cheby1_32_BandFilter<float_4> filters[4];

  Osc9() {
    config(PARAMS_LEN,INPUTS_LEN,OUTPUTS_LEN,LIGHTS_LEN);
    configParam(P1_PARAM,0,1,0.5,"P1");
    configParam(P1_CV_PARAM,0,1.f,0,"P1 CV");
    configInput(P1_INPUT,"P1 Amount");
    configParam(P2_PARAM,0,5,0,"P2");
    configParam(P2_CV_PARAM,0,1.f,0,"P2 CV");
    configInput(P2_INPUT,"P2");
    configParam(FREQ_PARAM,-4.f,4.f,0.f,"Frequency"," Hz",2,dsp::FREQ_C4);
    configInput(VOCT_INPUT,"V/Oct 1");
    configButton(LIN_PARAM,"Linear");
    configParam(FM_PARAM,0,3,0,"FM Amount","%",0,100);
    configInput(FM_INPUT,"FM");
    configOutput(OUTPUT,"CV");
  }

  void process(const ProcessArgs &args) override {
    float freqParam=params[FREQ_PARAM].getValue();
    float fmParam=params[FM_PARAM].getValue();
    bool linear=params[LIN_PARAM].getValue()>0;
    float p1Param=params[P1_PARAM].getValue();
    float p1ParamCV=params[P1_CV_PARAM].getValue();
    float p2Param=params[P2_PARAM].getValue();
    float p2ParamCV=params[P2_CV_PARAM].getValue();
    int channels=std::max(inputs[VOCT_INPUT].getChannels(),1);
    for(int c=0;c<channels;c+=4) {
      float_4 pitch1=freqParam+inputs[VOCT_INPUT].getVoltageSimd<float_4>(c);
      float_4 freq;
      if(linear) {
        freq=dsp::FREQ_C4*dsp::approxExp2_taylor5(pitch1+30.f)/std::pow(2.f,30.f);
        freq+=dsp::FREQ_C4*inputs[FM_INPUT].getPolyVoltageSimd<float_4>(c)*fmParam;
      } else {
        pitch1+=inputs[FM_INPUT].getPolyVoltageSimd<float_4>(c)*fmParam;
        freq=dsp::FREQ_C4*dsp::approxExp2_taylor5(pitch1+30.f)/std::pow(2.f,30.f);
      }
      freq=simd::fmin(freq,args.sampleRate/2);
      float_4 out=0.f;
      float_4 fms=args.sampleTime*freq/16.f;
      float_4 p1=simd::clamp(p1Param+inputs[P1_INPUT].getPolyVoltageSimd<float_4>(c)*0.1f*p1ParamCV,0.f,
                             1.f);
      float_4 p2=simd::clamp(p2Param+inputs[P2_INPUT].getPolyVoltageSimd<float_4>(c)*p2ParamCV,0.f,5.f);
      for(int k=0;k<16;k++) {
        bosc[c/4].updatePhs(fms);
        out=bosc[c/4].process(p1,p2);
        out=filters[c/4].process(out);
      }
      out=dcb[c/4].process(out);
      outputs[OUTPUT].setVoltageSimd(out*4,c);
    }
    outputs[OUTPUT].setChannels(channels);
  }
};


struct Osc9Widget : ModuleWidget {
  Osc9Widget(Osc9 *module) {
    setModule(module);
    setPanel(createPanel(asset::plugin(pluginInstance,"res/Osc9.svg")));
    float x=1.9f;
    addParam(createParam<TrimbotWhite>(mm2px(Vec(x,9)),module,Osc9::FREQ_PARAM));
    addInput(createInput<SmallPort>(mm2px(Vec(x,21)),module,Osc9::VOCT_INPUT));
    addInput(createInput<SmallPort>(mm2px(Vec(x,33)),module,Osc9::FM_INPUT));
    addParam(createParam<TrimbotWhite>(mm2px(Vec(x,40)),module,Osc9::FM_PARAM));
    addParam(createParam<MLED>(mm2px(Vec(x,52)),module,Osc9::LIN_PARAM));
    float y=64;
    addParam(createParam<TrimbotWhite>(mm2px(Vec(x,y)),module,Osc9::P1_PARAM));
    y+=7;
    addInput(createInput<SmallPort>(mm2px(Vec(x,y)),module,Osc9::P1_INPUT));
    y+=7;
    addParam(createParam<TrimbotWhite>(mm2px(Vec(x,y)),module,Osc9::P1_CV_PARAM));
    y+=12;
    addParam(createParam<TrimbotWhite>(mm2px(Vec(x,y)),module,Osc9::P2_PARAM));
    y+=7;
    addInput(createInput<SmallPort>(mm2px(Vec(x,y)),module,Osc9::P2_INPUT));
    y+=7;
    addParam(createParam<TrimbotWhite>(mm2px(Vec(x,y)),module,Osc9::P2_CV_PARAM));
    addOutput(createOutput<SmallPort>(mm2px(Vec(x,116)),module,Osc9::OUTPUT));
  }
};


Model *modelOsc9=createModel<Osc9,Osc9Widget>("Osc9");