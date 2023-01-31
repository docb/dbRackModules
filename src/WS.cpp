#include "dcb.h"
using simd::float_4;

struct WS : Module {
	enum ParamId {
		A_PARAM,B_PARAM,C_PARAM,D_PARAM,A_CV_PARAM,B_CV_PARAM,C_CV_PARAM,D_CV_PARAM,STEREO_WIDTH_PARAM,PARAMS_LEN
	};
	enum InputId {
		CV1_INPUT,CV2_INPUT,A_INPUT,B_INPUT,C_INPUT,D_INPUT,INPUTS_LEN
	};
	enum OutputId {
    CV1_OUTPUT,CV2_OUTPUT,OUTPUTS_LEN
	};
	enum LightId {
		LIGHTS_LEN
	};

  DCBlocker<float_4> dcb1[4];
  DCBlocker<float_4> dcb2[4];

	WS() {
		config(PARAMS_LEN, INPUTS_LEN, OUTPUTS_LEN, LIGHTS_LEN);
    configParam(A_PARAM,0,5,1,"A");
    configParam(B_PARAM,0,5,1,"B");
    configParam(C_PARAM,0,5,2,"C");
    configParam(D_PARAM,0,5,2,"D");
    configParam(A_CV_PARAM,0,1,0,"A");
    configParam(B_CV_PARAM,0,1,0,"B");
    configParam(C_CV_PARAM,0,1,0,"C");
    configParam(D_CV_PARAM,0,1,0,"D");
    configInput(A_INPUT,"A");
    configInput(B_INPUT,"B");
    configInput(C_INPUT,"C");
    configInput(D_INPUT,"D");
    configParam(STEREO_WIDTH_PARAM,0,1,0.5,"Stereo Width");
    configInput(CV1_INPUT,"CV1");
    configInput(CV2_INPUT,"CV2");
    configOutput(CV1_OUTPUT,"CV1");
    configOutput(CV2_OUTPUT,"CV2");
	}

	void process(const ProcessArgs& args) override {
    bool in2Connected=inputs[CV2_INPUT].isConnected();
    int channels=in2Connected?std::min(inputs[CV1_INPUT].getChannels(),inputs[CV2_INPUT].getChannels()):inputs[CV1_INPUT].getChannels();
    float a=params[A_PARAM].getValue();
    float b=params[B_PARAM].getValue();
    float c=params[C_PARAM].getValue();
    float d=params[D_PARAM].getValue();
    float av=params[A_CV_PARAM].getValue();
    float bv=params[B_CV_PARAM].getValue();
    float cv=params[C_CV_PARAM].getValue();
    float dv=params[D_CV_PARAM].getValue();
    float stw=params[STEREO_WIDTH_PARAM].getValue();
    for(int k=0;k<channels;k+=4) {
      float_4 in1=inputs[CV1_INPUT].getVoltageSimd<float_4>(k)*0.1f+0.5f;
      float_4 in2=simd::ifelse(in2Connected,inputs[CV2_INPUT].getVoltageSimd<float_4>(k)*0.1f+0.5f,in1);

      float_4 ap=simd::clamp(a+inputs[A_INPUT].getPolyVoltageSimd<float_4>(k)*av,0.f,5.f);
      float_4 bp=simd::clamp(b+inputs[B_INPUT].getPolyVoltageSimd<float_4>(k)*bv,0.f,5.f);
      float_4 cp=simd::clamp(c+inputs[C_INPUT].getPolyVoltageSimd<float_4>(k)*cv,0.f,5.f);
      float_4 dp=simd::clamp(d+inputs[D_INPUT].getPolyVoltageSimd<float_4>(k)*dv,0.f,5.f);

      float_4 A=in2*ap;
      float_4 B=in1*bp;
      float_4 C=in1*cp;
      float_4 D=in2*dp;

      in1=simd::sin(A)-simd::cos(B);
      in2=simd::sin(C)-simd::cos(D);

      A=in2*ap;
      B=in1*bp;
      C=in1*cp;
      D=in2*dp;

      float_4 out1=simd::sin(A)-simd::cos(B);
      float_4 out2=simd::sin(C)-simd::cos(D);
      out1=dcb1[k/4].process(out1);
      out2=dcb2[k/4].process(out2);

      outputs[CV1_OUTPUT].setVoltageSimd((1+stw)*out1+(1-stw)*out2,k);
      outputs[CV2_OUTPUT].setVoltageSimd((1+stw)*out2+(1-stw)*out1,k);
    }
    outputs[CV1_OUTPUT].setChannels(channels);
    outputs[CV2_OUTPUT].setChannels(channels);
	}
};


struct WSWidget : ModuleWidget {
	WSWidget(WS* module) {
		setModule(module);

		setPanel(createPanel(asset::plugin(pluginInstance, "res/WS.svg")));
    float x=1.9;
    float x2=11.9;
    float y=11;
    float y2=9;
    addParam(createParam<TrimbotWhite9>(mm2px(Vec(x,y)),module,WS::A_PARAM));
    addParam(createParam<TrimbotWhite>(mm2px(Vec(x2,y2)),module,WS::A_CV_PARAM));
    addInput(createInput<SmallPort>(mm2px(Vec(x2,y2+7)),module,WS::A_INPUT));
    y+=17.5;
    y2+=17.5;
    addParam(createParam<TrimbotWhite9>(mm2px(Vec(x,y)),module,WS::B_PARAM));
    addParam(createParam<TrimbotWhite>(mm2px(Vec(x2,y2)),module,WS::B_CV_PARAM));
    addInput(createInput<SmallPort>(mm2px(Vec(x2,y2+7)),module,WS::B_INPUT));
    y+=17.5;
    y2+=17.5;
    addParam(createParam<TrimbotWhite9>(mm2px(Vec(x,y)),module,WS::C_PARAM));
    addParam(createParam<TrimbotWhite>(mm2px(Vec(x2,y2)),module,WS::C_CV_PARAM));
    addInput(createInput<SmallPort>(mm2px(Vec(x2,y2+7)),module,WS::C_INPUT));
    y+=17.5;
    y2+=17.5;
    addParam(createParam<TrimbotWhite9>(mm2px(Vec(x,y)),module,WS::D_PARAM));
    addParam(createParam<TrimbotWhite>(mm2px(Vec(x2,y2)),module,WS::D_CV_PARAM));
    addInput(createInput<SmallPort>(mm2px(Vec(x2,y2+7)),module,WS::D_INPUT));

    y=92;
    x=1.9;
    x2=11.9;
    addInput(createInput<SmallPort>(mm2px(Vec(x,y)),module,WS::CV1_INPUT));
    addInput(createInput<SmallPort>(mm2px(Vec(x2,y)),module,WS::CV2_INPUT));
    y+=12;
    addParam(createParam<TrimbotWhite>(mm2px(Vec(7.048,y)),module,WS::STEREO_WIDTH_PARAM));
    y+=12;
    addOutput(createOutput<SmallPort>(mm2px(Vec(x,y)),module,WS::CV1_OUTPUT));
    addOutput(createOutput<SmallPort>(mm2px(Vec(x2,y)),module,WS::CV2_OUTPUT));
	}
};


Model* modelWS = createModel<WS, WSWidget>("WS");