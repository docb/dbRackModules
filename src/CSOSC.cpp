#include "dcb.h"
#include "filter.hpp"

using simd::float_4;

template<typename T>
struct COSC {
  T phase=0.f;

  void updatePhs(float sampleTime,T freq) {
    phase+=(sampleTime*freq);
    phase-=simd::floor(phase);
  }

  void updateExtPhs(T phs) {
    phase=simd::fmod(phs,1);
  }

  T process(T skew,T clip) {
    T cl1=skew*(1-clip);
    T cl2=(1-skew)*clip;
    T ct1=cl1;
    T ct2=1-cl2;
    T modPhs=simd::ifelse(phase<skew,simd::ifelse(phase<ct1,phase*(0.5/ct1),0.5),simd::ifelse(phase<ct2,0.5+(phase-skew)*(0.5/(ct2-skew)),1));
    return modPhs;
  }

  T process(T skew,T clip,T fms) {
    phase+=fms;
    phase-=simd::floor(phase);
    T cl1=skew*(1-clip);
    T cl2=(1-skew)*clip;
    T ct1=cl1;
    T ct2=1-cl2;
    T modPhs=simd::ifelse(phase<skew,simd::ifelse(phase<ct1,phase*(0.5/ct1),0.5),simd::ifelse(phase<ct2,0.5+(phase-skew)*(0.5/(ct2-skew)),1));
    return modPhs;
  }
};

struct CSOSC : Module {
  enum ParamId {
    FREQ_PARAM,SKEW_PARAM,CLIP_PARAM,SKEW_CV_PARAM,CLIP_CV_PARAM,FM_PARAM,LIN_PARAM,PARAMS_LEN
  };
  enum InputId {
    VOCT_INPUT,PHS_INPUT,SKEW_INPUT,CLIP_INPUT,FM_INPUT,INPUTS_LEN
  };
  enum OutputId {
    PHS_OUTPUT,CV_OUTPUT,OUTPUTS_LEN
  };
  enum LightId {
    LIGHTS_LEN
  };
  COSC<float_4> osc[4];
  Cheby1_32_BandFilter<float_4> filter24[4];
  bool oversample=false;
  bool blockDC=false;

  CSOSC() {
    config(PARAMS_LEN,INPUTS_LEN,OUTPUTS_LEN,LIGHTS_LEN);
    configParam(FREQ_PARAM,-14.f,4.f,0.f,"Frequency"," Hz",2,dsp::FREQ_C4);
    configParam(SKEW_PARAM,0,1,0.5,"Skew");
    configParam(CLIP_PARAM,0,1,0,"Clip");
    configParam(SKEW_CV_PARAM,0,1,0,"Skew Amt"," %",0,100);
    configParam(CLIP_CV_PARAM,0,1,0,"Clip Amt"," %",0,100);
    configInput(VOCT_INPUT,"V/Oct");
    configInput(SKEW_INPUT,"Skew");
    configInput(CLIP_INPUT,"Clip");
    configInput(PHS_INPUT,"Phs");
    configOutput(CV_OUTPUT,"CV");
    configOutput(PHS_OUTPUT,"Phs");
    configButton(LIN_PARAM,"Linear");
    configParam(FM_PARAM,0,1,0,"FM Amount","%",0,100);
    configInput(FM_INPUT,"FM");
  }

  void process(const ProcessArgs &args) override {
    float freqParam=params[FREQ_PARAM].getValue();
    float fmParam=params[FM_PARAM].getValue();
    bool linear=params[LIN_PARAM].getValue()>0;
    int channels=std::max(inputs[VOCT_INPUT].getChannels(),1);
    if(inputs[PHS_INPUT].isConnected())
      channels=inputs[PHS_INPUT].getChannels();
    float skew=params[SKEW_PARAM].getValue();
    float clip=params[CLIP_PARAM].getValue();
    bool over = oversample && !inputs[PHS_INPUT].isConnected() && outputs[CV_OUTPUT].isConnected();
    float_4 freq=0.f;
    for(int c=0;c<channels;c+=4) {
      auto *oscil=&osc[c/4];
      if(inputs[PHS_INPUT].isConnected()) {
        oscil->updateExtPhs(inputs[PHS_INPUT].getVoltageSimd<float_4>(c)/10.f+0.5f);
      } else {
        float_4 pitch=freqParam+inputs[VOCT_INPUT].getPolyVoltageSimd<float_4>(c);
        if(linear) {
          freq=dsp::FREQ_C4*dsp::approxExp2_taylor5(pitch+30.f)/std::pow(2.f,30.f);
          freq+=dsp::FREQ_C4*inputs[FM_INPUT].getPolyVoltageSimd<float_4>(c)*fmParam;
        } else {
          pitch+=inputs[FM_INPUT].getPolyVoltageSimd<float_4>(c)*fmParam;
          freq=dsp::FREQ_C4*dsp::approxExp2_taylor5(pitch+30.f)/std::pow(2.f,30.f);
        }
        freq=simd::fmin(freq,args.sampleRate/2);
        if(!over) oscil->updatePhs(args.sampleTime,freq);
      }
      float_4 skewIn=simd::clamp(inputs[SKEW_INPUT].getVoltageSimd<float_4>(c)*params[SKEW_CV_PARAM].getValue()*0.1+skew,0.f,1.f);
      float_4 clipIn=simd::clamp(inputs[CLIP_INPUT].getVoltageSimd<float_4>(c)*params[CLIP_CV_PARAM].getValue()*0.1+clip,0.f,1.f);
      if(over) {
        float_4 fms=freq*args.sampleTime/16.f;
        float_4 out;
        float_4 phs;
        for(int k=0;k<16;k++) {
          phs=oscil->process(skewIn,clipIn,fms);
          out=simd::cos(phs*TWOPIF);
          out=filter24[c/4].process(out);
        }
        outputs[PHS_OUTPUT].setVoltageSimd(phs*10.f-5.f,c);
        outputs[CV_OUTPUT].setVoltageSimd(out*5.f,c);
      } else {
        float_4 out=oscil->process(skewIn,clipIn);
        outputs[PHS_OUTPUT].setVoltageSimd(out*10.f-5.f,c);
        if(outputs[CV_OUTPUT].isConnected())
          outputs[CV_OUTPUT].setVoltageSimd(simd::cos(out*TWOPIF)*5.f,c);
      }
    }
    outputs[CV_OUTPUT].setChannels(channels);
    outputs[PHS_OUTPUT].setChannels(channels);
  }

  json_t *dataToJson() override {
    json_t *data=json_object();
    json_object_set_new(data,"oversample",json_boolean(oversample));
    json_object_set_new(data,"blockDC",json_boolean(blockDC));
    return data;
  }

  void dataFromJson(json_t *rootJ) override {
    json_t *jOverSample = json_object_get(rootJ,"oversample");
    if(jOverSample!=nullptr) oversample = json_boolean_value(jOverSample);
    json_t *jDcBlock = json_object_get(rootJ,"blockDC");
    if(jDcBlock!=nullptr) blockDC = json_boolean_value(jDcBlock);
  }
};


struct CSOSCWidget : ModuleWidget {
  CSOSCWidget(CSOSC *module) {
    setModule(module);
    setPanel(createPanel(asset::plugin(pluginInstance,"res/CSOSC.svg")));

    addChild(createWidget<ScrewSilver>(Vec(0,0)));
    addChild(createWidget<ScrewSilver>(Vec(box.size.x-15,0)));
    addChild(createWidget<ScrewSilver>(Vec(0,365)));
    addChild(createWidget<ScrewSilver>(Vec(box.size.x-15,365)));

    addParam(createParam<TrimbotWhite9>(mm2px(Vec(6,13.5)),module,CSOSC::FREQ_PARAM));
    addInput(createInput<SmallPort>(mm2px(Vec(2,28)),module,CSOSC::FM_INPUT));
    addParam(createParam<TrimbotWhite>(mm2px(Vec(11.9,28)),module,CSOSC::FM_PARAM));
    addInput(createInput<SmallPort>(mm2px(Vec(2,42)),module,CSOSC::VOCT_INPUT));
    addParam(createParam<MLED>(mm2px(Vec(11.9,42)),module,CSOSC::LIN_PARAM));

    addParam(createParam<TrimbotWhite>(mm2px(Vec(2,57)),module,CSOSC::SKEW_PARAM));
    addParam(createParam<TrimbotWhite>(mm2px(Vec(11.9,57)),module,CSOSC::CLIP_PARAM));
    addInput(createInput<SmallPort>(mm2px(Vec(2,67)),module,CSOSC::SKEW_INPUT));
    addInput(createInput<SmallPort>(mm2px(Vec(11.9,67)),module,CSOSC::CLIP_INPUT));
    addParam(createParam<TrimbotWhite>(mm2px(Vec(2,77)),module,CSOSC::SKEW_CV_PARAM));
    addParam(createParam<TrimbotWhite>(mm2px(Vec(11.9,77)),module,CSOSC::CLIP_CV_PARAM));

    addInput(createInput<SmallPort>(mm2px(Vec(7,94)),module,CSOSC::PHS_INPUT));
    addOutput(createOutput<SmallPort>(mm2px(Vec(2,112)),module,CSOSC::PHS_OUTPUT));
    addOutput(createOutput<SmallPort>(mm2px(Vec(11.9,112)),module,CSOSC::CV_OUTPUT));
  }

  void appendContextMenu(Menu* menu) override {
    auto module=dynamic_cast<CSOSC *>(this->module);
    assert(module);
    menu->addChild(new MenuSeparator);
    menu->addChild(createBoolPtrMenuItem("Oversample","",&module->oversample));
    //menu->addChild(createBoolPtrMenuItem("block DC","",&module->blockDC));
  }
};


Model *modelCSOSC=createModel<CSOSC,CSOSCWidget>("CSOSC");