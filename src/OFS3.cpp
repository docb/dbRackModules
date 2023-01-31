#include "dcb.h"
#include "OFS.hpp"

#define NUM_OFS 3

struct OFS3 : Module {
  enum ParamId {
    ENUMS(OFFSET_PARAM,NUM_OFS),ENUMS(OFFSET_CV_PARAM,NUM_OFS),ENUMS(SCALE_PARAM,NUM_OFS),ENUMS(SCALE_CV_PARAM,NUM_OFS),PARAMS_LEN
  };
  enum InputId {
    ENUMS(CV_INPUT,NUM_OFS),ENUMS(OFFSET_INPUT,NUM_OFS),ENUMS(SCALE_INPUT,NUM_OFS),INPUTS_LEN
  };
  enum OutputId {
    ENUMS(CV_OUTPUT,NUM_OFS),OUTPUTS_LEN
  };
	enum LightId {
		LIGHTS_LEN
	};

  OfsW ofsw[NUM_OFS]={
    {CV_OUTPUT,CV_INPUT,OFFSET_PARAM,OFFSET_CV_PARAM,OFFSET_INPUT,SCALE_PARAM,SCALE_CV_PARAM,SCALE_INPUT},
    {CV_OUTPUT+1,CV_INPUT+1,OFFSET_PARAM+1,OFFSET_CV_PARAM+1,OFFSET_INPUT+1,SCALE_PARAM+1,SCALE_CV_PARAM+1,SCALE_INPUT+1},
    {CV_OUTPUT+2,CV_INPUT+2,OFFSET_PARAM+2,OFFSET_CV_PARAM+2,OFFSET_INPUT+2,SCALE_PARAM+2,SCALE_CV_PARAM+2,SCALE_INPUT+2}
  };
  bool offsetThenScale[3]={};

	OFS3() {
		config(PARAMS_LEN, INPUTS_LEN, OUTPUTS_LEN, LIGHTS_LEN);
    for(int k=0;k<NUM_OFS;k++) {
      std::string no=std::to_string(k+1);
      configParam(OFFSET_PARAM+k,-10,10,0,"Offset "+no);
      configParam(SCALE_PARAM+k,-10,10,1,"Scale "+no);
      configParam(OFFSET_CV_PARAM+k,0,1,0,"Offset CV "+no," %",0,100);
      configParam(SCALE_CV_PARAM+k,0,1,0,"Scale CV "+no," %",0,100);
      configInput(CV_INPUT+k,"CV "+no);
      configInput(SCALE_INPUT+k,"Scale "+no);
      configInput(OFFSET_INPUT+k,"Offset "+no);
      configOutput(CV_OUTPUT+k,"CV "+no);
    }
	}

	void process(const ProcessArgs& args) override {
    for(int k=0;k<NUM_OFS;k++) {
      ofsw[k].process(this,offsetThenScale[k]);
    }
	}
};


struct OFS3Widget : ModuleWidget {
	OFS3Widget(OFS3* module) {
		setModule(module);
		setPanel(createPanel(asset::plugin(pluginInstance, "res/OFS3.svg")));
    float y0[NUM_OFS]={8,48,89};
    float x=1.9;
    float x2=11.9;
    for(int k=0;k<NUM_OFS;k++) {
      float y=y0[k];
      addParam(createParam<TrimbotWhite>(mm2px(Vec(x,y)),module,OFS3::OFFSET_PARAM+k));
      y+=8;
      addInput(createInput<SmallPort>(mm2px(Vec(x,y)),module,OFS3::OFFSET_INPUT+k));
      y+=8;
      addParam(createParam<TrimbotWhite>(mm2px(Vec(x,y)),module,OFS3::OFFSET_CV_PARAM+k));
      y=y0[k];
      addParam(createParam<TrimbotWhite>(mm2px(Vec(x2,y)),module,OFS3::SCALE_PARAM+k));
      y+=8;
      addInput(createInput<SmallPort>(mm2px(Vec(x2,y)),module,OFS3::SCALE_INPUT+k));
      y+=8;
      addParam(createParam<TrimbotWhite>(mm2px(Vec(x2,y)),module,OFS3::SCALE_CV_PARAM+k));
      y+=11;
      addInput(createInput<SmallPort>(mm2px(Vec(x,y)),module,OFS3::CV_INPUT+k));
      addOutput(createOutput<SmallPort>(mm2px(Vec(x2,y)),module,OFS3::CV_OUTPUT+k));
    }
	}
  void appendContextMenu(Menu* menu) override {
    OFS3 *module=dynamic_cast<OFS3 *>(this->module);
    assert(module);

    menu->addChild(new MenuSeparator);
    for(int k=0;k<NUM_OFS;k++)
       menu->addChild(createBoolPtrMenuItem("Offset Then Scale " + std::to_string(k+1),"",&module->offsetThenScale[k]));
  }
};


Model* modelOFS3 = createModel<OFS3, OFS3Widget>("OFS3");