#include "dcb.h"
#include "Gamma/Delay.h"
#include "Gamma/Oscillator.h"
struct YC : Module {
	enum ParamId {
		DELAY_PARAM,DEPTH_PARAM,FFD_PARAM,FB_PARAM,FREQ_PARAM,MIX_PARAM,PARAMS_LEN
	};
	enum InputId {
		MOD_L_INPUT,MOD_R_INPUT,L_INPUT,R_INPUT,INPUTS_LEN
	};
	enum OutputId {
		L_OUTPUT,R_OUTPUT,OUTPUTS_LEN
	};
	enum LightId {
		LIGHTS_LEN
	};

  gam::Comb<gam::real, gam::ipl::Cubic> comb1, comb2;		///< Comb filters
  gam::CSine<double> mod;

	YC() {
		config(PARAMS_LEN, INPUTS_LEN, OUTPUTS_LEN, LIGHTS_LEN);
    configParam(DELAY_PARAM,0.1f,20.f,2.1f,"Delay","ms");
    configParam(DEPTH_PARAM,1.f,3.f,2.f,"Depth");
    configParam(FB_PARAM,0.f,1.f,0.1f,"Feedback");
    configParam(FFD_PARAM,0.01f,1.f,0.9f,"Feed forward");
    configParam(FREQ_PARAM,-8,4,0,"Rate","HZ",2,1);
    configParam(MIX_PARAM,0.0f,1.f,0.5f,"Dry/Wet");
    comb1.maxDelay(2.5f);
    comb2.maxDelay(2.5f);
    configInput(L_INPUT,"Left");
    configInput(R_INPUT,"Right");
    configInput(MOD_L_INPUT,"Left LFO");
    configInput(MOD_R_INPUT,"Right LFO");
    configOutput(L_OUTPUT,"Left");
    configOutput(R_OUTPUT,"Right");
    configBypass(L_INPUT,L_OUTPUT);
    configBypass(R_INPUT,R_OUTPUT);
  }

	void process(const ProcessArgs& args) override {
    if(inputs[L_INPUT].isConnected()){
      float delay=params[DELAY_PARAM].getValue()/1000.f;
      float depth=params[DEPTH_PARAM].getValue()/1000.f;
      float fb=params[FB_PARAM].getValue();
      float ffd=params[FFD_PARAM].getValue();
      float freq=pow(2,params[FREQ_PARAM].getValue());
      float mix = params[MIX_PARAM].getValue();
      comb1.fbk(fb);
      comb2.fbk(fb);
      comb1.ffd(ffd);
      comb2.ffd(ffd);
      mod.freq(freq);
      mod.amp(depth);
      float modL = inputs[MOD_L_INPUT].isConnected()?inputs[MOD_L_INPUT].getVoltage()*depth/5.f:mod.val.r;
      float modR = inputs[MOD_R_INPUT].isConnected()?inputs[MOD_R_INPUT].getVoltage()*depth/5.f:mod.val.i;
      comb1.delay(delay+modL);
      comb2.delay(delay+modR);
      mod();
      float l=inputs[L_INPUT].getVoltage()/5.f;
      if(!inputs[R_INPUT].isConnected()) {
        if(outputs[R_OUTPUT].isConnected()) { // mono to stereo
          outputs[L_OUTPUT].setVoltage((mix*comb1(l)+(1-mix)*l)*2.5f);
          outputs[R_OUTPUT].setVoltage((mix*comb2(l)+(1-mix)*l)*2.5f);
        } else { // mono to mono
          float v = (comb1(l)+comb2(l))/2.f;
          outputs[L_OUTPUT].setVoltage((mix*v+(1-mix)*l)*2.5f);
        }
      } else { // stereo to stereo
        float r=inputs[R_INPUT].getVoltage()/5.f;
        outputs[L_OUTPUT].setVoltage((mix*comb1(l)+(1-mix)*l)*2.5f);
        outputs[R_OUTPUT].setVoltage((mix*comb2(r)+(1-mix)*r)*2.5f);
      }
    }
	}

  void onSampleRateChange(const SampleRateChangeEvent& e) override {
    gam::sampleRate(APP->engine->getSampleRate());
  }
  void onAdd(const AddEvent &e) override {
    gam::sampleRate(APP->engine->getSampleRate());
  }
};


struct YCWidget : ModuleWidget {
	YCWidget(YC* module) {
		setModule(module);
		setPanel(createPanel(asset::plugin(pluginInstance, "res/YC.svg")));

    addChild(createWidget<ScrewSilver>(Vec(0, 0)));
    addChild(createWidget<ScrewSilver>(Vec(box.size.x - 15, 0)));
    addChild(createWidget<ScrewSilver>(Vec(0, 365)));
    addChild(createWidget<ScrewSilver>(Vec(box.size.x - 15, 365)));

    addInput(createInput<SmallPort>(mm2px(Vec(2, MHEIGHT-108-6.2)), module, YC::L_INPUT));
    addInput(createInput<SmallPort>(mm2px(Vec(11.9, MHEIGHT-108-6.2)), module, YC::R_INPUT));

    addParam(createParam<TrimbotWhite>(mm2px(Vec(7.3f, MHEIGHT-96-6.2)), module, YC::DELAY_PARAM));
    addParam(createParam<TrimbotWhite>(mm2px(Vec(7.3f, MHEIGHT-84-6.2)), module, YC::FB_PARAM));
    addParam(createParam<TrimbotWhite>(mm2px(Vec(7.3f, MHEIGHT-72-6.2)), module, YC::FFD_PARAM));
    addParam(createParam<TrimbotWhite>(mm2px(Vec(7.3f, MHEIGHT-60-6.2)), module, YC::FREQ_PARAM));
    addParam(createParam<TrimbotWhite>(mm2px(Vec(7.3f, MHEIGHT-48-6.2)), module, YC::DEPTH_PARAM));

    addInput(createInput<SmallPort>(mm2px(Vec(2, MHEIGHT-36-6.2)), module, YC::MOD_L_INPUT));
    addInput(createInput<SmallPort>(mm2px(Vec(11.9, MHEIGHT-36-6.2)), module, YC::MOD_R_INPUT));

    addParam(createParam<TrimbotWhite>(mm2px(Vec(7.3, MHEIGHT-22-6.2)), module, YC::MIX_PARAM));

    addOutput(createOutput<SmallPort>(mm2px(Vec(2, MHEIGHT-10-6.2)), module, YC::L_OUTPUT));
    addOutput(createOutput<SmallPort>(mm2px(Vec(11.9, MHEIGHT-10-6.2)), module, YC::R_OUTPUT));

  }
};


Model* modelYC = createModel<YC, YCWidget>("YAC");