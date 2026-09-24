#include "dcb.h"
using simd::float_4;


struct SL : Module {
	enum ParamId {
		TIME_PARAM,RATIO_PARAM,PARAMS_LEN
	};
	enum InputId {
		CV_INPUT,TIME_INPUT,RATIO_INPUT,INPUTS_LEN
	};
	enum OutputId {
		CV_OUTPUT,OUTPUTS_LEN
	};
	enum LightId {
		LIGHTS_LEN
	};
	float_4 outVoltage[4] = {};
	static constexpr float minTime = 0.1f;  // ms
	static constexpr float maxTime = 10000.f;  // ms
	dsp::ClockDivider divider;
	SL() {
		config(PARAMS_LEN, INPUTS_LEN, OUTPUTS_LEN, LIGHTS_LEN);
		configParam(TIME_PARAM,0,1.f,0.5f,"Period"," ms",maxTime / minTime, minTime);
		configParam(RATIO_PARAM,0,1,0.5,"Ratio");
		configInput(TIME_INPUT,"V/Oct");
		configInput(CV_INPUT,"CV");
		configInput(RATIO_INPUT,"Ratio");
		configOutput(CV_OUTPUT,"CV");
		divider.setDivision(12000);
	}

	static float_4 convertCVToSec(float_4 cv) {
		return 0.001f * minTime * simd::pow(maxTime / minTime, cv);
	}

	void process(const ProcessArgs& args) override {
		int channels = std::max(inputs[CV_INPUT].getChannels(), inputs[TIME_INPUT].getChannels());
		if(channels==0) channels=1;
		outputs[CV_OUTPUT].setChannels(channels);


		float ratioParam = clamp(params[RATIO_PARAM].getValue(), 0.0001f, 0.9999f);
		for (int c = 0; c < channels; c += 4) {
			float_4 inVoltage = inputs[CV_INPUT].getVoltageSimd<float_4>(c);
			float_4 inTime = inputs[TIME_INPUT].getPolyVoltageSimd<float_4>(c);
			float_4 sec = convertCVToSec(params[TIME_PARAM].getValue()+inTime*0.0625f);
			float_4 ratio = clamp(ratioParam+inputs[RATIO_INPUT].getPolyVoltageSimd<float_4>(c)*0.1f);
			float_4 maxRiseDelta = ((10.0f / sec) / ratio) * args.sampleTime;
			float_4 maxFallDelta = ((10.0f / sec) / (1.0f - ratio)) * args.sampleTime;
			float_4 delta = inVoltage - outVoltage[c / 4];

			float_4 step = simd::ifelse(
					delta > 0.0f,
					simd::fmin(delta, maxRiseDelta),
					simd::fmax(delta, -maxFallDelta)
			);

			outVoltage[c / 4] += step;
			outputs[CV_OUTPUT].setVoltageSimd(outVoltage[c / 4], c);
		}
	}
};


struct SLWidget : ModuleWidget {
	SLWidget(SL* module) {
		setModule(module);
		setPanel(createPanel(asset::plugin(pluginInstance, "res/SL.svg")));
		float x=1.9;
		addParam(createParam<TrimbotWhite>(mm2px(Vec(x,12)),module,SL::TIME_PARAM));
		addInput(createInput<SmallPort>(mm2px(Vec(x,20)),module,SL::TIME_INPUT));
		addParam(createParam<TrimbotWhite>(mm2px(Vec(x,47)),module,SL::RATIO_PARAM));
		addInput(createInput<SmallPort>(mm2px(Vec(x,55)),module,SL::RATIO_INPUT));

		addInput(createInput<SmallPort>(mm2px(Vec(x,104)),module,SL::CV_INPUT));
		addOutput(createOutput<SmallPort>(mm2px(Vec(x,116)),module,SL::CV_OUTPUT));

	}
};


Model* modelSL = createModel<SL, SLWidget>("SL");