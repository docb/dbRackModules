#include "dcb.h"

using simd::float_4;

template<typename T>
struct SineOsc {
  T phs=0.f;
  float f1=2.f-M_PI/4.f;
  float f2=1.f-M_PI/4.f;
  float g1=TWOPIF-5.f;
  float g2=M_PI-3;
  float zero=cosf(0.3*M_PI);

  void updatePhs(float sampleTime,T freq) {
    phs+=simd::fmin(freq*sampleTime,0.5f);
    phs-=simd::floor(phs);
  }

  void updateExtPhs(T phase) {
    phs=simd::fmod(phase,1);
  }

  T sin2pi_pade_05_5_4(T x) {
    x-=0.5f;
    T x2=x*x;
    T x3=x2*x;
    T x4=x2*x2;
    T x5=x2*x3;
    return (T(-6.283185307)*x+T(33.19863968)*x3-T(32.44191367)*x5)/
           (1+T(1.296008659)*x2+T(0.7028072946)*x4);
  }

  T process554() {
    return sin2pi_pade_05_5_4(phs);
  }

  T firstHalf4P(T po) {
    T x=po*4.f-1.f;
    T x2=x*x;
    return 1-x2*(f1-x2*f2);
  }

  T secondHalf4P(T po) {
    T x=(po-0.5f)*4.f-1.f;
    T x2=x*x;
    return -1.f*(1.f-x2*(f1-x2*f2));
  }

  T process4P() {
    return simd::ifelse(phs<0.5f,firstHalf4P(phs),secondHalf4P(phs));
  }

  T firstHalf5P(T po) {
    T x=po*4.f-1.f;
    T x2=x*x;
    return 0.5f*(x*(M_PI-x2*(g1-x2*g2)));
  }

  T secondHalf5P(T po) {
    T x=(po-0.5f)*4.f-1.f;
    T x2=x*x;
    return -0.5f*(x*(M_PI-x2*(g1-x2*g2)));
  }

  T process5P() {
    return simd::ifelse(phs<0.5f,firstHalf5P(phs),secondHalf5P(phs));
  }


  T firstHalfBI(T po) {
    T z=po*(.5f-po);
    return 4.f*z/(.3125f-z);
  }

  T secondHalfBI(T po) {
    T z=(po-0.5f)*(po-1.f);
    return 4.f*z/(.3125f+z);
  }

  T processBI() {
    return simd::ifelse(phs<0.5f,firstHalfBI(phs),secondHalfBI(phs));
  }

  T process(int type) {
    switch(type) {
      case 0:
        return process4();
      case 1:
        return process5();
      case 2:
        return process6();
      case 3:
        return process4P();
      case 4:
        return process5P();
      case 5:
        return process554();
      case 6:
        return processBI();
      default:
        return 0.f;
    }
  }

  T process4() {
    T x=phs*4.f-2.f;
    T x2=x*x;
    T x4=x2*x2;
    return 0.125*x4-x2+1.f;
  }

  T process6() {
    T x=phs-0.5;
    T x2=x*x;
    T x4=x2*x2;
    T x6=x4*x2;
    return 32.f*x6-48.f*x4+18.f*x2-1;
  }

  T process5() {
    T x=phs*2*zero-zero;
    T x2=x*x;
    T x3=x*x2;
    T x5=x3*x2;
    return 16.f*x5-20.f*x3+5.f*x;
  }

  void reset(T trigger) {
    phs=simd::ifelse(trigger,0.f,phs);
  }
};


struct FS6 : Module {
  enum ParamId {
    FREQ_PARAM,FM_PARAM,LIN_PARAM,FINE_PARAM,OSC_PARAM,PARAMS_LEN
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

  SineOsc<float_4> osc[4];

  bool dcBlock=false;
  dsp::TSchmittTrigger<float_4> rstTriggers4[4];
  DCBlocker<float_4> dcb[4];

  FS6() {
    config(PARAMS_LEN,INPUTS_LEN,OUTPUTS_LEN,LIGHTS_LEN);
    configParam(FREQ_PARAM,-14.f,4.f,0.f,"Frequency"," Hz",2,dsp::FREQ_C4);
    configParam(FM_PARAM,0,3,0,"FM Amount","%",0,100);
    configParam(FINE_PARAM,-100,100,0,"Fine tune"," cents");
    configSwitch(OSC_PARAM,0,6,4,"Osc",{"P4","P5","P6","P4P","P5P","Pad554","BI"});
    getParamQuantity(OSC_PARAM)->snapEnabled=true;
    configInput(FM_INPUT,"FM");
    configButton(LIN_PARAM,"Linear");
    configInput(VOCT_INPUT,"V/Oct");
    configInput(PHS_INPUT,"Phase");
    configInput(RST_INPUT,"Rst");
    configOutput(CV_OUTPUT,"CV");
  }

  void process(const ProcessArgs &args) override {
    float freqParam=params[FREQ_PARAM].getValue();
    float fmParam=params[FM_PARAM].getValue();
    bool linear=params[LIN_PARAM].getValue()>0;
    float fineTune=params[FINE_PARAM].getValue();
    int channels=std::max(inputs[VOCT_INPUT].getChannels(),1);
    int oscType=params[OSC_PARAM].getValue();
    if(inputs[PHS_INPUT].isConnected())
      channels=inputs[PHS_INPUT].getChannels();
    for(int c=0;c<channels;c+=4) {
      auto *oscil=&osc[c/4];
      if(inputs[PHS_INPUT].isConnected()) {
        oscil->updateExtPhs(inputs[PHS_INPUT].getVoltageSimd<float_4>(c)/10.f+0.5f);
      } else {
        float_4 pitch=freqParam+inputs[VOCT_INPUT].getPolyVoltageSimd<float_4>(c)+fineTune/1200.f;;
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
      float_4 rst=inputs[RST_INPUT].getPolyVoltageSimd<float_4>(c);
      float_4 resetTriggered=rstTriggers4[c/4].process(rst,0.1f,2.f);
      oscil->reset(resetTriggered);
      float_4 out=oscil->process(oscType)*5.f;
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
    json_t *jDcBlock=json_object_get(rootJ,"dcBlock");
    if(jDcBlock!=nullptr) dcBlock=json_boolean_value(jDcBlock);
  }
};


struct FS6Widget : ModuleWidget {
  FS6Widget(FS6 *module) {
    setModule(module);
    setPanel(createPanel(asset::plugin(pluginInstance,"res/FS6.svg")));
    float x=1.9;
    addParam(createParam<TrimbotWhite>(mm2px(Vec(x,9)),module,FS6::FREQ_PARAM));
    addParam(createParam<TrimbotWhite>(mm2px(Vec(x,21)),module,FS6::FINE_PARAM));
    addInput(createInput<SmallPort>(mm2px(Vec(x,33)),module,FS6::VOCT_INPUT));
    addInput(createInput<SmallPort>(mm2px(Vec(x,46)),module,FS6::FM_INPUT));
    addParam(createParam<TrimbotWhite>(mm2px(Vec(x,53)),module,FS6::FM_PARAM));
    addParam(createParam<MLED>(mm2px(Vec(x,65)),module,FS6::LIN_PARAM));
    addInput(createInput<SmallPort>(mm2px(Vec(x,77)),module,FS6::RST_INPUT));
    addInput(createInput<SmallPort>(mm2px(Vec(x,92)),module,FS6::PHS_INPUT));
    addParam(createParam<TrimbotWhite>(mm2px(Vec(x,104)),module,FS6::OSC_PARAM));
    addOutput(createOutput<SmallPort>(mm2px(Vec(x,116)),module,FS6::CV_OUTPUT));
  }

  void appendContextMenu(Menu *menu) override {
    FS6 *module=dynamic_cast<FS6 *>(this->module);
    assert(module);
    menu->addChild(new MenuSeparator);
    menu->addChild(createBoolPtrMenuItem("Block DC","",&module->dcBlock));
  }
};


Model *modelFS6=createModel<FS6,FS6Widget>("FS6");