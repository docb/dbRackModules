#include "dcb.h"
using simd::float_4;
template<typename T>
struct PhasorOsc {
  T phs=0.f;

  void updatePhs(float sampleTime,T freq) {
    phs+=simd::fmin(freq * sampleTime, 0.5f);
    phs-=simd::trunc(phs);
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
    phs-=simd::trunc(phs);
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
    phs-=simd::trunc(phs);
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
    FREQ_PARAM,PARAMS_LEN
	};
	enum InputId {
    VOCT_INPUT,RST_INPUT,INPUTS_LEN
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
    configInput(VOCT_INPUT,"V/Oct");
    configInput(RST_INPUT,"Rst");
    configOutput(PHSR_OUTPUT,"Phasor");
    configOutput(SIN_OUTPUT,"Sin");
    configOutput(TRI_OUTPUT,"Tri");
  }

	void process(const ProcessArgs& args) override {
    int channels=std::max(inputs[VOCT_INPUT].getChannels(),1);
    if(outputs[PHSR_OUTPUT].isConnected())
    {
      for(int c=0;c<channels;c+=4) {
        float_4 pitch = params[FREQ_PARAM].getValue()+inputs[VOCT_INPUT].getVoltageSimd<float_4>(c);
        float_4 freq = dsp::FREQ_C4 * dsp::approxExp2_taylor5(pitch + 30.f) / std::pow(2.f, 30.f);
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
        float_4 pitch = params[FREQ_PARAM].getValue()+inputs[VOCT_INPUT].getVoltageSimd<float_4>(c);
        float_4 freq =  dsp::FREQ_C4 * dsp::approxExp2_taylor5(pitch + 30.f) / std::pow(2.f, 30.f);
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
        float_4 pitch = params[FREQ_PARAM].getValue()+inputs[VOCT_INPUT].getVoltageSimd<float_4>(c);
        float_4 freq =  dsp::FREQ_C4 * dsp::approxExp2_taylor5(pitch + 30.f) / std::pow(2.f, 30.f);
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
    addParam(createParam<TrimbotWhite>(mm2px(Vec(x,MHEIGHT-112)),module,PHSR::FREQ_PARAM));
    addInput(createInput<SmallPort>(mm2px(Vec(x,MHEIGHT-100)),module,PHSR::VOCT_INPUT));
    addInput(createInput<SmallPort>(mm2px(Vec(x,MHEIGHT-88)),module,PHSR::RST_INPUT));
    addOutput(createOutput<SmallPort>(mm2px(Vec(x,MHEIGHT-42)),module,PHSR::TRI_OUTPUT));
    addOutput(createOutput<SmallPort>(mm2px(Vec(x,MHEIGHT-30)),module,PHSR::SIN_OUTPUT));
    addOutput(createOutput<SmallPort>(mm2px(Vec(x,MHEIGHT-18)),module,PHSR::PHSR_OUTPUT));
	}
};


Model* modelPHSR = createModel<PHSR, PHSRWidget>("PHSR");