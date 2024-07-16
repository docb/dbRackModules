#include "dcb.h"
#include "filter.hpp"
using simd::float_4;

template<typename T>
struct WaveForm {
  T ramp(T phs) {
    return simd::fmod(-phs,1)*2.f-1.f;
  }
  T sine(T phs) {
    return simd::sin(phs*2.f*M_PI);
  }
  T tri(T phs) {
    T p = simd::fmod(phs+0.25f,1);
    return 4.f*simd::fabs(p-simd::round(p))-1.f;
  }
  T sq(T phs) {
    T p = simd::fmod(phs,1);
    return simd::ifelse(p<0.5f,1.f,-1.f);
  }
  T process(T phs, T form) {
    return simd::ifelse(form<=1,sine(phs)*(1.f-form)+tri(phs)*form,
                        simd::ifelse(form<=2,tri(phs)*(1-(form-1.f))+sq(phs)*(form-1.f),
                                     sq(phs)*(1-(form-2.f))+ramp(phs)*(form-2.f)));
  }
};

template<typename T>
struct Env {

  float powf_fast_ub(float a, float b) {
    union { float d; int x; } u = { a };
    u.x = (int)(b * (u.x - 1064631197) + 1065353217);
    return u.d;
  }

  float_4 fastPow(float a,float_4 b) {
    float r[4]={};
    for(int k=0;k<4;k++) {
      r[k]=powf_fast_ub(a,b[k]);
    }
    return {r[0],r[1],r[2],r[3]};
  }

  T gauss(T phs, T param) {
    return fastPow(M_E,-param*(phs-0.5)*(phs-0.5));
  }

  T expFall(T phs, T param) {
    return fastPow(M_E, -param*phs);
  }

  T tri(T phs) {
    return 2.f*simd::fabs(phs-simd::round(phs));
  }

  T process(T phs, int form, T param) {
    switch(form) {
      case 0: return 1;
      case 1: return phs;
      case 2: return expFall(phs,param);
      case 3: return gauss(phs,param);
      case 4: return tri(phs);
      default: return 1;
    }
  }
};
template<typename T>
struct PulsarOsc {
  T phs=0.f;
  WaveForm<T> wf;
  Env<T> env;
  void updatePhs(T fms) {
    phs+=fms;
    phs-=simd::floor(phs);
  }
  T process(T d, int N, T f, int envForm, T envParam) {
    T p=phs/((1.f-d)/float(N));
    return simd::ifelse(p<=float(N),wf.process(p,f)*env.process(p/float(N),envForm,envParam),0.f);
  }
};

struct Pulsar : Module {
  enum ParamId {
    FREQ_PARAM, FINE_PARAM, DUTY_PARAM, DUTY_CV_PARAM, CLUSTER_PARAM, WAVE_PARAM, WAVE_CV_PARAM, ENV_FORM_PARAM, ENV_PARAM, PARAMS_LEN
  };
  enum InputId {
    VOCT_INPUT,DUTY_INPUT, WAVE_INPUT, CLUSTER_INPUT, ENV_FORM_INPUT, ENV_PARAM_INPUT,INPUTS_LEN
  };
  enum OutputId {
    OUTPUT,OUTPUTS_LEN
  };
  enum LightId {
    LIGHTS_LEN
  };

  PulsarOsc<float_4> osc[4];
  Cheby1_32_BandFilter<float_4> filter24[4];

	Pulsar() {
		config(PARAMS_LEN, INPUTS_LEN, OUTPUTS_LEN, LIGHTS_LEN);
    configParam(FINE_PARAM,-100,100,0,"Fine tune"," cents");
    configParam(FREQ_PARAM,-8.f,4.f,0.f,"Frequency"," Hz",2,dsp::FREQ_C4);
    configInput(VOCT_INPUT,"V/Oct");
    configParam(DUTY_PARAM,0,0.99,0,"Duty Cycle");
    configInput(DUTY_INPUT,"Duty");
    configParam(DUTY_CV_PARAM,0,1,0,"D CV","%",0,100);
    configParam(CLUSTER_PARAM,1,8,1,"Cluster");
    getParamQuantity(CLUSTER_PARAM)->snapEnabled=true;
    configInput(CLUSTER_INPUT,"Cluster");
    configParam(WAVE_PARAM,0,3,0,"Waveform");
    configInput(WAVE_INPUT,"Waveform");
    configParam(WAVE_CV_PARAM,0,1,0,"Waveform CV","%",0,100);
    configSwitch(ENV_FORM_PARAM,0,4,0,"Envelope Form",{"Rect","Ramp","Decay","Gauss","Triangle"});
    getParamQuantity(ENV_FORM_PARAM)->snapEnabled=true;
    configInput(ENV_FORM_INPUT,"Envelope Form");
    configParam(ENV_PARAM,1,10,5,"Envelope Param");
    configInput(ENV_PARAM_INPUT,"Envelope Param Input");
    configOutput(OUTPUT,"CV");
	}

	void process(const ProcessArgs& args) override {
    float freqParam=params[FREQ_PARAM].getValue();
    float fineParam=params[FINE_PARAM].getValue();
    float dutyParam=params[DUTY_PARAM].getValue();
    float dutyCVParam=params[DUTY_CV_PARAM].getValue();
    float waveParam=params[WAVE_PARAM].getValue();
    float waveCVParam=params[WAVE_CV_PARAM].getValue();
    if(inputs[CLUSTER_INPUT].isConnected()) {
      setImmediateValue(getParamQuantity(CLUSTER_PARAM),clamp(inputs[CLUSTER_INPUT].getVoltage(),1.f,10.f));
    }
    float cluster = params[CLUSTER_PARAM].getValue();

    if(inputs[ENV_FORM_INPUT].isConnected()) {
      setImmediateValue(getParamQuantity(ENV_FORM_PARAM),clamp(inputs[ENV_FORM_INPUT].getVoltage(),0.f,4.f));
    }
    int envForm = params[ENV_FORM_PARAM].getValue();

    if(inputs[ENV_PARAM_INPUT].isConnected()) {
      setImmediateValue(getParamQuantity(ENV_PARAM),clamp(inputs[ENV_PARAM_INPUT].getVoltage(),1.f,10.f));
    }
    float envParam = params[ENV_PARAM].getValue();

    int channels=std::max(inputs[VOCT_INPUT].getChannels(),1);
    for(int c=0;c<channels;c+=4) {
      float_4 d=dutyParam;
      if(inputs[DUTY_INPUT].isConnected()) {
        d=clamp(d+inputs[DUTY_INPUT].getPolyVoltageSimd<float_4>(c)*dutyCVParam*0.2f,0.01f,1.99f);
      }
      float_4 wave=waveParam;
      if(inputs[WAVE_INPUT].isConnected()) {
        wave=clamp(wave+inputs[WAVE_INPUT].getPolyVoltageSimd<float_4>(c)*waveCVParam*0.3f,0.f,3.f);
      }

      auto *oscil=&osc[c/4];
      float_4 pitch=freqParam+inputs[VOCT_INPUT].getVoltageSimd<float_4>(c)+fineParam/1200.f;
      float_4 freq=dsp::FREQ_C4*dsp::approxExp2_taylor5(pitch+30.f)/std::pow(2.f,30.f);
      float_4 o=0;
      int over=16;
      float_4 fms=args.sampleTime*freq/float(over);
      for(int k=0;k<over;k++) {
        oscil->updatePhs(fms);
        o=oscil->process(d,cluster,wave,envForm,envParam);
        o=filter24[c/4].process(o);
      }
      outputs[OUTPUT].setVoltageSimd(o*5.f,c);
    }
    outputs[OUTPUT].setChannels(channels);
	}
};


struct PulsarWidget : ModuleWidget {
	PulsarWidget(Pulsar* module) {
		setModule(module);
		setPanel(createPanel(asset::plugin(pluginInstance, "res/Pulsar.svg")));
    addParam(createParam<TrimbotWhite9>(mm2px(Vec(1.9,11)),module,Pulsar::FREQ_PARAM));
    addParam(createParam<TrimbotWhite>(mm2px(Vec(12,12)),module,Pulsar::FINE_PARAM));
    addInput(createInput<SmallPort>(mm2px(Vec(3.5,22)),module,Pulsar::VOCT_INPUT));

    float y=37;
    addParam(createParam<TrimbotWhite9>(mm2px(Vec(1.9,y)),module,Pulsar::DUTY_PARAM));
    addInput(createInput<SmallPort>(mm2px(Vec(12,y+5)),module,Pulsar::DUTY_INPUT));
    addParam(createParam<TrimbotWhite>(mm2px(Vec(12,y-2)),module,Pulsar::DUTY_CV_PARAM));

    y=57;
    addParam(createParam<TrimbotWhite9>(mm2px(Vec(1.9,y)),module,Pulsar::WAVE_PARAM));
    addInput(createInput<SmallPort>(mm2px(Vec(12,y+5)),module,Pulsar::WAVE_INPUT));
    addParam(createParam<TrimbotWhite>(mm2px(Vec(12,y-2)),module,Pulsar::WAVE_CV_PARAM));


    addParam(createParam<TrimbotWhite>(mm2px(Vec(3,80)),module,Pulsar::ENV_FORM_PARAM));
    addInput(createInput<SmallPort>(mm2px(Vec(3,87)),module,Pulsar::ENV_FORM_INPUT));

    addParam(createParam<TrimbotWhite>(mm2px(Vec(12,80)),module,Pulsar::ENV_PARAM));
    addInput(createInput<SmallPort>(mm2px(Vec(12,87)),module,Pulsar::ENV_PARAM_INPUT));

    addParam(createParam<TrimbotWhite>(mm2px(Vec(3,102)),module,Pulsar::CLUSTER_PARAM));
    addInput(createInput<SmallPort>(mm2px(Vec(12,102)),module,Pulsar::CLUSTER_INPUT));
    addOutput(createOutput<SmallPort>(mm2px(Vec(8,116)),module,Pulsar::OUTPUT));

	}
};


Model* modelPulsar = createModel<Pulsar, PulsarWidget>("Pulsar");