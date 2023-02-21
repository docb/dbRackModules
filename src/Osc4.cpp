#include <utility>

#include "dcb.h"
#include "filter.hpp"
using simd::float_4;
struct ESegItem {
  float value;
  float duration;
  float bend;
  ESegItem(float v,float d,float b) : value(v),duration(d),bend(b) {}
};
struct ESeg {
  std::vector<ESegItem> params;
  uint32_t len;
  ESeg(std::vector<ESegItem> _params) : params(std::move(_params)) {
    len = params.size();
  }
  float process(float in) {
    if(len==0) return 0;
    if(len==1) return params[0].value;
    float s=0;
    float pre=0;
    uint32_t j=0;
    for(;j<len;j++) {
      pre=s;
      s+=params[j].duration;
      if(in<s) break;
    }
    if(j>=len) return params[len-1].value;
    float pct=params[j].duration==0?1:(in-pre)/params[j].duration;
    float f=params[j].bend==0?pct:(1 - std::exp( pct*params[j].bend)) / (1 - std::exp(params[j].bend));
    return params[j].value+f*(params[j+1].value-params[j].value);
  }
};

template<typename T>
struct FSquareOsc {
  T phs=0.f;
  T lastSyncValue = 0.f;
  T syncDirection = 1.f;
  void updatePhs(float sampleTime,T freq, bool soft=false) {

  }

  T process(T wv,T fms, bool syncEnabled, T syncValue, bool soft) {
    T phsDelta=simd::fmin(fms,0.5f);
    if(soft) {
      phsDelta *= syncDirection;
    } else {
      syncDirection = 1.f;
    }
    phs+=phsDelta;
    phs-=simd::floor(phs);
    if (syncEnabled) {
      T deltaSync=syncValue-lastSyncValue;
      T syncCrossing=-lastSyncValue/deltaSync;
      lastSyncValue=syncValue;
      T sync=(0.f<syncCrossing)&(syncCrossing<=1.f)&(syncValue>=0.f);
      int syncMask=simd::movemask(sync);
      if(syncMask) {
        if (soft) {
          syncDirection = simd::ifelse(sync, -syncDirection, syncDirection);
        }
        else {
          phs=simd::ifelse(sync,(1.f-syncCrossing)*phsDelta,phs);
        }
      }
    }
    T wva=simd::fabs(wv);
    return simd::ifelse(wv<0,simd::ifelse(phs<wva,-1+phs*2/wva,(1-phs)*2/(1-wva)-1),simd::ifelse(phs<wv,1.f,simd::ifelse(phs>=1-wv,-1,1-(phs-wv)*2/(1-wv*2))));
  }
};
#define OVERSMP 16
struct Osc4 : Module {
	enum ParamId {
    FREQ_PARAM,FM_PARAM,LIN_PARAM,WAVE_PARAM,WAVE_CV_PARAM,SYNC_PARAM,PARAMS_LEN
	};
	enum InputId {
    VOCT_INPUT,FM_INPUT,WAVE_CV_INPUT,SYNC_INPUT,INPUTS_LEN
	};
	enum OutputId {
		CV_OUTPUT,OUTPUTS_LEN
	};
	enum LightId {
		LIGHTS_LEN
	};

  FSquareOsc<float_4> osc[4];
  Cheby1_32_BandFilter<float_4> filter24[4];
  DCBlocker<float_4> dcBlocker[4];

  ESeg eseg{{{-.5f,0.3f,0.f},{0.f,0.3f,-5.f},{0.5f,0.4f,5.f},{0.98f,0.5f,0.f}}};
	Osc4() {
		config(PARAMS_LEN, INPUTS_LEN, OUTPUTS_LEN, LIGHTS_LEN);
    configParam(FREQ_PARAM,-4.f,4.f,0.f,"Frequency"," Hz",2,dsp::FREQ_C4);
    configInput(VOCT_INPUT,"V/Oct 1");
    configButton(LIN_PARAM,"Linear");
    configParam(FM_PARAM,0,1,0,"FM Amount","%",0,100);
    //configParam(WAVE_PARAM,-0.5,0.95,0.5,"Wave");
    configParam(WAVE_PARAM,0,1,0.6,"Wave");
    configParam(WAVE_CV_PARAM,0,1,0,"Wave CV"," %",0,100);

    configInput(FM_INPUT,"FM");
    configInput(WAVE_CV_INPUT,"Wave CV");
    configOutput(CV_OUTPUT,"CV");
  }

	void process(const ProcessArgs& args) override {
    float freqParam=params[FREQ_PARAM].getValue();
    float fmParam=params[FM_PARAM].getValue();
    bool linear=params[LIN_PARAM].getValue()>0;
    bool softSync=params[SYNC_PARAM].getValue()>0;
    float wp=params[WAVE_PARAM].getValue();
    if(inputs[WAVE_CV_INPUT].isConnected()) {
      wp+=0.1f*inputs[WAVE_CV_INPUT].getVoltage()*params[WAVE_CV_PARAM].getValue();
    }
    float wave=eseg.process(wp);
    bool syncEnabled=inputs[SYNC_INPUT].isConnected();
    int channels=std::max(inputs[VOCT_INPUT].getChannels(),1);
    for(int c=0;c<channels;c+=4) {
      auto *oscil=&osc[c/4];
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
      float_4 o=0;
      float_4 syncValue=0.f;
      if(syncEnabled) {
        syncValue = inputs[SYNC_INPUT].getPolyVoltageSimd<float_4>(c);
      }
      float_4 fms=args.sampleTime*freq/float(OVERSMP);
      for(int k=0;k<OVERSMP;k++) {
        o=oscil->process(wave,fms,syncEnabled,syncValue,softSync);
        o=filter24[c/4].process(o);
      }
      outputs[CV_OUTPUT].setVoltageSimd(dcBlocker[c/4].process(o*5.f),c);
    }
    outputs[CV_OUTPUT].setChannels(channels);
	}
};


struct Osc4Widget : ModuleWidget {
	Osc4Widget(Osc4* module) {
		setModule(module);
		setPanel(createPanel(asset::plugin(pluginInstance, "res/Osc4.svg")));

    float x=1.9;
    addParam(createParam<TrimbotWhite>(mm2px(Vec(x,9)),module,Osc4::FREQ_PARAM));
    addInput(createInput<SmallPort>(mm2px(Vec(x,21)),module,Osc4::VOCT_INPUT));
    addInput(createInput<SmallPort>(mm2px(Vec(x,33)),module,Osc4::FM_INPUT));
    addParam(createParam<TrimbotWhite>(mm2px(Vec(x,40)),module,Osc4::FM_PARAM));
    addParam(createParam<MLED>(mm2px(Vec(x,52)),module,Osc4::LIN_PARAM));
    addParam(createParam<TrimbotWhite>(mm2px(Vec(x,70)),module,Osc4::WAVE_PARAM));
    addInput(createInput<SmallPort>(mm2px(Vec(x,77)),module,Osc4::WAVE_CV_INPUT));
    addParam(createParam<TrimbotWhite>(mm2px(Vec(x,84)),module,Osc4::WAVE_CV_PARAM));
    addParam(createParam<MLED>(mm2px(Vec(x,96)),module,Osc4::SYNC_PARAM));
    addInput(createInput<SmallPort>(mm2px(Vec(x,104)),module,Osc4::SYNC_INPUT));
    addOutput(createOutput<SmallPort>(mm2px(Vec(x,116)),module,Osc4::CV_OUTPUT));

	}
};


Model* modelOsc4 = createModel<Osc4, Osc4Widget>("Osc4");