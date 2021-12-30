#include "dcb.h"
//static const uint32_t DEPTH = 2147483647U;
//static const uint32_t DEPTH = 16777216U;
static const uint32_t DEPTH = 8388608U;
//static const uint32_t DEPTH = 65536U;

struct FLA : Module {
	enum ParamId {
		PARAMS_LEN
	};
	enum InputId {
    N_INPUT,
    M_INPUT,
    INPUTS_LEN
	};
	enum OutputId {
    DIV_OUTPUT,
    MULT_OUTPUT,
    MOD_OUTPUT,
    PLUS_OUTPUT,
    MINUS_OUTPUT,
    OUTPUTS_LEN
	};
	enum LightId {
		LIGHTS_LEN
	};

	FLA() {
		config(PARAMS_LEN, INPUTS_LEN, OUTPUTS_LEN, LIGHTS_LEN);
    configInput(N_INPUT,"N");
    configInput(M_INPUT,"M");
    configOutput(DIV_OUTPUT,"N/M");
    configOutput(MULT_OUTPUT,"N*M");
    configOutput(MOD_OUTPUT,"N%M");
    configOutput(PLUS_OUTPUT,"N+M");
    configOutput(MINUS_OUTPUT,"N-M");
	}


	void process(const ProcessArgs& args) override {
    float fn = clamp(inputs[N_INPUT].getVoltage(),0.f,10.f)*0.1f;
    float fm = clamp(inputs[M_INPUT].getVoltage(),0.f,10.f)*0.1f;
    unsigned long n = (unsigned long)floor(float(DEPTH)*fn);
    unsigned long m = (unsigned long)floor(float(DEPTH)*fm);
    outputs[DIV_OUTPUT].setVoltage(m==0?0:(n/m)/(float(DEPTH)/10.f));
    outputs[MOD_OUTPUT].setVoltage(m==0?0:(n%m)/(float(DEPTH)/10.f));
    outputs[MULT_OUTPUT].setVoltage(((n*m)&(DEPTH-1))/(float(DEPTH)/10.f));
    outputs[PLUS_OUTPUT].setVoltage(((n+m)&(DEPTH-1))/(float(DEPTH)/10.f));
    outputs[MINUS_OUTPUT].setVoltage(((n-m)&(DEPTH-1))/(float(DEPTH)/10.f));
	}
};


struct FLAWidget : ModuleWidget {
	FLAWidget(FLA* module) {
		setModule(module);
		setPanel(createPanel(asset::plugin(pluginInstance, "res/FLA.svg")));

		addChild(createWidget<ScrewSilver>(Vec(RACK_GRID_WIDTH, 0)));
		addChild(createWidget<ScrewSilver>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, 0)));
		addChild(createWidget<ScrewSilver>(Vec(RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));
		addChild(createWidget<ScrewSilver>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));

    addInput(createInput<SmallPort>(mm2px(Vec(1.9f,MHEIGHT-115.f)),module,FLA::N_INPUT));
    addInput(createInput<SmallPort>(mm2px(Vec(1.9f,MHEIGHT-103.f)),module,FLA::M_INPUT));
    addOutput(createOutput<SmallPort>(mm2px(Vec(1.9f,MHEIGHT-79.f-12.f)),module,FLA::DIV_OUTPUT));
    addOutput(createOutput<SmallPort>(mm2px(Vec(1.9f,MHEIGHT-67.f-12.f)),module,FLA::MULT_OUTPUT));
    addOutput(createOutput<SmallPort>(mm2px(Vec(1.9f,MHEIGHT-55.f-12.f)),module,FLA::MOD_OUTPUT));
    addOutput(createOutput<SmallPort>(mm2px(Vec(1.9f,MHEIGHT-43.f-12.f)),module,FLA::PLUS_OUTPUT));
    addOutput(createOutput<SmallPort>(mm2px(Vec(1.9f,MHEIGHT-31.f-12.f)),module,FLA::MINUS_OUTPUT));
	}
};

struct FLL : Module {
  enum ParamId {
    PARAMS_LEN
  };
  enum InputId {
    N_INPUT,
    M_INPUT,
    INPUTS_LEN
  };
  enum OutputId {
    OR_OUTPUT,
    AND_OUTPUT,
    XOR_OUTPUT,
    SR_OUTPUT,
    SL_OUTPUT,
    NOTN_OUTPUT,
    NOTM_OUTPUT,
    OUTPUTS_LEN
  };
  enum LightId {
    LIGHTS_LEN
  };

  FLL() {
    config(PARAMS_LEN, INPUTS_LEN, OUTPUTS_LEN, LIGHTS_LEN);
    configInput(N_INPUT,"N");
    configInput(M_INPUT,"M");
    configOutput(OR_OUTPUT,"N|M (or)");
    configOutput(AND_OUTPUT,"N&M (and)");
    configOutput(XOR_OUTPUT,"N^M (xor)");
    configOutput(SR_OUTPUT,"N>>M (shift right)");
    configOutput(SL_OUTPUT,"N<<M (shift left)");
    configOutput(NOTM_OUTPUT,"~M (not M)");
    configOutput(NOTN_OUTPUT,"~N (not N)");
  }

  void process(const ProcessArgs& args) override {
    float fn = clamp(inputs[N_INPUT].getVoltage(),0.f,10.f)*0.1f;
    float fm = clamp(inputs[M_INPUT].getVoltage(),0.f,10.f)*0.1f;
    unsigned long n = (unsigned long)floor((float(DEPTH))*fn);
    unsigned long m = (unsigned long)floor((float(DEPTH))*fm);
    outputs[AND_OUTPUT].setVoltage((n&m)/(float(DEPTH)/10.f));
    outputs[OR_OUTPUT].setVoltage((n|m)/(float(DEPTH)/10.f));
    outputs[XOR_OUTPUT].setVoltage(((n^m)&(DEPTH-1))/(float(DEPTH)/10.f));
    unsigned long s = (unsigned long)round(fm*(rack::math::log2(DEPTH)-1));
    outputs[SR_OUTPUT].setVoltage((n>>s)/(float(DEPTH)/10.f));
    outputs[SL_OUTPUT].setVoltage(((n<<s)&(DEPTH-1))/(float(DEPTH)/10.f));
    outputs[NOTN_OUTPUT].setVoltage(((~n)&(DEPTH-1))/(float(DEPTH)/10.f));
    outputs[NOTM_OUTPUT].setVoltage(((~n)&(DEPTH-1))/(float(DEPTH)/10.f));
  }
};

struct FLLWidget : ModuleWidget {
  FLLWidget(FLL* module) {
    setModule(module);
    setPanel(createPanel(asset::plugin(pluginInstance, "res/FLL.svg")));

    addChild(createWidget<ScrewSilver>(Vec(RACK_GRID_WIDTH, 0)));
    addChild(createWidget<ScrewSilver>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, 0)));
    addChild(createWidget<ScrewSilver>(Vec(RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));
    addChild(createWidget<ScrewSilver>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));

    addInput(createInput<SmallPort>(mm2px(Vec(1.9f,MHEIGHT-115.f)),module,FLL::N_INPUT));
    addInput(createInput<SmallPort>(mm2px(Vec(1.9f,MHEIGHT-103.f)),module,FLL::M_INPUT));
    addOutput(createOutput<SmallPort>(mm2px(Vec(1.9f,MHEIGHT-79.f-12.f)),module,FLL::OR_OUTPUT));
    addOutput(createOutput<SmallPort>(mm2px(Vec(1.9f,MHEIGHT-67.f-12.f)),module,FLL::AND_OUTPUT));
    addOutput(createOutput<SmallPort>(mm2px(Vec(1.9f,MHEIGHT-55.f-12.f)),module,FLL::XOR_OUTPUT));
    addOutput(createOutput<SmallPort>(mm2px(Vec(1.9f,MHEIGHT-43.f-12.f)),module,FLL::SR_OUTPUT));
    addOutput(createOutput<SmallPort>(mm2px(Vec(1.9f,MHEIGHT-31.f-12.f)),module,FLL::SL_OUTPUT));
    addOutput(createOutput<SmallPort>(mm2px(Vec(1.9f,MHEIGHT-31.f)),module,FLL::NOTN_OUTPUT));
    addOutput(createOutput<SmallPort>(mm2px(Vec(1.9f,MHEIGHT-19.f)),module,FLL::NOTM_OUTPUT));
  }
};

Model* modelFLA = createModel<FLA, FLAWidget>("FLA");
Model* modelFLL = createModel<FLL, FLLWidget>("FLL");