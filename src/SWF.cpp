#include "dcb.h"

#include "Waveshaping.hpp"

using simd::float_4;

struct SWF : Module {
	enum ParamId {
		GAIN_PARAM,GAIN_CV_PARAM,SYM_PARAM,SYM_CV_PARAM,PARAMS_LEN
	};
	enum InputId {
		L_INPUT,R_INPUT,GAIN_INPUT,SYM_INPUT,INPUTS_LEN
	};
	enum OutputId {
		L_OUTPUT,R_OUTPUT,OUTPUTS_LEN
	};
	enum LightId {
		LIGHTS_LEN
	};

  DCBlocker<float_4> dcbL[4];
  DCBlocker<float_4> dcbR[4];
  HardClipper<float_4> hcL[4];
  HardClipper<float_4> hcR[4];
  Wavefolder<float_4> wfL[4];
  Wavefolder<float_4> wfR[4];

	SWF() {
		config(PARAMS_LEN, INPUTS_LEN, OUTPUTS_LEN, LIGHTS_LEN);
    configInput(L_INPUT, "Left");
    configOutput(L_OUTPUT, "Left");
    //configBypass(L_INPUT, L_OUTPUT);
    configInput(R_INPUT, "Right");
    configOutput(R_OUTPUT, "Right");
    //configBypass(R_INPUT, R_OUTPUT);

    configInput(GAIN_INPUT, "Gain");
    configInput(SYM_INPUT, "Symmetry");
    configParam(GAIN_PARAM, 0.9f, 10.f, 0.9f, "Folds");
    configParam(GAIN_CV_PARAM, -1.0f, 1.0f, 0.0f, "Folds CV");
    configParam(SYM_PARAM, -5.0f, 5.0f, 0.0f, "Symmetry");
    configParam(SYM_CV_PARAM, -1.0f, 1.0f, 0.0f, "Symmetry CV");
  }

  void process(const ProcessArgs& args) override {
    if(inputs[L_INPUT].isConnected() && outputs[L_OUTPUT].isConnected()) {
      int channels=inputs[L_INPUT].getChannels();
      for(int c=0;c<channels;c+=4) {
        float_4 input = 0.2f * inputs[L_INPUT].getVoltageSimd<float_4>(c);
        float_4 foldLevel = simd::clamp(params[GAIN_PARAM].getValue() + params[GAIN_CV_PARAM].getValue()*simd::fabs(inputs[GAIN_INPUT].getPolyVoltageSimd<float_4>(c)), -10.0f, 10.0f);
        float_4 symLevel = simd::clamp(params[SYM_PARAM].getValue() + 0.5f*params[SYM_CV_PARAM].getValue()*inputs[SYM_INPUT].getPolyVoltageSimd<float_4>(c),-5.f,5.f);
        float_4 foldedOutput = input*foldLevel + symLevel;
        for (int i=0; i<4; i++) {
          foldedOutput=wfL[c/4].process(foldedOutput);
        }
        foldedOutput=hcL[c/4].process(foldedOutput);
        outputs[L_OUTPUT].setVoltageSimd(5.0f * dcbL[c/4].process(foldedOutput),c);
      }
      outputs[L_OUTPUT].setChannels(channels);
    }

    if(inputs[R_INPUT].isConnected() && outputs[R_OUTPUT].isConnected()) {
      int channels=inputs[R_INPUT].getChannels();
      for(int c=0;c<channels;c+=4) {
        float_4 input = 0.2f * inputs[R_INPUT].getVoltageSimd<float_4>(c);
        float_4 foldLevel = simd::clamp(params[GAIN_PARAM].getValue() + params[GAIN_CV_PARAM].getValue()*simd::fabs(inputs[GAIN_INPUT].getPolyVoltageSimd<float_4>(c)), -10.0f, 10.0f);
        float_4 symLevel = simd::clamp(params[SYM_PARAM].getValue() + 0.5f*params[SYM_CV_PARAM].getValue()*inputs[SYM_INPUT].getPolyVoltageSimd<float_4>(c),-5.f,5.f);
        float_4 foldedOutput = input*foldLevel + symLevel;
        for (int i=0; i<4; i++) {
          foldedOutput=wfR[c/4].process(foldedOutput);
        }
        foldedOutput=hcR[c/4].process(foldedOutput);
        outputs[R_OUTPUT].setVoltageSimd(5.0f * dcbR[c/4].process(foldedOutput),c);
      }
      outputs[R_OUTPUT].setChannels(channels);
    }
  }
};


struct SWFWidget : ModuleWidget {
	SWFWidget(SWF* module) {
		setModule(module);
		setPanel(createPanel(asset::plugin(pluginInstance, "res/SWF.svg")));
    float y=9;
    float x=1.9;
    addParam(createParam<TrimbotWhite>(mm2px(Vec(x,y)),module,SWF::GAIN_PARAM));
    y+=7;
    addInput(createInput<SmallPort>(mm2px(Vec(x,y)),module,SWF::GAIN_INPUT));
    y+=7;
    addParam(createParam<TrimbotWhite>(mm2px(Vec(x,y)),module,SWF::GAIN_CV_PARAM));
    y=38;
    addParam(createParam<TrimbotWhite>(mm2px(Vec(x,y)),module,SWF::SYM_PARAM));
    y+=7;
    addInput(createInput<SmallPort>(mm2px(Vec(x,y)),module,SWF::SYM_INPUT));
    y+=7;
    addParam(createParam<TrimbotWhite>(mm2px(Vec(x,y)),module,SWF::SYM_CV_PARAM));

    y=80;
    addInput(createInput<SmallPort>(mm2px(Vec(x,y)),module,SWF::L_INPUT));
    y+=12;
    addInput(createInput<SmallPort>(mm2px(Vec(x,y)),module,SWF::R_INPUT));
    y+=12;
    addOutput(createOutput<SmallPort>(mm2px(Vec(x,y)),module,SWF::L_OUTPUT));
    y+=12;
    addOutput(createOutput<SmallPort>(mm2px(Vec(x,y)),module,SWF::R_OUTPUT));
	}
};


Model* modelSWF = createModel<SWF, SWFWidget>("SWF");