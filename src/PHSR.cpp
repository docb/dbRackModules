#include "dcb.h"
using simd::float_4;
template<typename T>
struct PhasorOsc {
  T phs=0.f;

  void updatePhs(float sampleTime,T freq) {
    phs+=freq * sampleTime;
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
struct SinOsc {
  T phs = 0.f;

  void updatePhs(float sampleTime,T freq) {
    phs+=simd::fmin(freq * sampleTime, 0.5f);
    phs-=simd::floor(phs);
  }

  T process() {
    return simd::sin(phs*2*M_PI)*0.5f+0.5f;
  }

  void reset(T trigger) {
    phs = simd::ifelse(trigger, 0.f, phs);
  }
};
template<typename T>
struct TriOsc {
  T phs=0.f;

  void updatePhs(float sampleTime,T freq) {
    phs+=simd::fmin(freq * sampleTime, 0.5f);
    phs-=simd::floor(phs);
  }

  T process() {
    return  4.f * simd::fabs(phs - simd::round(phs)) - 1.f;
  }

  void reset(T trigger) {
    phs = simd::ifelse(trigger, 0.f, phs);
  }
};


struct PHSR : Module {
	enum ParamId {
    FREQ_PARAM,FM_PARAM,LIN_PARAM,PARAMS_LEN
	};
	enum InputId {
    VOCT_INPUT,RST_INPUT,FM_INPUT,INPUTS_LEN
	};
	enum OutputId {
    PHSR_OUTPUT,TRI_OUTPUT,SIN_OUTPUT,OUTPUTS_LEN
	};
	enum LightId {
		LIGHTS_LEN
	};
  SinOsc<float_4> sinOsc[4];
  PhasorOsc<float_4> phsOsc[4];
  TriOsc<float_4> triOsc[4];
  dsp::TSchmittTrigger<float_4> rstTriggers4[4];

	PHSR() {
		config(PARAMS_LEN, INPUTS_LEN, OUTPUTS_LEN, LIGHTS_LEN);
    configParam(FREQ_PARAM,-14.f,4.f,0.f,"Frequency"," Hz",2,dsp::FREQ_C4);
    configParam(FM_PARAM,0,1,0,"FM Amount","%",0,100);
    configInput(FM_INPUT,"FM");
    configButton(LIN_PARAM,"Linear");
    configInput(VOCT_INPUT,"V/Oct");
    configInput(RST_INPUT,"Rst");
    configOutput(PHSR_OUTPUT,"Phasor");
    configOutput(SIN_OUTPUT,"Sin");
    configOutput(TRI_OUTPUT,"Tri");
  }

  float_4 getFreq(int c,float sampleRate) {
    float freqParam = params[FREQ_PARAM].getValue();
    float fmParam=params[FM_PARAM].getValue();
    bool linear = params[LIN_PARAM].getValue()>0;
    float_4 pitch = freqParam + inputs[VOCT_INPUT].getPolyVoltageSimd<float_4>(c);
    float_4 freq;
    if (!linear) {
      pitch += inputs[FM_INPUT].getPolyVoltageSimd<float_4>(c) * fmParam;
      freq = dsp::FREQ_C4 * dsp::approxExp2_taylor5(pitch + 30.f) / std::pow(2.f, 30.f);
    } else {
      freq = dsp::FREQ_C4 * dsp::approxExp2_taylor5(pitch + 30.f) / std::pow(2.f, 30.f);
      freq += dsp::FREQ_C4 * inputs[FM_INPUT].getPolyVoltageSimd<float_4>(c) * fmParam;
    }
    return simd::fmin(freq, sampleRate / 2);
  }

	void process(const ProcessArgs& args) override {
    int channels=std::max(inputs[VOCT_INPUT].getChannels(),1);
    if(outputs[PHSR_OUTPUT].isConnected())
    {
      for(int c=0;c<channels;c+=4) {
        //float_4 pitch = params[FREQ_PARAM].getValue()+inputs[VOCT_INPUT].getVoltageSimd<float_4>(c);
        //float_4 freq = dsp::FREQ_C4 * dsp::approxExp2_taylor5(pitch + 30.f) / std::pow(2.f, 30.f);
        float_4 freq = getFreq(c,args.sampleRate);
        phsOsc[c/4].updatePhs(args.sampleTime,freq);
        float_4 rst = inputs[RST_INPUT].getPolyVoltageSimd<float_4>(c);
        float_4 resetTriggered = rstTriggers4[c/4].process(rst, 0.1f, 2.f);
        phsOsc[c/4].reset(resetTriggered);
        outputs[PHSR_OUTPUT].setVoltageSimd(phsOsc[c/4].process()*10.f-5.f,c);
      }
      outputs[PHSR_OUTPUT].setChannels(channels);
    }
    if(outputs[SIN_OUTPUT].isConnected()) {
      for(int c=0;c<channels;c+=4) {
        //float_4 pitch = params[FREQ_PARAM].getValue()+inputs[VOCT_INPUT].getVoltageSimd<float_4>(c);
        //float_4 freq =  dsp::FREQ_C4 * dsp::approxExp2_taylor5(pitch + 30.f) / std::pow(2.f, 30.f);
        float_4 freq = getFreq(c,args.sampleRate);
        sinOsc[c/4].updatePhs(args.sampleTime,freq);
        float_4 rst = inputs[RST_INPUT].getPolyVoltageSimd<float_4>(c);
        float_4 resetTriggered = rstTriggers4[c/4].process(rst, 0.1f, 2.f);
        sinOsc[c/4].reset(resetTriggered);
        outputs[SIN_OUTPUT].setVoltageSimd(sinOsc[c/4].process()*10.f-5.f,c);
      }
      outputs[SIN_OUTPUT].setChannels(channels);
    }
    if(outputs[TRI_OUTPUT].isConnected()) {
      for(int c=0;c<channels;c+=4) {
        //float_4 pitch = params[FREQ_PARAM].getValue()+inputs[VOCT_INPUT].getVoltageSimd<float_4>(c);
        //float_4 freq =  dsp::FREQ_C4 * dsp::approxExp2_taylor5(pitch + 30.f) / std::pow(2.f, 30.f);
        float_4 freq = getFreq(c,args.sampleRate);
        triOsc[c/4].updatePhs(args.sampleTime,freq);
        float_4 rst = inputs[RST_INPUT].getPolyVoltageSimd<float_4>(c);
        float_4 resetTriggered = rstTriggers4[c/4].process(rst, 0.1f, 2.f);
        triOsc[c/4].reset(resetTriggered);
        outputs[TRI_OUTPUT].setVoltageSimd(triOsc[c/4].process()*5.f,c);
      }
      outputs[TRI_OUTPUT].setChannels(channels);
    }
  }
};


struct PHSRWidget : ModuleWidget {
	PHSRWidget(PHSR* module) {
		setModule(module);
		setPanel(createPanel(asset::plugin(pluginInstance, "res/PHSR.svg")));

		addChild(createWidget<ScrewSilver>(Vec(RACK_GRID_WIDTH, 0)));
		addChild(createWidget<ScrewSilver>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, 0)));
		addChild(createWidget<ScrewSilver>(Vec(RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));
		addChild(createWidget<ScrewSilver>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));

    float x=1.9;
    addParam(createParam<TrimbotWhite>(mm2px(Vec(x,16)),module,PHSR::FREQ_PARAM));
    addInput(createInput<SmallPort>(mm2px(Vec(x,28)),module,PHSR::VOCT_INPUT));
    addInput(createInput<SmallPort>(mm2px(Vec(x,40)),module,PHSR::FM_INPUT));
    addParam(createParam<TrimbotWhite>(mm2px(Vec(x,47)),module,PHSR::FM_PARAM));
    addParam(createParam<MLED>(mm2px(Vec(x,59)),module,PHSR::LIN_PARAM));
    addInput(createInput<SmallPort>(mm2px(Vec(x,71)),module,PHSR::RST_INPUT));
    addOutput(createOutput<SmallPort>(mm2px(Vec(x,88)),module,PHSR::TRI_OUTPUT));
    addOutput(createOutput<SmallPort>(mm2px(Vec(x,100)),module,PHSR::SIN_OUTPUT));
    addOutput(createOutput<SmallPort>(mm2px(Vec(x,112)),module,PHSR::PHSR_OUTPUT));
	}
};


Model* modelPHSR = createModel<PHSR, PHSRWidget>("PHSR");