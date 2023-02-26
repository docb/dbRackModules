#include "dcb.h"
#include "clipper.hpp"
using simd::float_4;
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