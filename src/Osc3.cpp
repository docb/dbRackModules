#include "dcb.h"
#include "filter.hpp"

using simd::float_4;
using simd::int32_4;

struct StepOsc4 {
  int pos[4]={};
  float phs[4]={};
  float x[4]={};
  float *pts=nullptr;
  int len=0;

  void init(float *_pts,int _len) {
    pts=_pts;
    len=_len;
    len=std::max(_len,2);
    for(int k=0;k<4;k++) {
      x[k]=pts[0];
      pos[k]=1;
      phs[k]=0;
    }
  }

  void updatePhs(float sampleTime,float_4 freq) {
    for(int k=0;k<4;k++) {
      phs[k]+=(sampleTime*freq[k]*float(len));
      if(phs[k]>1.f) {
        phs[k]=phs[k]-simd::floor(phs[k]);
        x[k]=pts[pos[k]];
        pos[k]=(pos[k]+1)%len;
      }
    }
  }

  float_4 process() {
    return {x[0],x[1],x[2],x[3]};
  }

  void reset() {
    for(int k=0;k<4;k++) {
      x[k]=pts[0];
      pos[k]=1;
      phs[k]=0;
    }
  }
};

struct LineOsc4 {
  int pos[4]={};
  float phs[4]={};
  float x0[4]={};
  float x1[4]={};
  float out[4]={};
  float *pts=nullptr;
  int len=0;

  void init(float *_pts,int _len) {
    pts=_pts;
    len=_len;
    len=std::max(_len,2);
    for(int k=0;k<4;k++) {
      x0[k]=pts[0];
      x1[k]=pts[1];
      pos[k]=2;
      phs[k]=0;
      out[k]=0;
    }
  }

  void updatePhs(float sampleTime,float_4 freq) {
    for(int k=0;k<4;k++) {
      phs[k]+=(sampleTime*freq[k]*float(len));
      if(phs[k]>1.f) {
        phs[k]=phs[k]-simd::floor(phs[k]);
        x0[k]=x1[k];
        x1[k]=pts[pos[k]];
        pos[k]=(pos[k]+1)%len;
      }
      out[k]=x0[k]+phs[k]*(x1[k]-x0[k]);
    }
  }

  float_4 process() {
    return {out[0],out[1],out[2],out[3]};
  }

  void reset() {
    for(int k=0;k<4;k++) {
      x0[k]=pts[0];
      x1[k]=pts[1];
      pos[k]=2;
      phs[k]=0;
      out[k]=0;
    }
  }
};

struct Osc3 : Module {
  enum ParamId {
    FREQ_PARAM,FM_PARAM,LIN_PARAM,PARAMS_LEN
  };
  enum InputId {
    VOCT_INPUT,PTS_INPUT,FM_INPUT,INPUTS_LEN
  };
  enum OutputId {
    LIN_OUTPUT,STEP_OUTPUT,OUTPUTS_LEN
  };
  enum LightId {
    LIGHTS_LEN
  };

  LineOsc4 oscL[4];
  DCBlocker<float_4> dcbL[4];
  Cheby1_32_BandFilter<float_4> filtersL[4];

  StepOsc4 osc[4];
  DCBlocker<float_4> dcb[4];
  Cheby1_32_BandFilter<float_4> filters[4];
  float pts[16]={};
  int len=0;
  dsp::ClockDivider divider;
  bool alias=false;

  Osc3() {
    config(PARAMS_LEN,INPUTS_LEN,OUTPUTS_LEN,LIGHTS_LEN);
    configParam(FREQ_PARAM,-8.f,4.f,0.f,"Frequency"," Hz",2,dsp::FREQ_C4);
    configButton(LIN_PARAM,"Linear");
    configParam(FM_PARAM,0,1,0,"FM Amount","%",0,100);
    configInput(FM_INPUT,"FM");
    configInput(VOCT_INPUT,"V/Oct");
    configInput(PTS_INPUT,"Points");
    configOutput(STEP_OUTPUT,"Steps");
    configOutput(LIN_OUTPUT,"Lines");
    divider.setDivision(32);
  }

  void onAdd(const AddEvent &e) override {
    for(int k=0;k<4;k++) {
      osc[k].init(pts,len);
      oscL[k].init(pts,len);
    }
  }

  void process(const ProcessArgs &args) override {
    if(divider.process()) {
      int ptsChannels=std::max(inputs[PTS_INPUT].getChannels(),2);
      if(ptsChannels!=len) {
        for(int c=0;c<4;c++) {
          len=ptsChannels;
          osc[c].init(pts,len);
          oscL[c].init(pts,len);
        }
      }
      for(int k=0;k<16;k++) {
        pts[k]=inputs[PTS_INPUT].getVoltage(k);
      }
    }
    int channels=std::max(inputs[VOCT_INPUT].getChannels(),1);
    for(int c=0;c<channels;c+=4) {
      float freqParam=params[FREQ_PARAM].getValue();
      float fmParam=params[FM_PARAM].getValue();
      bool linear=params[LIN_PARAM].getValue()>0;
      float_4 pitch=freqParam+inputs[VOCT_INPUT].getPolyVoltageSimd<float_4>(c);
      float_4 freq;
      if(linear) {
        freq=dsp::FREQ_C4*dsp::approxExp2_taylor5(pitch+30.f)/std::pow(2.f,30.f);
        freq+=dsp::FREQ_C4*inputs[FM_INPUT].getPolyVoltageSimd<float_4>(c)*fmParam;
      } else {
        pitch+=inputs[FM_INPUT].getPolyVoltageSimd<float_4>(c)*fmParam;
        freq=dsp::FREQ_C4*dsp::approxExp2_taylor5(pitch+30.f)/std::pow(2.f,30.f);
      }
      freq=simd::fmin(freq,args.sampleRate/2);
      float_4 v;
      if(outputs[STEP_OUTPUT].isConnected()) {
        for(int k=0;k<16;k++) {
          osc[c/4].updatePhs(args.sampleTime,freq/16.f);
          v=osc[c/4].process();
          v=filters[c/4].process(v);
        }
        outputs[STEP_OUTPUT].setVoltageSimd(dcb[c/4].process(v),c);
      } else {
        outputs[STEP_OUTPUT].setVoltageSimd(float_4(0.f),c);
      }
      float_4 vL;
      if(outputs[LIN_OUTPUT].isConnected()) {
        for(int k=0;k<16;k++) {
          oscL[c/4].updatePhs(args.sampleTime,freq/16.f);
          vL=oscL[c/4].process();
          vL=filtersL[c/4].process(vL);
        }
        outputs[LIN_OUTPUT].setVoltageSimd(dcbL[c/4].process(vL),c);
      } else {
        outputs[LIN_OUTPUT].setVoltageSimd(float_4(0.f),c);
      }
    }
    if(outputs[STEP_OUTPUT].isConnected())
      outputs[STEP_OUTPUT].setChannels(channels);
    if(outputs[LIN_OUTPUT].isConnected())
      outputs[LIN_OUTPUT].setChannels(channels);
  }
};


struct Osc3Widget : ModuleWidget {
  Osc3Widget(Osc3 *module) {
    setModule(module);
    setPanel(createPanel(asset::plugin(pluginInstance,"res/Osc3.svg")));
    float x=1.9;
    addParam(createParam<TrimbotWhite>(mm2px(Vec(x,10)),module,Osc3::FREQ_PARAM));
    addInput(createInput<SmallPort>(mm2px(Vec(x,22)),module,Osc3::VOCT_INPUT));
    addInput(createInput<SmallPort>(mm2px(Vec(x,34)),module,Osc3::FM_INPUT));
    addParam(createParam<TrimbotWhite>(mm2px(Vec(x,41)),module,Osc3::FM_PARAM));
    addParam(createParam<MLED>(mm2px(Vec(x,52)),module,Osc3::LIN_PARAM));
    addInput(createInput<SmallPort>(mm2px(Vec(x,68)),module,Osc3::PTS_INPUT));
    addOutput(createOutput<SmallPort>(mm2px(Vec(x,104)),module,Osc3::LIN_OUTPUT));
    addOutput(createOutput<SmallPort>(mm2px(Vec(x,116)),module,Osc3::STEP_OUTPUT));

  }
};


Model *modelOsc3=createModel<Osc3,Osc3Widget>("Osc3");