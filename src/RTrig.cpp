#include "dcb.h"
#include "rnd.h"

struct RTrig : Module {
	enum ParamId {
    FREQ_PARAM,
    DEV_PARAM,
    CHN_PARAM,
		PARAMS_LEN
	};
	enum InputId {
    SRC_INPUT,
    FRQ_INPUT,
    DEV_INPUT,
    RST_INPUT,
		INPUTS_LEN
	};
	enum OutputId {
    TRIG_OUTPUT,
		OUTPUTS_LEN
	};
	enum LightId {
		LIGHTS_LEN
	};

  RND rnd;
  int counter = 0;
  dsp::PulseGenerator pulseGenerators[16];
  dsp::SchmittTrigger rstTrigger;

  RTrig() {
		config(PARAMS_LEN, INPUTS_LEN, OUTPUTS_LEN, LIGHTS_LEN);
    configParam(FREQ_PARAM,-8.f,8.f,0.f,"Frequency"," Hz",2,1);
    configParam(DEV_PARAM,0,1,0,"Deviation"," %",0,100);
    configParam(CHN_PARAM,1,16,1,"Channels");
    getParamQuantity(CHN_PARAM)->snapEnabled = true;
    configInput(SRC_INPUT,"Random source");
    configInput(RST_INPUT,"Reset");
    configInput(FRQ_INPUT,"Frequency");
    configInput(DEV_INPUT,"Deviation");
    configOutput(TRIG_OUTPUT,"Trig");
	}

	void process(const ProcessArgs& args) override {
    if(rstTrigger.process(inputs[RST_INPUT].getVoltage())) {
      counter = 0;
    }
    bool src = inputs[SRC_INPUT].isConnected();
    int channels = params[CHN_PARAM].getValue();
    if(counter<=0) {
      float freq = powf(2.0f,inputs[FRQ_INPUT].getNormalVoltage(params[FREQ_PARAM].getValue()));
      float nextsmps = args.sampleRate/freq;
      float rndVal = src?clamp(inputs[SRC_INPUT].getVoltage()*0.2f,-1.f,1.f):rnd.nextTri(2)*2.0f - 1.0f;
      float dev;
      if(inputs[DEV_INPUT].isConnected()) dev =  clamp(inputs[DEV_INPUT].getVoltage()*0.1f,0.f,1.f);
      else dev = params[DEV_PARAM].getValue();
      counter = int(nextsmps) + int(nextsmps*dev*rndVal);
      int chn = floor(rnd.nextDouble()*channels);// Choose some channel
      pulseGenerators[chn].trigger(0.01f);
      //INFO("trigger %d %f %d %f %f",chn,nextsmps,counter, freq, params[FREQ_PARAM].getValue() );
    }
    counter--;
    for(int k=0;k<channels;k++) {
      bool trigger=pulseGenerators[k].process(args.sampleTime);
      outputs[TRIG_OUTPUT].setVoltage((trigger?10.0f:0.0f),k);
    }
    outputs[TRIG_OUTPUT].setChannels(channels);
	}
};


struct RTrigWidget : ModuleWidget {
	RTrigWidget(RTrig* module) {
		setModule(module);
		setPanel(createPanel(asset::plugin(pluginInstance, "res/RTrig.svg")));

		addChild(createWidget<ScrewSilver>(Vec(RACK_GRID_WIDTH, 0)));
		addChild(createWidget<ScrewSilver>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, 0)));
		addChild(createWidget<ScrewSilver>(Vec(RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));
		addChild(createWidget<ScrewSilver>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));

    addParam(createParam<TrimbotWhite>(mm2px(Vec(2,MHEIGHT-115.f)),module,RTrig::FREQ_PARAM));
    addInput(createInput<SmallPort>(mm2px(Vec(2,MHEIGHT-105.f)),module,RTrig::FRQ_INPUT));
    addParam(createParam<TrimbotWhite>(mm2px(Vec(2,MHEIGHT-91.f)),module,RTrig::DEV_PARAM));
    addInput(createInput<SmallPort>(mm2px(Vec(2,MHEIGHT-81.f)),module,RTrig::DEV_INPUT));
    addInput(createInput<SmallPort>(mm2px(Vec(2,MHEIGHT-55.f)),module,RTrig::RST_INPUT));
    addInput(createInput<SmallPort>(mm2px(Vec(2,MHEIGHT-43.f)),module,RTrig::SRC_INPUT));
    addParam(createParam<TrimbotWhite>(mm2px(Vec(2,MHEIGHT-31.f)),module,RTrig::CHN_PARAM));
    addOutput(createOutput<SmallPort>(mm2px(Vec(2,MHEIGHT-19.f)),module,RTrig::TRIG_OUTPUT));
  }
};


Model* modelRtrig = createModel<RTrig, RTrigWidget>("RTrig");