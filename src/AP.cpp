#include "dcb.h"
using simd::float_4;
template<typename T,size_t S>
struct RB {
  T data[S]={};
  size_t pos=0;
  size_t len=128;

  RB() {
    clear();
  }

  void setLen(unsigned int l) {
    if(l>1&&l<=S)
      len=l;
  }

  void push(T t) {
    data[pos]=t;
    pos++;
    if(pos>=len)
      pos=0;
  }

  T get(int relPos) {
    int idx=pos+relPos;
    while(idx<0)
      idx+=len;
    return data[idx%len];
  }
  T sum(int relPos) {
    while(relPos<0)
      relPos+=len;
    T s= T(0);
    for(int k=relPos;k<len;k++) {
      s+=get(k);
    }
    return s;
  }
  void clear() {
    pos=0;
    len=1024;
    for(size_t k=0;k<S;k++)
      data[k]=0;
  }
};
struct AP : Module {
	enum ParamId {
		FREQ_PARAM,DELAY_PARAM,DELAY_SAMP_PARAM,DELAY_CV_PARAM,FREQ_CV_PARAM,PARAMS_LEN
	};
	enum InputId {
		CV_INPUT,FREQ_INPUT,DELAY_INPUT,INPUTS_LEN
	};
	enum OutputId {
		CV_OUTPUT,OUTPUTS_LEN
	};
	enum LightId {
		LIGHTS_LEN
	};
  RB<float_4,48000> rb[4];
  DCBlocker<float_4> db[4];
  bool highCV=false;

  AP() {
		config(PARAMS_LEN, INPUTS_LEN, OUTPUTS_LEN, LIGHTS_LEN);
    //configParam(FREQ_PARAM,-4.f,8.f,0.f,"Frequency"," Hz",2,dsp::FREQ_C4);
    configParam(FREQ_PARAM,0.001,0.49,0.125,"Frequency");
    configParam(FREQ_CV_PARAM,0,1,0,"Freq CV");
    configInput(CV_INPUT,"CV");
    configInput(FREQ_INPUT,"Freq");
    configInput(DELAY_INPUT,"Delay");
    configParam(DELAY_PARAM,0,1.f,0.f,"Delay (s)");
    configParam(DELAY_SAMP_PARAM,1,48,1,"Delay smps");
    getParamQuantity(DELAY_SAMP_PARAM)->snapEnabled=true;
    configParam(DELAY_CV_PARAM,0,1,0,"Delay CV"," %",0,100);
    configOutput(CV_OUTPUT,"CV");
    for(int k=0;k<4;k++) {
      rb[k].setLen(48000);
    }
    configBypass(CV_INPUT,CV_OUTPUT);
	}

	void process(const ProcessArgs& args) override {
    int channels=inputs[CV_INPUT].getChannels();
    float freq=params[FREQ_PARAM].getValue();
    int del=params[DELAY_SAMP_PARAM].getValue()+
      clamp(inputs[DELAY_INPUT].getVoltage()*(highCV?0.01f:0.001f)*params[DELAY_CV_PARAM].getValue()+params[DELAY_PARAM].getValue(),0.f,0.5f)*args.sampleRate;
    for(int c=0;c<channels;c+=4) {
      float_4 in=inputs[CV_INPUT].getVoltageSimd<float_4>(c);
      float_4 freq4 = freq;
      if(inputs[FREQ_INPUT].isConnected()) {
        freq4=simd::clamp(freq4+inputs[FREQ_INPUT].getPolyVoltageSimd<float_4>(c)*0.05f*params[FREQ_CV_PARAM].getValue(),0.001f,0.49f);
      }
      float_4 t=simd::tan(M_PI*freq4);
      float_4 a1=(t-1.f)/(t+1.f);
      float_4 y=in*a1+rb[c/4].get(-del);
      rb[c/4].push(in-a1*y);
      outputs[CV_OUTPUT].setVoltageSimd(db[c/4].process(y),c);
    }
    outputs[CV_OUTPUT].setChannels(channels);
	}

  json_t *dataToJson() override {
    json_t *data=json_object();
    json_object_set_new(data,"highCV",json_boolean(highCV));
    return data;
  }

  void dataFromJson(json_t *rootJ) override {
    json_t *jhighCV = json_object_get(rootJ,"highCV");
    if(jhighCV!=nullptr) highCV = json_boolean_value(jhighCV);
  }

};

struct APWidget : ModuleWidget {
	APWidget(AP* module) {
		setModule(module);
		setPanel(createPanel(asset::plugin(pluginInstance, "res/AP.svg")));
    float x=1.9;
    addParam(createParam<TrimbotWhite>(mm2px(Vec(x,10)),module,AP::FREQ_PARAM));
    addInput(createInput<SmallPort>(mm2px(Vec(x,18)),module,AP::FREQ_INPUT));
    addParam(createParam<TrimbotWhite>(mm2px(Vec(x,26)),module,AP::FREQ_CV_PARAM));
    addParam(createParam<TrimbotWhite>(mm2px(Vec(x,54)),module,AP::DELAY_PARAM));
    addParam(createParam<TrimbotWhite>(mm2px(Vec(x,66)),module,AP::DELAY_SAMP_PARAM));
    addInput(createInput<SmallPort>(mm2px(Vec(x,74)),module,AP::DELAY_INPUT));
    addParam(createParam<TrimbotWhite>(mm2px(Vec(x,82)),module,AP::DELAY_CV_PARAM));
    addInput(createInput<SmallPort>(mm2px(Vec(x,104)),module,AP::CV_INPUT));
    addOutput(createOutput<SmallPort>(mm2px(Vec(x,116)),module,AP::CV_OUTPUT));
  }

  void appendContextMenu(Menu* menu) override {
    AP *module=dynamic_cast<AP *>(this->module);
    assert(module);
    menu->addChild(new MenuSeparator);
    menu->addChild(createBoolPtrMenuItem("High CV","",&module->highCV));
  }

};


Model* modelAP = createModel<AP, APWidget>("AP");