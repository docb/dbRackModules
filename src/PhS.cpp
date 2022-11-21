#include "dcb.h"
#include "Shaper.hpp"
using simd::float_4;
struct PhS : Module {
  enum ParamId {
    SHAPE_PARAM,SHAPE_CV_PARAM,SHAPE_TYPE_PARAM,PARAMS_LEN
  };
  enum InputId {
    SHAPE_PARAM_INPUT,INPUT,SHAPE_TYPE_INPUT,INPUTS_LEN
  };
  enum OutputId {
    OUTPUT,OUTPUTS_LEN
  };
  enum LightId {
    LIGHTS_LEN
  };
  Shaper shaper;

	PhS() {
		config(PARAMS_LEN, INPUTS_LEN, OUTPUTS_LEN, LIGHTS_LEN);
    configParam(SHAPE_PARAM,0,0.99,0.5,"Depth");
    configParam(SHAPE_PARAM,-1,1.f,0.5,"Shape param CV");
    configSwitch(SHAPE_TYPE_PARAM,0,Shaper::NUM_MODES-1,0,"Shape Type",{"BEND","TILT","LEAN","TWIST","WRAP","MIRROR","REFLECT","PULSE","STEP4","STEP8","STEP16","VARSTEP","SINEWRAP","HARMONICS","BUZZ_X2","BUZZ_X4","BUZZ_X8","WRINKLE_X2","WRINKLE_X4","WRINKLE_X8","SINE_DOWN_X2","SINE_DOWN_X4","SINE_DOWN_X8","SINE_UP_X2","SINE_UP_X4","SINE_UP_X8"});
    configInput(SHAPE_PARAM_INPUT,"Shape Param");
    configInput(SHAPE_TYPE_INPUT,"Shape Type");
    configInput(INPUT,"Phs");
    configOutput(OUTPUT,"Phs");
	}

	void process(const ProcessArgs& args) override {
    shaper.setShapeMode(params[SHAPE_TYPE_PARAM].getValue());
    int channels = inputs[INPUT].getChannels();
    float _shapeParam = params[SHAPE_PARAM].getValue();
    float _shapeParamCV = params[SHAPE_CV_PARAM].getValue();
    for(int c=0;c<channels;c+=4) {
      float_4 shapeParam = simd::clamp(inputs[SHAPE_PARAM_INPUT].getPolyVoltageSimd<float_4>(c)/10.f*_shapeParamCV+_shapeParam,-1.f,1.f);
      float_4 in = inputs[INPUT].getVoltageSimd<float_4>(c)/5.f;
      in = (in+1.f)/2.f;
      float_4 out = shaper.process(in.v,shapeParam.v);
      out = ((out*2.f)-1.f)*5.f;
      outputs[OUTPUT].setVoltageSimd(simd::clamp(out,-5.f,5.f),c);
    }
    outputs[OUTPUT].setChannels(channels);
	}
};


struct PhSWidget : ModuleWidget {
	PhSWidget(PhS* module) {
		setModule(module);
		setPanel(createPanel(asset::plugin(pluginInstance, "res/PhS.svg")));

    float x=1.9f;
    addParam(createParam<TrimbotWhite>(mm2px(Vec(x,9)),module,PhS::SHAPE_TYPE_PARAM));
    addParam(createParam<TrimbotWhite>(mm2px(Vec(x,21)),module,PhS::SHAPE_PARAM));
    addInput(createInput<SmallPort>(mm2px(Vec(x,31)),module,PhS::SHAPE_PARAM_INPUT));
    addParam(createParam<TrimbotWhite>(mm2px(Vec(x,41)),module,PhS::SHAPE_CV_PARAM));
    addInput(createInput<SmallPort>(mm2px(Vec(x,MHEIGHT-67)),module,PhS::INPUT));
    addOutput(createOutput<SmallPort>(mm2px(Vec(x,116)),module,PhS::OUTPUT));
	}
};


Model* modelPhS = createModel<PhS, PhSWidget>("PhS");