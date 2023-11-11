#include "dcb.h"
#include "OFS.hpp"

struct OFS : Module {
	enum ParamId {
		OFFSET_PARAM,OFFSET_CV_PARAM,SCALE_PARAM,SCALE_CV_PARAM,PARAMS_LEN
	};
	enum InputId {
		CV_INPUT,OFFSET_INPUT,SCALE_INPUT,INPUTS_LEN
	};
	enum OutputId {
		CV_OUTPUT,OUTPUTS_LEN
	};
	enum LightId {
		LIGHTS_LEN
	};
  bool offsetThenScale=false;

  OfsW ofsW{CV_OUTPUT,CV_INPUT,OFFSET_PARAM,OFFSET_CV_PARAM,OFFSET_INPUT,SCALE_PARAM,SCALE_CV_PARAM,SCALE_INPUT};
	OFS() {
		config(PARAMS_LEN, INPUTS_LEN, OUTPUTS_LEN, LIGHTS_LEN);
    configParam(OFFSET_PARAM,-10,10,0,"Offset");
    configParam(SCALE_PARAM,-10,10,1,"Scale");
    configParam(OFFSET_CV_PARAM,0,1,0,"Offset CV"," %",0,100);
    configParam(SCALE_CV_PARAM,0,1,0,"Scale CV"," %",0,100);
    configInput(CV_INPUT,"CV");
    configInput(SCALE_INPUT,"Scale");
    configInput(OFFSET_INPUT,"Offset");
    configOutput(CV_OUTPUT,"CV");
	}

	void process(const ProcessArgs& args) override {
    ofsW.process(this,offsetThenScale);
	}
  void dataFromJson(json_t *root) override {
    json_t *jOffsetThenScale=json_object_get(root,"offsetThenScale");
    if(jOffsetThenScale) {
      offsetThenScale=json_boolean_value(jOffsetThenScale);
    }
  }

  json_t *dataToJson() override {
    json_t *root=json_object();
    json_object_set_new(root,"offsetThenScale",json_boolean(offsetThenScale));
    return root;
  }
};


struct OFSWidget : ModuleWidget {
	OFSWidget(OFS* module) {
		setModule(module);
		setPanel(createPanel(asset::plugin(pluginInstance, "res/OFS.svg")));
    float y=10;
    float x=1.9;
    addParam(createParam<TrimbotWhite>(mm2px(Vec(x,y)),module,OFS::OFFSET_PARAM));
    y+=8;
    addInput(createInput<SmallPort>(mm2px(Vec(x,y)),module,OFS::OFFSET_INPUT));
    y+=8;
    addParam(createParam<TrimbotWhite>(mm2px(Vec(x,y)),module,OFS::OFFSET_CV_PARAM));
    y=40;
    addParam(createParam<TrimbotWhite>(mm2px(Vec(x,y)),module,OFS::SCALE_PARAM));
    y+=8;
    addInput(createInput<SmallPort>(mm2px(Vec(x,y)),module,OFS::SCALE_INPUT));
    y+=8;
    addParam(createParam<TrimbotWhite>(mm2px(Vec(x,y)),module,OFS::SCALE_CV_PARAM));

    y=104;
    addInput(createInput<SmallPort>(mm2px(Vec(x,y)),module,OFS::CV_INPUT));
    y+=12;
    addOutput(createOutput<SmallPort>(mm2px(Vec(x,y)),module,OFS::CV_OUTPUT));
	}
  void appendContextMenu(Menu* menu) override {
    OFS *module=dynamic_cast<OFS *>(this->module);
    assert(module);

    menu->addChild(new MenuSeparator);

    menu->addChild(createBoolPtrMenuItem("Offset Then Scale","",&module->offsetThenScale));
  }
};


Model* modelOFS = createModel<OFS, OFSWidget>("OFS");