#include "dcb.h"
#include "filter.hpp"
using simd::float_4;
template<typename T>
struct SinOscP {
  T phs=0.f;
  float pih=M_PI/2;
  float fdp2=4/(M_PI*M_PI);

  void updatePhs(T fms) {
    phs+=simd::fmin(fms,0.5f);
    phs-=simd::floor(phs);
  }
  T firstHalf(T po) {
    T x=po*TWOPIF-pih;
    return (-fdp2*x*x+1);
  }

  T secondHalf(T po) {
    T x=(po-1.f)*TWOPIF+pih;
    return (simd::fabs(fdp2*x*x)-1);
  }

  T process(T ofs) {
    T po=(phs+ofs) - simd::floor(phs+ofs);
    return simd::ifelse(po<0.5f,firstHalf(po),secondHalf(po));
  }

  void reset(T trigger) {
    phs=simd::ifelse(trigger,0.f,phs);
  }
};

struct Osc2 : Module {
  enum ParamId {
    FREQ_PARAM,FM_PARAM,LIN_PARAM,PHS_PARAM,PHS_CV_PARAM,RST_PARAM,PARAMS_LEN
  };
  enum InputId {
    VOCT_INPUT,VOCT2_INPUT,PHS_INPUT,FM_INPUT,RST_INPUT,INPUTS_LEN
  };
  enum OutputId {
    MAX_OUTPUT,CLIP_OUTPUT,OUTPUTS_LEN
  };
  enum LightId {
    LIGHTS_LEN
  };

  SinOscP<float_4> osc1p[4];
  SinOscP<float_4> osc2p[4];
  DCBlocker<float_4> dcBlock[4];
  DCBlocker<float_4> dcBlock2[4];
  dsp::TSchmittTrigger<float_4> rstTriggers[4];
  dsp::SchmittTrigger manualRst;
  bool oversample=false;
  Cheby1_32_BandFilter<float_4> maxFilter[4];
  Cheby1_32_BandFilter<float_4> clipFilter[4];

  Osc2() {
    config(PARAMS_LEN,INPUTS_LEN,OUTPUTS_LEN,LIGHTS_LEN);
    configParam(FREQ_PARAM,-8.f,4.f,0.f,"Frequency"," Hz",2,dsp::FREQ_C4);
    configInput(VOCT_INPUT,"V/Oct 1");
    configInput(VOCT2_INPUT,"V/Oct 2");
    configParam(PHS_PARAM,0,1,0,"Phs Shift","%",0,100);
    configParam(PHS_CV_PARAM,0,1,0,"Phs CV","%",0,100);
    configInput(PHS_INPUT,"Phs");
    configInput(RST_INPUT,"Rst");
    configOutput(MAX_OUTPUT,"Max");
    configOutput(CLIP_OUTPUT,"Clip");
    configButton(LIN_PARAM,"Linear");
    configParam(FM_PARAM,0,1,0,"FM Amount","%",0,100);
    configInput(FM_INPUT,"FM");
    configButton(RST_PARAM,"RST");
  }

  void onReset(const ResetEvent &e) override {
    Module::onReset(e);
  }

  void process(const ProcessArgs &args) override {
    float freqParam=params[FREQ_PARAM].getValue();
    float fmParam=params[FM_PARAM].getValue();
    bool linear=params[LIN_PARAM].getValue()>0;
    int channels=std::max(inputs[VOCT_INPUT].getChannels(),1);
    float phsParam=params[PHS_PARAM].getValue();
    float phsCV=params[PHS_CV_PARAM].getValue();

    for(int c=0;c<channels;c+=4) {
      auto *dcb=&dcBlock[c/4];
      auto *dcb2=&dcBlock2[c/4];
      float_4 pitch1=freqParam+inputs[VOCT_INPUT].getVoltageSimd<float_4>(c);
      float_4 pitch2=pitch1;
      if(inputs[VOCT2_INPUT].isConnected()) pitch2=freqParam+inputs[VOCT2_INPUT].getPolyVoltageSimd<float_4>(c);

      float_4 freq1;
      if(linear) {
        freq1=dsp::FREQ_C4*dsp::approxExp2_taylor5(pitch1+30.f)/std::pow(2.f,30.f);
        freq1+=dsp::FREQ_C4*inputs[FM_INPUT].getPolyVoltageSimd<float_4>(c)*fmParam;
      } else {
        pitch1+=inputs[FM_INPUT].getPolyVoltageSimd<float_4>(c)*fmParam;
        freq1=dsp::FREQ_C4*dsp::approxExp2_taylor5(pitch1+30.f)/std::pow(2.f,30.f);
      }
      freq1=simd::fmin(freq1,args.sampleRate/2);
      float_4 freq2;
      if(linear) {
        freq2=dsp::FREQ_C4*dsp::approxExp2_taylor5(pitch2+30.f)/std::pow(2.f,30.f);
        freq2+=dsp::FREQ_C4*inputs[FM_INPUT].getPolyVoltageSimd<float_4>(c)*fmParam;
      } else {
        pitch2+=inputs[FM_INPUT].getPolyVoltageSimd<float_4>(c)*fmParam;
        freq2=dsp::FREQ_C4*dsp::approxExp2_taylor5(pitch2+30.f)/std::pow(2.f,30.f);
      }
      freq2=simd::fmin(freq2,args.sampleRate/2);

      float_4 rst = inputs[RST_INPUT].getPolyVoltageSimd<float_4>(c);
      float_4 resetTriggered = rstTriggers[c/4].process(rst, 0.1f, 2.f) | manualRst.process(params[RST_PARAM].getValue());
      osc1p[c/4].reset(resetTriggered);
      osc2p[c/4].reset(resetTriggered);

      float_4 phsOffset=phsParam;
      if(inputs[PHS_INPUT].isConnected()) {
        phsOffset=phsParam+(simd::clamp(inputs[PHS_INPUT].getPolyVoltageSimd<float_4>(c),-5.f,5.f)*0.1f+0.5f)*phsCV;
      }
      bool maxConnected=outputs[MAX_OUTPUT].isConnected();
      bool clipConnected=outputs[CLIP_OUTPUT].isConnected();
      if(oversample) {
        float_4 maxOut;
        float_4 clipOut;
        float_4 fms1=(freq1/16.f)*args.sampleTime;
        float_4 fms2=(freq2/16.f)*args.sampleTime;
        for(int k=0;k<16;k++) {
          osc1p[c/4].updatePhs(fms1);
          float_4 o1=osc1p[c/4].process(0);
          osc2p[c/4].updatePhs(fms2);
          float_4 o2=osc2p[c/4].process(phsOffset);
          if(maxConnected) {
            maxOut=simd::fmax(o1,o2);
            maxOut=maxFilter[c/4].process(maxOut);
          }
          if(clipConnected) {
            float_4 bAbs=simd::fabs(o2);
            clipOut=simd::ifelse(bAbs<o1,bAbs,simd::ifelse(o1<-bAbs,-bAbs,o1));
            clipOut=clipFilter[c/4].process(clipOut);
          }
        }
        if(maxConnected) {
          maxOut=dcb->process(maxOut);
          outputs[MAX_OUTPUT].setVoltageSimd(maxOut*5.f,c);
        }
        if(clipConnected) {
          clipOut=dcb2->process(clipOut);
          outputs[CLIP_OUTPUT].setVoltageSimd(clipOut*5.f,c);
        }

      } else {
        osc1p[c/4].updatePhs(args.sampleTime*freq1);
        float_4 o1=osc1p[c/4].process(0);
        osc2p[c/4].updatePhs(args.sampleTime*freq2);
        float_4 o2=osc2p[c/4].process(phsOffset);
        if(maxConnected) {
          float_4 out=dcb->process(simd::fmax(o1,o2));
          outputs[MAX_OUTPUT].setVoltageSimd(out*5.f,c);
        }
        if(clipConnected) {
          float_4 bAbs=simd::fabs(o2);
          float_4 a=simd::ifelse(bAbs<o1,bAbs,simd::ifelse(o1<-bAbs,-bAbs,o1));
          float_4 out=dcb2->process(a);
          outputs[CLIP_OUTPUT].setVoltageSimd(out*5.f,c);
        }
      }
    }
    outputs[MAX_OUTPUT].setChannels(channels);
    outputs[CLIP_OUTPUT].setChannels(channels);
  }

  json_t *dataToJson() override {
    json_t *data=json_object();
    json_object_set_new(data,"oversample",json_boolean(oversample));
    return data;
  }

  void dataFromJson(json_t *rootJ) override {
    json_t *jOverSample = json_object_get(rootJ,"oversample");
    if(jOverSample!=nullptr) oversample = json_boolean_value(jOverSample);
  }

};


struct Osc2Widget : ModuleWidget {
  Osc2Widget(Osc2 *module) {
    setModule(module);
    setPanel(createPanel(asset::plugin(pluginInstance,"res/Osc2.svg")));

    addChild(createWidget<ScrewSilver>(Vec(0,0)));
    addChild(createWidget<ScrewSilver>(Vec(box.size.x-15,0)));
    addChild(createWidget<ScrewSilver>(Vec(0,365)));
    addChild(createWidget<ScrewSilver>(Vec(box.size.x-15,365)));

    addParam(createParam<TrimbotWhite9>(mm2px(Vec(6,13.5)),module,Osc2::FREQ_PARAM));
    addInput(createInput<SmallPort>(mm2px(Vec(2,28)),module,Osc2::FM_INPUT));
    addParam(createParam<TrimbotWhite>(mm2px(Vec(11.9,28)),module,Osc2::FM_PARAM));
    addParam(createParam<MLED>(mm2px(Vec(7,37)),module,Osc2::LIN_PARAM));

    addInput(createInput<SmallPort>(mm2px(Vec(2,52)),module,Osc2::VOCT_INPUT));
    addInput(createInput<SmallPort>(mm2px(Vec(11.9,52)),module,Osc2::VOCT2_INPUT));

    addParam(createParam<TrimbotWhite>(mm2px(Vec(7,64)),module,Osc2::PHS_PARAM));
    addInput(createInput<SmallPort>(mm2px(Vec(7,72)),module,Osc2::PHS_INPUT));
    addParam(createParam<TrimbotWhite>(mm2px(Vec(7,80)),module,Osc2::PHS_CV_PARAM));
    addParam(createParam<MLEDM>(mm2px(Vec(1.9,96)),module,Osc2::RST_PARAM));
    addInput(createInput<SmallPort>(mm2px(Vec(11.9,96)),module,Osc2::RST_INPUT));

    addOutput(createOutput<SmallPort>(mm2px(Vec(2,112)),module,Osc2::MAX_OUTPUT));
    addOutput(createOutput<SmallPort>(mm2px(Vec(11.9,112)),module,Osc2::CLIP_OUTPUT));

  }

  void appendContextMenu(Menu* menu) override {
    Osc2 *module=dynamic_cast<Osc2 *>(this->module);
    assert(module);

    menu->addChild(new MenuSeparator);

    menu->addChild(createBoolPtrMenuItem("Oversample","",&module->oversample));
  }

};


Model *modelOsc2=createModel<Osc2,Osc2Widget>("Osc2");