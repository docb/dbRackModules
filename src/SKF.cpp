#include "dcb.h"
using simd::float_4;
// algorithm taken from csound Copyright (c) Victor Lazzarini, 2021
template<typename T>
struct SKFFilter {
  T s[2]={};
  T a[2]={};
  T b[2]={};
  T process(T in,T freq, T R_,float piosr) {
    float_4 R = 2.f - R_*2.f;
    T w = simd::tan(freq*piosr);
    T w2 = w*w;
    T fac = 1.f/(1.f + R*w + w2);
    a[0] = w2*fac;
    a[1] = 2.f*w2*fac;
    b[0] = -2.f*(1.f - w2)*fac;
    b[1] = (1.f - R*w + w2)*fac;
    T yy = in - b[0]*s[0] - b[1]*s[1];
    T out = a[0]*yy + a[1]*s[0] + a[0]*s[1];
    s[1] = s[0];
    s[0] = yy;
    return out;
  }
};

struct SKF : Module {
	enum ParamId {
    FREQ_PARAM,FREQ_CV_PARAM,R_PARAM,R_CV_PARAM,PARAMS_LEN
	};
	enum InputId {
    CV_L_INPUT,CV_R_INPUT,FREQ_INPUT,R_INPUT,INPUTS_LEN
	};
	enum OutputId {
		CV_L_OUTPUT,CV_R_OUTPUT,OUTPUTS_LEN
	};
	enum LightId {
		LIGHTS_LEN
	};

  SKFFilter<float_4> filter[4];
  SKFFilter<float_4> filterR[4];
  dsp::TSlewLimiter<float_4> slews[4];
  float slew=0.04f;
	SKF() {
		config(PARAMS_LEN, INPUTS_LEN, OUTPUTS_LEN, LIGHTS_LEN);
    configParam(FREQ_PARAM,4.f,14.f,11.f,"Frequency"," Hz",2,1);
    configInput(FREQ_INPUT,"Freq");
    configParam(FREQ_CV_PARAM,0,1,0,"Freq CV","%",0,100);
    configParam(R_PARAM,0,1,0.5,"R");
    configParam(R_CV_PARAM,0,1,0,"R CV");
    configOutput(CV_L_OUTPUT,"Left");
    configOutput(CV_R_OUTPUT,"Right");
    configInput(CV_L_INPUT,"Left");
    configInput(CV_R_INPUT,"Right");
    configInput(R_INPUT,"R");
    configBypass(CV_L_INPUT,CV_L_OUTPUT);
    configBypass(CV_R_INPUT,CV_R_OUTPUT);
    for(int k=0;k<4;k++) {
      slews[k].setRiseFall(16.f,16.f);
    }
	}

	void process(const ProcessArgs& args) override {
    float piosr=M_PI/APP->engine->getSampleRate();
    float r=params[R_PARAM].getValue();
    float rCvParam = params[R_CV_PARAM].getValue();

    float freqParam = params[FREQ_PARAM].getValue();
    float freqCvParam = params[FREQ_CV_PARAM].getValue();
    int channels=inputs[CV_L_INPUT].getChannels();
    int channelsR=inputs[CV_R_INPUT].getChannels();
    bool freqMod=inputs[FREQ_INPUT].isConnected();
    if(outputs[CV_L_OUTPUT].isConnected()) {
      for(int c=0;c<channels;c+=4) {
        float_4 pitch=freqParam+inputs[FREQ_INPUT].getPolyVoltageSimd<float_4>(c)*freqCvParam;
        if(freqMod) {
          float sl=(slew+0.001f)*1000.f;
          slews[c/4].setRiseFall(1.f/sl,1.f/sl);
          pitch=slews[c/4].process(1,pitch);
        }
        float_4 cutoff=simd::clamp(simd::pow(2.f,pitch),2.f,args.sampleRate*0.33f);
        float_4 R=simd::clamp(r+inputs[R_INPUT].getPolyVoltageSimd<float_4>(c)*rCvParam*0.1f,0.0f,1.f);
        float_4 in=inputs[CV_L_INPUT].getVoltageSimd<float_4>(c);
        float_4 out=filter[c/4].process(in,cutoff,R,piosr);
        outputs[CV_L_OUTPUT].setVoltageSimd(out,c);
      }
    }
    if(outputs[CV_R_OUTPUT].isConnected()) {
      for(int c=0;c<channelsR;c+=4) {
        float_4 pitch=freqParam+inputs[FREQ_INPUT].getPolyVoltageSimd<float_4>(c)*freqCvParam;
        if(freqMod) {
          float sl=(slew+0.001f)*1000.f;
          slews[c/4].setRiseFall(1.f/sl,1.f/sl);
          pitch=slews[c/4].process(1,pitch);
        }
        float_4 cutoff=simd::clamp(simd::pow(2.f,pitch),2.f,args.sampleRate*0.33f);
        float_4 R=simd::clamp(r+inputs[R_INPUT].getPolyVoltageSimd<float_4>(c)*rCvParam*0.1f,0.0f,1.f);
        float_4 in=inputs[CV_R_INPUT].getVoltageSimd<float_4>(c);
        float_4 out=filterR[c/4].process(in,cutoff,R,piosr);
        outputs[CV_R_OUTPUT].setVoltageSimd(out,c);
      }
    }
    outputs[CV_L_OUTPUT].setChannels(channels);
    outputs[CV_R_OUTPUT].setChannels(channelsR);
	}

  json_t *dataToJson() override {
    json_t *data=json_object();
    json_object_set_new(data,"slew",json_real(slew));
    return data;
  }

  void dataFromJson(json_t *rootJ) override {
    json_t *jSlew = json_object_get(rootJ,"slew");
    if(jSlew!=nullptr) slew = json_real_value(jSlew);
  }

};

struct SlewQuantity : Quantity {
  SKF* module;

  SlewQuantity(SKF* m) : module(m) {}

  void setValue(float value) override {
    value = clamp(value, getMinValue(), getMaxValue());
    if (module) {
      module->slew = value;
    }
  }

  float getValue() override {
    if (module) {
      return module->slew;
    }
    return 0.5f;
  }

  float getMinValue() override { return 0.0f; }
  float getMaxValue() override { return 1.0f; }
  float getDefaultValue() override { return 1.f/16.f; }
  float getDisplayValue() override { return getValue(); }
  void setDisplayValue(float displayValue) override { setValue(displayValue); }
  std::string getLabel() override { return "Slew"; }
  std::string getUnit() override { return ""; }
};

struct SlewSlider : ui::Slider {
  SlewSlider(SKF* module) {
    quantity = new SlewQuantity(module);
    box.size.x = 200.0f;
  }
  virtual ~SlewSlider() {
    delete quantity;
  }
};

struct SlewMenuItem : MenuItem {
  SKF* module;

  SlewMenuItem(SKF* m) : module(m) {
    this->text = "Freq Input";
    this->rightText = "▸";
  }

  Menu* createChildMenu() override {
    Menu* menu = new Menu;
    menu->addChild(new SlewSlider(module));
    return menu;
  }
};


struct SKFWidget : ModuleWidget {
	SKFWidget(SKF* module) {
		setModule(module);
		setPanel(createPanel(asset::plugin(pluginInstance, "res/SKF.svg")));
    float y=9;
    float x=1.9;
    addParam(createParam<TrimbotWhite>(mm2px(Vec(x,y)),module,SKF::FREQ_PARAM));
    y+=7;
    addInput(createInput<SmallPort>(mm2px(Vec(x,y)),module,SKF::FREQ_INPUT));
    y+=7;
    addParam(createParam<TrimbotWhite>(mm2px(Vec(x,y)),module,SKF::FREQ_CV_PARAM));
    y=38;
    addParam(createParam<TrimbotWhite>(mm2px(Vec(x,y)),module,SKF::R_PARAM));
    y+=7;
    addInput(createInput<SmallPort>(mm2px(Vec(x,y)),module,SKF::R_INPUT));
    y+=7;
    addParam(createParam<TrimbotWhite>(mm2px(Vec(x,y)),module,SKF::R_CV_PARAM));

    y=80;
    addInput(createInput<SmallPort>(mm2px(Vec(x,y)),module,SKF::CV_L_INPUT));
    y+=12;
    addInput(createInput<SmallPort>(mm2px(Vec(x,y)),module,SKF::CV_R_INPUT));
    y+=12;
    addOutput(createOutput<SmallPort>(mm2px(Vec(x,y)),module,SKF::CV_L_OUTPUT));
    y+=12;
    addOutput(createOutput<SmallPort>(mm2px(Vec(x,y)),module,SKF::CV_R_OUTPUT));
	}

  void appendContextMenu(Menu* menu) override {
    SKF *module=dynamic_cast<SKF *>(this->module);
    assert(module);

    menu->addChild(new MenuSeparator);

    menu->addChild(new SlewMenuItem(module));
  }

};


Model* modelSKF = createModel<SKF, SKFWidget>("SKF");