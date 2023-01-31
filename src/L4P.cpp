#include "dcb.h"

using simd::float_4;
// algorithm taken from csound Copyright (c) Victor Lazzarini, 2021
template<typename T>
struct L4PFilter {
  T s[4];
  T A, G[4];
  T process(T in,T freq, T r,float piosr) {
    T k=r*4;
    T g = simd::tan(freq*piosr);
    G[0] = g/(1+g);
    A = (g-1)/(1+g);
    G[1] = G[0]*G[0];
    G[2] = G[0]*G[1];
    G[3] = G[0]*G[2];
    T ss = s[3];
    for(int j = 0; j < 3; j++) ss += s[j]*G[2-j];
    T out = (G[3]*in + ss)/(1 + k*G[3]);
    T u = G[0]*(in - k*out);
    T w;
    for(int j = 0; j < 3; j++) {
      w = u + s[j];
      s[j] = u - A*w;
      u = G[0]*w;
    }
    s[3] = G[0]*w - A*out;
    return out;
  }
};

struct L4P : Module {
  enum ParamId {
    FREQ_PARAM,FREQ_CV_PARAM,R_PARAM,R_CV_PARAM,PARAMS_LEN
  };
  enum InputId {
    CV_L_INPUT,CV_R_INPUT,FREQ_INPUT,R_INPUT,INPUTS_LEN
  };
  enum OutputId {
    CV_L_OUTPUT,CV_R_OUTPUT,OUTPUTS_LEN
  };
  enum LightId {
    LIGHTS_LEN
  };
  L4PFilter<float_4> filter[4];
  L4PFilter<float_4> filterR[4];

	L4P() {
		config(PARAMS_LEN, INPUTS_LEN, OUTPUTS_LEN, LIGHTS_LEN);
    configParam(FREQ_PARAM,1.f,14.f,14.f,"Frequency"," Hz",2,1);
    configInput(FREQ_INPUT,"Freq");
    configParam(FREQ_CV_PARAM,0,1,0,"Freq CV","%",0,100);
    configParam(R_PARAM,0,1,0,"R");
    configInput(R_INPUT,"R");
    configParam(R_CV_PARAM,0,1,0,"R CV");
    configOutput(CV_L_OUTPUT,"Left");
    configOutput(CV_R_OUTPUT,"Right");
    configInput(CV_L_INPUT,"Left");
    configInput(CV_R_INPUT,"Right");
    configBypass(CV_L_INPUT,CV_L_OUTPUT);
    configBypass(CV_R_INPUT,CV_R_OUTPUT);
	}

	void process(const ProcessArgs& args) override {
    float piosr=M_PI/APP->engine->getSampleRate();
    float r=params[R_PARAM].getValue();
    float rCvParam = params[R_CV_PARAM].getValue();

    float freqParam = params[FREQ_PARAM].getValue();
    float freqCvParam = params[FREQ_CV_PARAM].getValue();
    int channels=inputs[CV_L_INPUT].getChannels();
    int channelsR=inputs[CV_R_INPUT].getChannels();
    if(outputs[CV_L_OUTPUT].isConnected()) {
      for(int c=0;c<channels;c+=4) {
        float_4 pitch=freqParam+inputs[FREQ_INPUT].getPolyVoltageSimd<float_4>(c)*freqCvParam;
        float_4 cutoff=clamp(simd::pow(2.f,pitch),20.f,12000.f);
        float_4 R=simd::clamp(r+inputs[R_INPUT].getPolyVoltageSimd<float_4>(c)*rCvParam*0.1f,0.0f,1.f);
        float_4 in=inputs[CV_L_INPUT].getVoltageSimd<float_4>(c);
        float_4 out=filter[c/4].process(in,cutoff,R,piosr);
        outputs[CV_L_OUTPUT].setVoltageSimd(out,c);
      }
    }
    if(outputs[CV_R_OUTPUT].isConnected()) {
      for(int c=0;c<channelsR;c+=4) {
        float_4 pitch=freqParam+inputs[FREQ_INPUT].getPolyVoltageSimd<float_4>(c)*freqCvParam;
        float_4 cutoff=simd::pow(2.f,pitch);
        float_4 R=simd::clamp(r+inputs[R_INPUT].getPolyVoltageSimd<float_4>(c)*rCvParam*0.1f,0.0f,1.f);
        float_4 in=inputs[CV_R_INPUT].getVoltageSimd<float_4>(c);
        float_4 out=filterR[c/4].process(in,cutoff,R,piosr);
        outputs[CV_R_OUTPUT].setVoltageSimd(out,c);
      }
    }
    outputs[CV_L_OUTPUT].setChannels(channels);
    outputs[CV_R_OUTPUT].setChannels(channelsR);
	}
};


struct L4PWidget : ModuleWidget {
	L4PWidget(L4P* module) {
		setModule(module);
		setPanel(createPanel(asset::plugin(pluginInstance, "res/L4P.svg")));

    float y=9;
    float x=1.9;
    addParam(createParam<TrimbotWhite>(mm2px(Vec(x,y)),module,L4P::FREQ_PARAM));
    y+=7;
    addInput(createInput<SmallPort>(mm2px(Vec(x,y)),module,L4P::FREQ_INPUT));
    y+=7;
    addParam(createParam<TrimbotWhite>(mm2px(Vec(x,y)),module,L4P::FREQ_CV_PARAM));
    y=38;
    addParam(createParam<TrimbotWhite>(mm2px(Vec(x,y)),module,L4P::R_PARAM));
    y+=7;
    addInput(createInput<SmallPort>(mm2px(Vec(x,y)),module,L4P::R_INPUT));
    y+=7;
    addParam(createParam<TrimbotWhite>(mm2px(Vec(x,y)),module,L4P::R_CV_PARAM));

    y=80;
    addInput(createInput<SmallPort>(mm2px(Vec(x,y)),module,L4P::CV_L_INPUT));
    y+=12;
    addInput(createInput<SmallPort>(mm2px(Vec(x,y)),module,L4P::CV_R_INPUT));
    y+=12;
    addOutput(createOutput<SmallPort>(mm2px(Vec(x,y)),module,L4P::CV_L_OUTPUT));
    y+=12;
    addOutput(createOutput<SmallPort>(mm2px(Vec(x,y)),module,L4P::CV_R_OUTPUT));
	}
};


Model* modelL4P = createModel<L4P, L4PWidget>("L4P");