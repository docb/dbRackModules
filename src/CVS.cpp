#include "dcb.h"
using simd::float_4;

struct CVS : Module {
	enum ParamId {
    LEN_PARAM,PARAMS_LEN
	};
	enum InputId {
		POLY_INPUT=16,CV_INPUT=20,INPUTS_LEN
	};
	enum OutputId {
		CV_OUTPUT,TRG_OUTPUT,OUTPUTS_LEN
	};
	enum LightId {
		LIGHTS_LEN=16
	};

  float last[16]={};

	CVS() {
		config(PARAMS_LEN, INPUTS_LEN, OUTPUTS_LEN, LIGHTS_LEN);
    configParam(LEN_PARAM,2,16,16,"Length");
    getParamQuantity(LEN_PARAM)->snapEnabled=true;
    for(int k=0;k<16;k++) {
      std::string label=std::to_string(k+1);
      configInput(k,label);
    }
    configInput(CV_INPUT,"CV");
    for(int k=0;k<4;k++) {
      std::string label=std::to_string(k+1);
      configInput(POLY_INPUT+k,"Poly Chn " + label);
    }
    configOutput(TRG_OUTPUT,"Changed");
    configOutput(CV_OUTPUT,"CV");
	}

	void process(const ProcessArgs& args) override {
    int channels=std::max(inputs[CV_INPUT].getChannels(),1);
    int length=params[LEN_PARAM].getValue();
    for(int k=0;k<16;k++) {
      lights[k].setBrightness(0);
    }
    for(int chn=0;chn<channels;chn++) {
      float pos=clamp(inputs[CV_INPUT].getVoltage(chn),0.f,10.f)*1.6f;
      int stage=int(pos);
      float frac=pos-float(stage);
      stage=stage%length;
      int stage1=(stage+1)%length;
      float v1=0.f;
      float v2=0.f;
      if(chn<4)
        v1=inputs[POLY_INPUT+chn].getVoltage(stage);
      if(inputs[stage].isConnected()) v1=inputs[stage].getVoltage(chn);
      if(chn<4)
        v2=inputs[POLY_INPUT+chn].getVoltage(stage1);
      if(inputs[stage1].isConnected()) v2=inputs[stage1].getVoltage(chn);
      float out=v1*(1-frac)+v2*frac;
      outputs[TRG_OUTPUT].setVoltage(out!=last[chn]?10.f:0.f,chn);
      last[chn]=out;
      outputs[CV_OUTPUT].setVoltage(out,chn);
      lights[stage].setBrightness(1-frac);
      lights[stage1].setBrightness(frac);
    }
    outputs[CV_OUTPUT].setChannels(channels);
    outputs[TRG_OUTPUT].setChannels(channels);
	}
};

struct CVSWidget : ModuleWidget {
	CVSWidget(CVS* module) {
		setModule(module);
		setPanel(createPanel(asset::plugin(pluginInstance, "res/CVS.svg")));
    float x=2;
    float y=11;
    for(int k=0;k<16;k++) {
      addInput(createInput<SmallPort>(mm2px(Vec(x,y)),module,k));
      addChild(createLightCentered<DBSmallLight<GreenLight>>(mm2px(Vec(10,y+2)),module,k));
      y+=7;

    }
    x=12;
    y=18;
    for(int k=0;k<4;k++) {
      addInput(createInput<SmallPort>(mm2px(Vec(x,y)),module,CVS::POLY_INPUT+k));
      y+=14;
    }
    addParam(createParam<TrimbotWhite>(mm2px(Vec(12,76)),module,CVS::LEN_PARAM));
    addInput(createInput<SmallPort>(mm2px(Vec(12,88)),module,CVS::CV_INPUT));
    addOutput(createOutput<SmallPort>(mm2px(Vec(12,104)),module,CVS::TRG_OUTPUT));
    addOutput(createOutput<SmallPort>(mm2px(Vec(12,116)),module,CVS::CV_OUTPUT));
	}
};


Model* modelCVS = createModel<CVS, CVSWidget>("CVS");