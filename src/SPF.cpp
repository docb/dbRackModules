#include "dcb.h"
using simd::float_4;
// algorithm taken from csound Copyright (c) Victor Lazzarini, 2021
template<typename T>
struct SPFFilter {
  T s[2],sl[2],sh[2],sb[2];
  T al[2],ah[2],ab,b[2];

  T process(T lpIn,T bpIn,T hpIn,T freq, T R,float piosr) {
    T w = simd::tan(freq*piosr);
    T w2 = w*w;
    T fac = 1./(1. + R*w + w2);
    al[0] = w2*fac;
    al[1] = 2*w2*fac;
    ah[0] = fac;
    ah[1] = -2*fac;
    ab = w*fac*R;
    b[0] = -2*(1 - w2)*fac;
    b[1] = (1. - R*w + w2)*fac;

    T x = hpIn*ah[0] + sh[0]*ah[1] + sh[1]*ah[0];
    sh[1] = sh[0];
    sh[0] = hpIn;
    x += lpIn*al[0] + sl[0]*al[1] + sl[1]*al[0];
    sl[1] = sl[0];
    sl[0] = lpIn;
    x += (bpIn - sb[1])*ab;
    sb[1] = sb[0];
    sb[0] = bpIn;
    T ret = x - b[0]*s[0] - b[1]*s[1];
    s[1] = s[0];
    s[0] = ret;
    return ret;
  }

};

struct SPF : Module {
	enum ParamId {
		FREQ_PARAM,FREQ_CV_PARAM,R_PARAM,R_CV_PARAM,PARAMS_LEN
	};
	enum InputId {
		LP_INPUT,BP_INPUT,HP_INPUT,R_INPUT,FREQ_INPUT,INPUTS_LEN
	};
	enum OutputId {
		CV_OUTPUT,OUTPUTS_LEN
	};
	enum LightId {
		LIGHTS_LEN
	};
  SPFFilter<float_4> filter[4];
	SPF() {
		config(PARAMS_LEN, INPUTS_LEN, OUTPUTS_LEN, LIGHTS_LEN);
    configParam(FREQ_PARAM,4.f,14.f,10.f,"Frequency"," Hz",2,1);
    configParam(R_PARAM,0,2,1,"R");
    configParam(R_CV_PARAM,0,1,0,"R CV");
    configInput(R_INPUT,"R");
    configInput(LP_INPUT,"Low Pass");
    configInput(BP_INPUT,"Band Pass");
    configInput(HP_INPUT,"High Pass");
    configInput(FREQ_INPUT,"Freq");
    configParam(FREQ_CV_PARAM,0,1,0,"Freq CV","%",0,100);
    configOutput(CV_OUTPUT,"CV");
    configBypass(LP_INPUT,CV_OUTPUT);
	}

	void process(const ProcessArgs& args) override {
    float piosr=M_PI/APP->engine->getSampleRate();
    float r=params[R_PARAM].getValue();
    float rCvParam = params[R_CV_PARAM].getValue();

    float freqParam = params[FREQ_PARAM].getValue();
    float freqCvParam = params[FREQ_CV_PARAM].getValue();
    int channels=inputs[LP_INPUT].getChannels();
    if(inputs[BP_INPUT].isConnected() && inputs[BP_INPUT].getChannels()>channels) channels=inputs[BP_INPUT].getChannels();
    if(inputs[HP_INPUT].isConnected() && inputs[HP_INPUT].getChannels()>channels) channels=inputs[HP_INPUT].getChannels();
    for(int c=0;c<channels;c+=4) {
      float_4 pitch = freqParam + inputs[FREQ_INPUT].getPolyVoltageSimd<float_4>(c) * freqCvParam;
      float_4 cutoff=simd::clamp(simd::pow(2.f,pitch),2.f,args.sampleRate*0.33f);
      float_4 R = 2.f-simd::clamp(r + inputs[R_INPUT].getPolyVoltageSimd<float_4>(c)*rCvParam,0.0f,1.999f);
      float_4 lpInput=inputs[LP_INPUT].getVoltageSimd<float_4>(c);
      float_4 bpInput=inputs[BP_INPUT].getVoltageSimd<float_4>(c);
      float_4 hpInput=inputs[HP_INPUT].getVoltageSimd<float_4>(c);
      float_4 out = filter[c/4].process(lpInput,bpInput,hpInput,cutoff,R,piosr);
      outputs[CV_OUTPUT].setVoltageSimd(out,c);
    }
    outputs[CV_OUTPUT].setChannels(channels);
	}
};


struct SPFWidget : ModuleWidget {
	SPFWidget(SPF* module) {
		setModule(module);
		setPanel(createPanel(asset::plugin(pluginInstance, "res/SPF.svg")));

		float y=9;
    float x=1.9;
    addParam(createParam<TrimbotWhite>(mm2px(Vec(x,y)),module,SPF::FREQ_PARAM));
    y+=7;
    addInput(createInput<SmallPort>(mm2px(Vec(x,y)),module,SPF::FREQ_INPUT));
    y+=7;
    addParam(createParam<TrimbotWhite>(mm2px(Vec(x,y)),module,SPF::FREQ_CV_PARAM));
    y=38;
    addParam(createParam<TrimbotWhite>(mm2px(Vec(x,y)),module,SPF::R_PARAM));
    y+=7;
    addInput(createInput<SmallPort>(mm2px(Vec(x,y)),module,SPF::R_INPUT));
    y+=7;
    addParam(createParam<TrimbotWhite>(mm2px(Vec(x,y)),module,SPF::R_CV_PARAM));

    y=80;
    addInput(createInput<SmallPort>(mm2px(Vec(x,y)),module,SPF::LP_INPUT));
    y+=12;
    addInput(createInput<SmallPort>(mm2px(Vec(x,y)),module,SPF::BP_INPUT));
    y+=12;
    addInput(createInput<SmallPort>(mm2px(Vec(x,y)),module,SPF::HP_INPUT));
    y+=12;
    addOutput(createOutput<SmallPort>(mm2px(Vec(x,y)),module,SPF::CV_OUTPUT));

  }
};


Model* modelSPF = createModel<SPF, SPFWidget>("SPF");