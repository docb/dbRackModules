#include "dcb.h"
#include "Shaper.hpp"
#include "filter.hpp"
#define OVERSMP 16
using simd::float_4;
template<typename T>
struct PhsOsc {
  T phs=0.f;

  void updatePhs(T fms) {
    phs+=fms;
    phs-=simd::floor(phs);
  }

  T process() {
    return phs;
  }

  void reset(T trigger) {
    phs = simd::ifelse(trigger, 0.f, phs);
  }
};
template<typename T>
struct SOsc {
  T phs = 0.f;
  float fdp;
  float pih=M_PI/2;
  SOsc() {
    fdp=4/powf(float(M_PI),2);
  }
  void updatePhs(T fms) {
    phs+=simd::fmin(fms, 0.5f);
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

  T process() {
    return simd::ifelse(phs<0.5f,firstHalf(phs),secondHalf(phs));
  }

  void reset(T trigger) {
    phs = simd::ifelse(trigger, 0.f, phs);
  }
};
struct Osc7 : Module {
  enum ParamId {
    FREQ_PARAM,FM_PARAM,LIN_PARAM,SHAPE_PARAM,SHAPE_CV_PARAM,SHAPE_TYPE_PARAM,PARAMS_LEN
  };
  enum InputId {
    VOCT_INPUT,FM_INPUT,SHAPE_PARAM_INPUT,INPUTS_LEN
  };
  enum OutputId {
    OUTPUT,OUTPUTS_LEN
  };
	enum LightId {
		LIGHTS_LEN
	};
  Cheby1_32_BandFilter<float_4> filters[4];
  DCBlocker<float_4> dcBlocker[4];
  PhsOsc<float_4> osc[4];
  SOsc<float_4> sosc[4];
  Shaper shaper;
  int shaperMap[4]={Shaper::WRAP_MODE,Shaper::PULSE_MODE,Shaper::BUZZ_X8_MODE,Shaper::WRINKLE_X2_MODE};
	Osc7() {
		config(PARAMS_LEN, INPUTS_LEN, OUTPUTS_LEN, LIGHTS_LEN);
    configParam(SHAPE_PARAM,0,4,0,"Depth");
    configParam(SHAPE_CV_PARAM,0,1.f,0,"Depth CV");
    configSwitch(SHAPE_TYPE_PARAM,0,7,0,"Shape Type",{"WRAP","PULSE","BUZZ","WRINKLE","SIN_WRAP","SIN_PULSE","SIN_BUZZ","SIN_WRINKLE"});
    configInput(SHAPE_PARAM_INPUT,"Depth");
    configParam(FREQ_PARAM,-4.f,4.f,0.f,"Frequency"," Hz",2,dsp::FREQ_C4);
    configInput(VOCT_INPUT,"V/Oct 1");
    configButton(LIN_PARAM,"Linear");
    configParam(FM_PARAM,0,1,0,"FM Amount","%",0,100);
    configInput(FM_INPUT,"FM");
    configOutput(OUTPUT,"CV");

  }

	void process(const ProcessArgs& args) override {
    float freqParam=params[FREQ_PARAM].getValue();
    float fmParam=params[FM_PARAM].getValue();
    bool linear=params[LIN_PARAM].getValue()>0;
    int idx=int(params[SHAPE_TYPE_PARAM].getValue());
    shaper.setShapeMode(shaperMap[idx%4]);
    float _shapeParam = params[SHAPE_PARAM].getValue();
    float _shapeParamCV = params[SHAPE_CV_PARAM].getValue();

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
      float_4 o=0;
      float_4 fms=args.sampleTime*freq/float(OVERSMP);
      float_4 shapeParam = simd::clamp((inputs[SHAPE_PARAM_INPUT].getPolyVoltageSimd<float_4>(c)/10.f)*_shapeParamCV+_shapeParam,0.f,4.f);
      for(int k=0;k<OVERSMP;k++) {
        if(idx>3) {
          sosc[c/4].updatePhs(fms);
          o=sosc[c/4].process();
        } else {
          osc[c/4].updatePhs(fms);
          o=osc[c/4].process();
        }
        o = shaper.process(o.v,shapeParam.v);
        o = simd::clamp(((o*2.f)-1.f)*5.f,-5.f,5.f);
        o=filters[c/4].process(o);
      }
      float_4 out=dcBlocker[c/4].process(o);
      //float_4 out=o;
      outputs[OUTPUT].setVoltageSimd(out,c);
    }
    outputs[OUTPUT].setChannels(channels);
	}
};


struct Osc7Widget : ModuleWidget {
	Osc7Widget(Osc7* module) {
		setModule(module);
		setPanel(createPanel(asset::plugin(pluginInstance, "res/Osc7.svg")));
    float x=1.9f;
    addParam(createParam<TrimbotWhite>(mm2px(Vec(x,9)),module,Osc7::FREQ_PARAM));
    addInput(createInput<SmallPort>(mm2px(Vec(x,21)),module,Osc7::VOCT_INPUT));
    addInput(createInput<SmallPort>(mm2px(Vec(x,33)),module,Osc7::FM_INPUT));
    addParam(createParam<TrimbotWhite>(mm2px(Vec(x,40)),module,Osc7::FM_PARAM));
    addParam(createParam<MLED>(mm2px(Vec(x,52)),module,Osc7::LIN_PARAM));
    addParam(createParam<TrimbotWhite>(mm2px(Vec(x,74)),module,Osc7::SHAPE_TYPE_PARAM));
    addParam(createParam<TrimbotWhite>(mm2px(Vec(x,86)),module,Osc7::SHAPE_PARAM));
    addInput(createInput<SmallPort>(mm2px(Vec(x,94)),module,Osc7::SHAPE_PARAM_INPUT));
    addParam(createParam<TrimbotWhite>(mm2px(Vec(x,102)),module,Osc7::SHAPE_CV_PARAM));
    addOutput(createOutput<SmallPort>(mm2px(Vec(x,116)),module,Osc7::OUTPUT));
  }
};


Model* modelOsc7 = createModel<Osc7, Osc7Widget>("Osc7");