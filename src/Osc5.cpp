#include "dcb.h"
#include "filter.hpp"
using simd::float_4;

struct PulseOsc {
  float phs=0.f;
  int stage=0;
  void updatePhs(float sampleTime,float freq) {
    phs+=sampleTime*(freq);
    phs-=std::floor(phs);
  }
  float process(float bp=0.5f,float bp1=0.5f) {

    float o=0.f;
    float bp01=bp*bp1;
    float bp02=bp+(1-bp)*bp1;
    switch(stage) {
      case 0:
        if(phs<bp01) {
          stage++;
          o=10;
        }
        break;
      case 1:
        if(phs>=bp01&&phs<bp) {
          stage++;
          o=5.f;
        }
        break;
      case 2:
        if(phs>=bp&&phs<bp02) {
          stage++;
          o=-10.f;
        }
        break;
      case 3:
        if(phs>=bp02&&phs<=1) {
          stage=0;
          o=-5.f;
        }
        break;
    }
    return o;
  }
};
struct PulseOsc4 {
  PulseOsc osc[4];
  void updatePhs(float sampleTime,float_4 freq) {
    for(int k=0;k<4;k++) {
      osc[k].updatePhs(sampleTime,freq[k]);
    }
  }
  float_4 process(float_4 bp1,float_4 bp2) {
    float ret[4];
    for(int k=0;k<4;k++) {
      ret[k]=osc[k].process(bp1[k],bp2[k]);
    }
    return {ret[0],ret[1],ret[2],ret[3]};
  }
};

struct Osc5 : Module {
	enum ParamId {
    FREQ_PARAM,FM_PARAM,LIN_PARAM,BP1_PARAM,BP1_CV_PARAM,BP2_PARAM,BP2_CV_PARAM,PARAMS_LEN
	};
	enum InputId {
    VOCT_INPUT,FM_INPUT,BP1_INPUT,BP2_INPUT,INPUTS_LEN
	};
	enum OutputId {
		CV_OUTPUT,OUTPUTS_LEN
	};
	enum LightId {
		LIGHTS_LEN
	};

  PulseOsc4 osc[4];
  Cheby1_32_BandFilter<float_4> filter24[4];
	Osc5() {
		config(PARAMS_LEN, INPUTS_LEN, OUTPUTS_LEN, LIGHTS_LEN);
    configParam(FREQ_PARAM,-14.f,4.f,0.f,"Frequency"," Hz",2,dsp::FREQ_C4);
    configInput(VOCT_INPUT,"V/Oct 1");
    configButton(LIN_PARAM,"Linear");
    configParam(FM_PARAM,0,1,0,"FM Amount","%",0,100);
    configParam(BP1_PARAM,0.01,0.98,0.5,"Breakpoint 1");
    configInput(BP1_INPUT,"Wave CV");
    configParam(BP1_CV_PARAM,0,0.1,0,"Breakpoint 1 CV");
    configParam(BP2_PARAM,0.01,0.98,0.5,"Breakpoint 2");
    configInput(BP2_INPUT,"Wave CV");
    configParam(BP2_CV_PARAM,0,0.1,0,"Breakpoint 2 CV");
    configInput(FM_INPUT,"FM");
    configOutput(CV_OUTPUT,"CV");
	}

	void process(const ProcessArgs& args) override {
    float freqParam=params[FREQ_PARAM].getValue();
    float fmParam=params[FM_PARAM].getValue();
    bool linear=params[LIN_PARAM].getValue()>0;
    float bp1f=params[BP1_PARAM].getValue();
    float bp2f=params[BP2_PARAM].getValue();
    int channels=std::max(inputs[VOCT_INPUT].getChannels(),1);
    for(int c=0;c<channels;c+=4) {
      float_4 bp1=bp1f;
      if(inputs[BP1_INPUT].isConnected()) {
        bp1=clamp(bp1+inputs[BP1_INPUT].getPolyVoltageSimd<float_4>(c)*params[BP1_CV_PARAM].getValue(),0.01f,0.99f);
      }
      float_4 bp2=bp2f;
      if(inputs[BP2_INPUT].isConnected()) {
        bp2=clamp(bp2+inputs[BP2_INPUT].getPolyVoltageSimd<float_4>(c)*params[BP2_CV_PARAM].getValue(),0.01f,0.99f);
      }
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
      int over=16;
      for(int k=0;k<over;k++) {
        oscil->updatePhs(args.sampleTime,freq/float(over));
        o=oscil->process(bp1,bp2);
        o=filter24[c/4].process(o);
      }
      outputs[CV_OUTPUT].setVoltageSimd(o*10.f,c);
    }
    outputs[CV_OUTPUT].setChannels(channels);
	}
};


struct Osc5Widget : ModuleWidget {
	Osc5Widget(Osc5* module) {
		setModule(module);
		setPanel(createPanel(asset::plugin(pluginInstance, "res/Osc5.svg")));
    float x=1.9;
    addParam(createParam<TrimbotWhite>(mm2px(Vec(x,9)),module,Osc5::FREQ_PARAM));
    addInput(createInput<SmallPort>(mm2px(Vec(x,21)),module,Osc5::VOCT_INPUT));
    addInput(createInput<SmallPort>(mm2px(Vec(x,33)),module,Osc5::FM_INPUT));
    addParam(createParam<TrimbotWhite>(mm2px(Vec(x,40)),module,Osc5::FM_PARAM));
    addParam(createParam<MLED>(mm2px(Vec(x,52)),module,Osc5::LIN_PARAM));
    addParam(createParam<TrimbotWhite>(mm2px(Vec(x,64)),module,Osc5::BP1_PARAM));
    addInput(createInput<SmallPort>(mm2px(Vec(x,71)),module,Osc5::BP1_INPUT));
    addParam(createParam<TrimbotWhite>(mm2px(Vec(x,78)),module,Osc5::BP1_CV_PARAM));
    addParam(createParam<TrimbotWhite>(mm2px(Vec(x,90)),module,Osc5::BP2_PARAM));
    addInput(createInput<SmallPort>(mm2px(Vec(x,97)),module,Osc5::BP2_INPUT));
    addParam(createParam<TrimbotWhite>(mm2px(Vec(x,104)),module,Osc5::BP2_CV_PARAM));
    addOutput(createOutput<SmallPort>(mm2px(Vec(x,116)),module,Osc5::CV_OUTPUT));

	}
};


Model* modelOsc5 = createModel<Osc5, Osc5Widget>("Osc5");