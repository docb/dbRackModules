#include "dcb.h"

using simd::float_4;

template<typename T>
struct SinOsc2 {
  T phs=0.f;

  void updatePhs(float sampleTime,T freq) {
    phs+=simd::fmin(freq*sampleTime,0.5f);
    phs-=simd::floor(phs);
  }

  T process(T ofs) {
    return simd::sin((phs+ofs)*2*M_PI);
  }

  void reset(T trigger) {
    phs = simd::ifelse(trigger, 0.f, phs);
  }
};

struct Osc2 : Module {
  enum ParamId {
    FREQ_PARAM,FM_PARAM,LIN_PARAM,PHS_PARAM,PHS_CV_PARAM,PARAMS_LEN
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

  SinOsc2<float_4> osc1[4];
  SinOsc2<float_4> osc2[4];
  DCBlocker<float_4> dcBlock[4];
  DCBlocker<float_4> dcBlock2[4];
  dsp::TSchmittTrigger<float_4> rstTriggers[4];

  Osc2() {
    config(PARAMS_LEN,INPUTS_LEN,OUTPUTS_LEN,LIGHTS_LEN);
    configParam(FREQ_PARAM,-14.f,4.f,0.f,"Frequency"," Hz",2,dsp::FREQ_C4);
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
  }

  void onReset(const ResetEvent &e) override {
    Module::onReset(e);
  }

  void process(const ProcessArgs &args) override {
    float freqParam=params[FREQ_PARAM].getValue();
    float fmParam=params[FM_PARAM].getValue();
    bool linear=params[LIN_PARAM].getValue()>0;
    int channels=std::max(inputs[VOCT_INPUT].getChannels(),1);
    float_4 phs=params[PHS_PARAM].getValue();
    float_4 phsCV=params[PHS_CV_PARAM].getValue();

    for(int c=0;c<channels;c+=4) {
      auto *oscil1=&osc1[c/4];
      auto *oscil2=&osc2[c/4];
      auto *dcb=&dcBlock[c/4];
      auto *dcb2=&dcBlock2[c/4];
      float_4 pitch1=freqParam+inputs[VOCT_INPUT].getVoltageSimd<float_4>(c);
      float_4 pitch2=pitch1;
      if(inputs[VOCT2_INPUT].isConnected()) pitch2=freqParam+inputs[VOCT2_INPUT].getPolyVoltageSimd<float_4>(c);

      float_4 freq;
      if(linear) {
        freq=dsp::FREQ_C4*dsp::approxExp2_taylor5(pitch1+30.f)/std::pow(2.f,30.f);
        freq+=dsp::FREQ_C4*inputs[FM_INPUT].getPolyVoltageSimd<float_4>(c)*fmParam;
      } else {
        pitch1+=inputs[FM_INPUT].getPolyVoltageSimd<float_4>(c)*fmParam;
        freq=dsp::FREQ_C4*dsp::approxExp2_taylor5(pitch1+30.f)/std::pow(2.f,30.f);
      }
      freq=simd::fmin(freq,args.sampleRate/2);
      oscil1->updatePhs(args.sampleTime,freq);

      if(linear) {
        freq=dsp::FREQ_C4*dsp::approxExp2_taylor5(pitch2+30.f)/std::pow(2.f,30.f);
        freq+=dsp::FREQ_C4*inputs[FM_INPUT].getPolyVoltageSimd<float_4>(c)*fmParam;
      } else {
        pitch2+=inputs[FM_INPUT].getPolyVoltageSimd<float_4>(c)*fmParam;
        freq=dsp::FREQ_C4*dsp::approxExp2_taylor5(pitch2+30.f)/std::pow(2.f,30.f);
      }
      freq=simd::fmin(freq,args.sampleRate/2);
      oscil2->updatePhs(args.sampleTime,freq);

      float_4 rst = inputs[RST_INPUT].getPolyVoltageSimd<float_4>(c);
      float_4 resetTriggered = rstTriggers[c/4].process(rst, 0.1f, 2.f);
      oscil1->reset(resetTriggered);
      oscil2->reset(resetTriggered);


      if(inputs[PHS_INPUT].isConnected()) {
        phs+=(simd::clamp(inputs[PHS_INPUT].getPolyVoltageSimd<float_4>(c),-5.f,5.f)*0.1f+0.5f)*phsCV;
      }

      float_4 o1=oscil1->process(0);
      float_4 o2=oscil2->process(phs);

      if(outputs[MAX_OUTPUT].isConnected()) {
        float_4 out=dcb->process(simd::fmax(o1,o2));
        outputs[MAX_OUTPUT].setVoltageSimd(out*5.f,c);
      }
      if(outputs[CLIP_OUTPUT].isConnected()) {
        float_4 bAbs=simd::fabs(o2);
        float_4 a=simd::ifelse(bAbs<o1,bAbs,simd::ifelse(o1<-bAbs,-bAbs,o1));
        float_4 out=dcb2->process(a);
        outputs[CLIP_OUTPUT].setVoltageSimd(out*5.f,c);
      }
    }
    outputs[MAX_OUTPUT].setChannels(channels);
    outputs[CLIP_OUTPUT].setChannels(channels);
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
    addInput(createInput<SmallPort>(mm2px(Vec(7,96)),module,Osc2::RST_INPUT));

    addOutput(createOutput<SmallPort>(mm2px(Vec(2,112)),module,Osc2::MAX_OUTPUT));
    addOutput(createOutput<SmallPort>(mm2px(Vec(11.9,112)),module,Osc2::CLIP_OUTPUT));


  }
};


Model *modelOsc2=createModel<Osc2,Osc2Widget>("Osc2");