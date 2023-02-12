#include "dcb.h"
#include "Waveshaping.hpp"
using simd::float_4;
template<typename T>
struct Clipper {
  HardClipper<T> hc;
  SoftClipper<T> sc;
  T soft(T drive,T in) {
    T n=in*drive;
    return simd::ifelse(n>1.5f,1.f,simd::ifelse(n<-1.5f,-1.f,n-(4.f/27.f)*n*n*n));
  }

  T hard(T drive,T in) {
    T x=in*drive;
    return simd::ifelse(simd::fabs(x)>1,sgn(x),x);
  }

  T dsin(T drive,T in) {
    T n=in*drive;
    return simd::ifelse(n>2.4674f,1.f,simd::ifelse(n<-2.4674f,-1.f,simd::sin(n*0.63661977f)));
  }

  T overdrive(T drive, T in) {
    T n=in*drive;
    T ab=simd::fabs(n);

    return simd::ifelse(ab<0.333333f,2.f*n,simd::ifelse(ab<0.666666f,sgn(n)*0.33333f*(3.f-(2.f-3.f*ab)*(2.f-3.f*ab)),sgn(n)*1.f));
  }

  T dexp(T drive, T in) {
    T n=in*drive;
    return simd::sgn(n)*(1-simd::exp(-simd::abs(n)));
  }

  T dabs(T drive, T in) {
    T n=in*drive;
    return n/(1+simd::fabs(n));
  }

  T sqr(T drive, T in) {
    T n=in*drive;
    return n/simd::sqrt(1+n*n);
  }

  T hardAA(T drive, T in) {
    T n=in*drive;
    return hc.antialiasedHardClipN1(n);
  }

  T softAA(T drive, T in) {
    T n=in*drive;
    return sc.antialiasedSoftClipN1(n);
  }

  T process(T drive,T in,int mode) {
    switch(mode) {
      case 0: return soft(drive,in);
      case 1: return hard(drive,in);
      case 2: return dsin(drive,in);
      case 3: return overdrive(drive,in);
      case 4: return dexp(drive,in);
      case 5: return sqr(drive,in);
      case 6: return dabs(drive,in);
      case 7: return hardAA(drive,in);
      default: return softAA(drive,in);
    }
  }
};

struct CLP : Module {
	enum ParamId {
		TYPE_PARAM,DRIVE_PARAM,DRIVE_CV_PARAM,PARAMS_LEN
	};
	enum InputId {
		L_INPUT,R_INPUT,DRIVE_INPUT,INPUTS_LEN
	};
	enum OutputId {
		L_OUTPUT,R_OUTPUT,OUTPUTS_LEN
	};
	enum LightId {
		LIGHTS_LEN
	};

  Clipper<float_4> clipper[4];

	CLP() {
		config(PARAMS_LEN, INPUTS_LEN, OUTPUTS_LEN, LIGHTS_LEN);
    configInput(L_INPUT,"Left");
    configInput(R_INPUT,"Right");
    configOutput(L_OUTPUT,"Left");
    configOutput(R_OUTPUT,"Right");
    configParam(DRIVE_PARAM,0,10,1,"Drive");
    configParam(DRIVE_CV_PARAM,0,1,0,"Drive CV"," %",0,100);
    configSwitch(TYPE_PARAM,0,8,0,"Type",{"Tanh","Hard Clip","Sin","Overdrive","Exp","Sqr","Abs","AA Hard","AA Soft"});
    configBypass(L_INPUT,L_OUTPUT);
    configBypass(R_INPUT,R_OUTPUT);
	}

	void process(const ProcessArgs& args) override {
    int channels=inputs[L_INPUT].getChannels();
    float driveParam=params[DRIVE_PARAM].getValue();
    int mode=params[TYPE_PARAM].getValue();
    for(int c=0;c<channels;c+=4) {
      float_4 inL=inputs[L_INPUT].getVoltageSimd<float_4>(c)*0.2f;
      float_4 inR=inputs[R_INPUT].getVoltageSimd<float_4>(c)*0.2f;
      float_4 drive=simd::clamp(driveParam+inputs[DRIVE_INPUT].getPolyVoltageSimd<float_4>(c)*params[DRIVE_CV_PARAM].getValue(),0.f,10.f);
      outputs[L_OUTPUT].setVoltageSimd(clipper[c/4].process(drive,inL,mode)*5.f,c);
      outputs[R_OUTPUT].setVoltageSimd(clipper[c/4].process(drive,inR,mode)*5.f,c);
    }
    outputs[L_OUTPUT].setChannels(channels);
    outputs[R_OUTPUT].setChannels(channels);

	}
};


struct CLPWidget : ModuleWidget {
	CLPWidget(CLP* module) {
		setModule(module);
		setPanel(createPanel(asset::plugin(pluginInstance, "res/CLP.svg")));
    float x=1.9;
    auto modeParam=createParam<SelectParam>(mm2px(Vec(1.2,10)),module,CLP::TYPE_PARAM);
    modeParam->box.size=Vec(22,99);
    modeParam->init({"Soft","Hard","Sin","OD","Exp","Sqr","Abs","AAH","AAS"});
    addParam(modeParam);
    float y=48;
    addParam(createParam<TrimbotWhite>(mm2px(Vec(x,y)),module,CLP::DRIVE_PARAM));
    y+=8;
    addInput(createInput<SmallPort>(mm2px(Vec(x,y)),module,CLP::DRIVE_INPUT));
    y+=8;
    addParam(createParam<TrimbotWhite>(mm2px(Vec(x,y)),module,CLP::DRIVE_CV_PARAM));
    y=80;
    addInput(createInput<SmallPort>(mm2px(Vec(x,y)),module,CLP::L_INPUT));
    y+=12;
    addInput(createInput<SmallPort>(mm2px(Vec(x,y)),module,CLP::R_INPUT));
    y+=12;
    addOutput(createOutput<SmallPort>(mm2px(Vec(x,y)),module,CLP::L_OUTPUT));
    y+=12;
    addOutput(createOutput<SmallPort>(mm2px(Vec(x,y)),module,CLP::R_OUTPUT));
	}
};


Model* modelCLP = createModel<CLP, CLPWidget>("CLP");