#include "dcb.h"


struct Frac : Module {
	enum ParamIds {
    N_PARAM,
    D_PARAM,
    BASE_PARAM,
    OFS_PARAM,
    SCL_PARAM,
    BV_PARAM,
		NUM_PARAMS
	};
	enum InputIds {
    CLOCK_INPUT,RST_INPUT,NUM_INPUTS
	};
	enum OutputIds {
    CV_OUTPUT,
		NUM_OUTPUTS
	};
	enum LightIds {
		NUM_LIGHTS
	};

	Frac() {
		config(NUM_PARAMS, NUM_INPUTS, NUM_OUTPUTS, NUM_LIGHTS);
    configParam(N_PARAM,1.f,100000.f,1.f,"N");
    configParam(D_PARAM,1.f,100000.f,1.f,"D");
    configParam(BASE_PARAM,2.f,16.f,10.f,"Base");
    configParam(OFS_PARAM,0.f,100.f,0.f,"Offset");
    configParam(BV_PARAM,-10.f,10.f,0.f,"Base Volt");
    configParam(SCL_PARAM,0.01f,1.f,0.1f,"Scale");
    configInput(CLOCK_INPUT,"Clock");
    configInput(RST_INPUT,"Reset");
    configOutput(CV_OUTPUT,"CV");
    state = 1;
	}

  int state;
  int sbase;
  int sd;
  dsp::SchmittTrigger clockTrigger;
  dsp::SchmittTrigger rstTrigger;

  int lastN=-1;
  int lastD=-1;
  int lastBase=-1;

  // the phenomenal two lines of code of the algorithm we learned at school on a piece of paper
  int next() {
    int ret = state / sd;
    state = state % sd * sbase;
    return ret;
  }

  void init(int n,int d,int base) {
    if(d<=0) d = 1;
    int ofs = floorf(params[OFS_PARAM].getValue());
    state = n;
    // doesnt matter where the comma is, so reducing here to the case where the comma is after the first digit
    while(true) {
      int dd = d * base;
      if(dd>n) break;
      d = d * base;
    }
    sd = d;
    sbase = base;
    for(int k=0;k<ofs;k++) {
      next();
    }
    lastN=n;
    lastD=d;
    lastBase=base;
  }
  
  void process(const ProcessArgs& args) override {
    int n = floorf(params[N_PARAM].getValue());
    int d = floorf(params[D_PARAM].getValue());
    int base = floorf(params[BASE_PARAM].getValue());
    if(n!=lastN||d!=lastD||base!=lastBase) init(n,d,base);
    if(rstTrigger.process(inputs[RST_INPUT].getVoltage())) {
      init(n,d,base);
    }
    if(inputs[CLOCK_INPUT].isConnected()) {
      if(clockTrigger.process(inputs[CLOCK_INPUT].getVoltage())) {
        auto value = (float)next();
        float bv = params[BV_PARAM].getValue();
        float scl = params[SCL_PARAM].getValue();
        outputs[CV_OUTPUT].setVoltage(bv+scl*value);
      }
    }
  }
};


struct FracWidget : ModuleWidget {
	FracWidget(Frac* module) {
		setModule(module);
		setPanel(APP->window->loadSvg(asset::plugin(pluginInstance, "res/Frac.svg")));

		addChild(createWidget<ScrewSilver>(Vec(RACK_GRID_WIDTH, 0)));
		addChild(createWidget<ScrewSilver>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, 0)));
		addChild(createWidget<ScrewSilver>(Vec(RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));
		addChild(createWidget<ScrewSilver>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));

    float xpos = 1.9f;
    addInput(createInput<SmallPort>(mm2px(Vec(xpos,MHEIGHT-115.f)),module,Frac::CLOCK_INPUT));
    addInput(createInput<SmallPort>(mm2px(Vec(xpos,MHEIGHT-103.f)),module,Frac::RST_INPUT));
    addParam(createParam<TrimbotWhiteSnap>(mm2px(Vec(2,MHEIGHT-91.5f)),module,Frac::N_PARAM));
    addParam(createParam<TrimbotWhiteSnap>(mm2px(Vec(2,MHEIGHT-79.0f)),module,Frac::D_PARAM));
    addParam(createParam<TrimbotWhiteSnap>(mm2px(Vec(2,MHEIGHT-67.0f)),module,Frac::BASE_PARAM));
    addParam(createParam<TrimbotWhiteSnap>(mm2px(Vec(2,MHEIGHT-55.5f)),module,Frac::OFS_PARAM));
    addParam(createParam<TrimbotWhite>(mm2px(Vec(2,MHEIGHT-43.5f)),module,Frac::SCL_PARAM));
    addParam(createParam<TrimbotWhite>(mm2px(Vec(2,MHEIGHT-31.5f)),module,Frac::BV_PARAM));
    addOutput(createOutput<SmallPort>(mm2px(Vec(xpos,MHEIGHT-19.5f)),module,Frac::CV_OUTPUT));
    
	}
};


Model* modelFrac = createModel<Frac, FracWidget>("Frac");