#include "dcb.h"
using simd::float_4;
template<typename T>
struct ParaSinOsc {
  T phs=0.f;
  float fdp[16];
  float pih=M_PI/2;

  ParaSinOsc() {
    for(int k=2;k<18;k++)
      fdp[k-2]=ipowf(2,k)/ipowf(float(M_PI),k);
  }
  float ipowf(float base, int exp)
  {
    float result = 1;
    for (;;)
    {
      if (exp & 1)
        result *= base;
      exp >>= 1;
      if (!exp)
        break;
      base *= base;
    }
    return result;
  }
  T ipow(T base, int exp)
  {
    T result = 1;
    for (;;)
    {
      if (exp & 1)
        result *= base;
      exp >>= 1;
      if (!exp)
        break;
      base *= base;
    }
    return result;
  }

  void updatePhs(float sampleTime,T freq) {
    phs+=simd::fmin(freq*sampleTime,0.5f);
    phs-=simd::floor(phs);
  }
  void updateExtPhs(T phase) {
    phs=simd::fmod(phase,1);
  }
  T firstHalf(T po,int k) {
    T x=po*TWOPIF-pih;
    return (-fdp[k-2]*simd::fabs(ipow(x,k))+1)*.5f+.5f;
  }

  T secondHalf(T po,int k) {
    T x=(po-1.f)*TWOPIF+pih;
    return (simd::fabs(fdp[k-2]*ipow(x,k))-1)*.5f+.5f;
  }

  T process(int k) {
    return simd::ifelse(phs<0.5f,firstHalf(phs,k),secondHalf(phs,k));
  }

  void reset(T trigger) {
    phs=simd::ifelse(trigger,0.f,phs);
  }
};
struct PRB : Module {
	enum ParamId {
    FREQ_PARAM,FM_PARAM,LIN_PARAM,FINE_PARAM,DEGREE_PARAM,PARAMS_LEN
	};
	enum InputId {
    VOCT_INPUT,RST_INPUT,FM_INPUT,PHS_INPUT,INPUTS_LEN
	};
	enum OutputId {
		CV_OUTPUT,OUTPUTS_LEN
	};
	enum LightId {
		LIGHTS_LEN
	};
  ParaSinOsc<float_4> osc[4];

  bool dcBlock=false;
  dsp::TSchmittTrigger<float_4> rstTriggers4[4];
  DCBlocker<float_4> dcb[4];
	PRB() {
		config(PARAMS_LEN, INPUTS_LEN, OUTPUTS_LEN, LIGHTS_LEN);
    configParam(FREQ_PARAM,-14.f,4.f,0.f,"Frequency"," Hz",2,dsp::FREQ_C4);
    configParam(FM_PARAM,0,1,0,"FM Amount","%",0,100);
    configParam(FINE_PARAM,-100,100,0,"Fine tune"," cents");
    configParam(DEGREE_PARAM,2,17,2,"Degree");
    getParamQuantity(DEGREE_PARAM)->snapEnabled=true;
    configInput(FM_INPUT,"FM");
    configButton(LIN_PARAM,"Linear");
    configInput(VOCT_INPUT,"V/Oct");
    configInput(PHS_INPUT,"Phase");
    configInput(RST_INPUT,"Rst");
    configOutput(CV_OUTPUT,"CV");
	}

	void process(const ProcessArgs& args) override {
    float freqParam=params[FREQ_PARAM].getValue();
    float fmParam=params[FM_PARAM].getValue();
    bool linear=params[LIN_PARAM].getValue()>0;
    float fineTune=params[FINE_PARAM].getValue();
    int deg=params[DEGREE_PARAM].getValue();
    int channels=std::max(inputs[VOCT_INPUT].getChannels(),1);
    if(inputs[PHS_INPUT].isConnected())
      channels=inputs[PHS_INPUT].getChannels();
    for(int c=0;c<channels;c+=4) {
      auto *oscil=&osc[c/4];
      if(inputs[PHS_INPUT].isConnected()) {
        oscil->updateExtPhs(inputs[PHS_INPUT].getVoltageSimd<float_4>(c)/10.f+0.5f);
      } else {
        float_4 pitch=freqParam+inputs[VOCT_INPUT].getPolyVoltageSimd<float_4>(c) + fineTune/1200.f;;
        float_4 freq;
        if(linear) {
          freq=dsp::FREQ_C4*dsp::approxExp2_taylor5(pitch+30.f)/std::pow(2.f,30.f);
          freq+=dsp::FREQ_C4*inputs[FM_INPUT].getPolyVoltageSimd<float_4>(c)*fmParam;
        } else {
          pitch+=inputs[FM_INPUT].getPolyVoltageSimd<float_4>(c)*fmParam;
          freq=dsp::FREQ_C4*dsp::approxExp2_taylor5(pitch+30.f)/std::pow(2.f,30.f);
        }
        freq=simd::fmin(freq,args.sampleRate/2);
        oscil->updatePhs(args.sampleTime,freq);
      }
      float_4 rst = inputs[RST_INPUT].getPolyVoltageSimd<float_4>(c);
      float_4 resetTriggered = rstTriggers4[c/4].process(rst, 0.1f, 2.f);
      oscil->reset(resetTriggered);
      float_4 out=oscil->process(deg)*10.f-5.f;
      if(outputs[CV_OUTPUT].isConnected())
        outputs[CV_OUTPUT].setVoltageSimd(dcBlock?dcb[c/4].process(out):out,c);
    }
    outputs[CV_OUTPUT].setChannels(channels);
	}

  json_t *dataToJson() override {
    json_t *data=json_object();
    json_object_set_new(data,"dcBlock",json_boolean(dcBlock));
    return data;
  }

  void dataFromJson(json_t *rootJ) override {
    json_t *jDcBlock = json_object_get(rootJ,"dcBlock");
    if(jDcBlock!=nullptr) dcBlock = json_boolean_value(jDcBlock);
  }
};


struct PRBWidget : ModuleWidget {
	PRBWidget(PRB* module) {
		setModule(module);
		setPanel(createPanel(asset::plugin(pluginInstance, "res/PRB.svg")));

    float x=1.9;
    addParam(createParam<TrimbotWhite>(mm2px(Vec(x,9)),module,PRB::FREQ_PARAM));
    addParam(createParam<TrimbotWhite>(mm2px(Vec(x,21)),module,PRB::FINE_PARAM));
    addInput(createInput<SmallPort>(mm2px(Vec(x,33)),module,PRB::VOCT_INPUT));
    addInput(createInput<SmallPort>(mm2px(Vec(x,46)),module,PRB::FM_INPUT));
    addParam(createParam<TrimbotWhite>(mm2px(Vec(x,53)),module,PRB::FM_PARAM));
    addParam(createParam<MLED>(mm2px(Vec(x,65)),module,PRB::LIN_PARAM));
    addInput(createInput<SmallPort>(mm2px(Vec(x,77)),module,PRB::RST_INPUT));
    addInput(createInput<SmallPort>(mm2px(Vec(x,92)),module,PRB::PHS_INPUT));
    addParam(createParam<TrimbotWhite>(mm2px(Vec(x,104)),module,PRB::DEGREE_PARAM));
    addOutput(createOutput<SmallPort>(mm2px(Vec(x,116)),module,PRB::CV_OUTPUT));
	}
  void appendContextMenu(Menu* menu) override {
    PRB *module=dynamic_cast<PRB *>(this->module);
    assert(module);

    menu->addChild(new MenuSeparator);

    menu->addChild(createBoolPtrMenuItem("Block DC","",&module->dcBlock));
  }
};


Model* modelPRB = createModel<PRB, PRBWidget>("PRB");