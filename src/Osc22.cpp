#include "dcb.h"
#include "filter.hpp"
using simd::float_4;

struct Osc22 : Module {
  enum ParamId {
    FREQ_A_PARAM, FINE_A_PARAM, FREQ_B_PARAM, FINE_B_PARAM, FM_A_PARAM,
    LIN_A_PARAM, FM_B_PARAM, LIN_B_PARAM,
    WIN_PARAM, PHS_PARAM, PHS_CV_PARAM, RST_PARAM, FM_A2_PARAM, FM_B2_PARAM, MIX_PARAM, PARAMS_LEN
  };

  enum InputId {
    VO_A_INPUT, VO_B_INPUT, FM_A_INPUT, FM_B_INPUT, RST_INPUT, FM_A2_INPUT, FM_B2_INPUT, MIX_INPUT, PHS_INPUT, INPUTS_LEN
  };

  enum OutputId {
    EQ_OUT, MAX_OUT, CLIP_OUT, SAW_A_OUT, TRI_A_OUT, SQ_A_OUT, SAW_B_OUT, TRI_B_OUT, SQ_B_OUT, LT_OUT, MIX_OUT, OUTPUTS_LEN
  };

  enum LightId {
    LIGHTS_LEN
  };

  enum SelectA { TRI_A, SAW_A, SQR_A };

  enum SelectB { TRI_B, SAW_B, SQR_B };

  bool connected[OUTPUTS_LEN]={};

  SinOsc<float_4> sinOscA[4];
  PhasorOsc<float_4> sawOscA[4];
  TriOsc<float_4> triOscA[4];
  SquareOsc<float_4> sqOscA[4];
  SinOsc<float_4> sinOscB[4];
  PhasorOsc<float_4> sawOscB[4];
  TriOsc<float_4> triOscB[4];
  SquareOsc<float_4> sqOscB[4];

  DCBlocker<float_4> dcbTriA[4];
  DCBlocker<float_4> dcbSawA[4];
  DCBlocker<float_4> dcbSqA[4];
  DCBlocker<float_4> dcbSawB[4];
  DCBlocker<float_4> dcbTriB[4];
  DCBlocker<float_4> dcbSqB[4];
  bool dcBlock = false;
  dsp::TSchmittTrigger<float_4> rstTriggers4[4];
  dsp::TSchmittTrigger<float_4> shTrigger[4];
  float_4 shValue = 0.f;
  dsp::SchmittTrigger manualRst;
  bool oversample = false;
  bool additive = false;
  Cheby1_32_BandFilter<float_4> maxFilter[4];
  Cheby1_32_BandFilter<float_4> clipFilter[4];
  Cheby1_32_BandFilter<float_4> cmpFilter[4];
  Cheby1_32_BandFilter<float_4> ltFilter[4];
  Cheby1_32_BandFilter<float_4> sqFilterA[4];
  Cheby1_32_BandFilter<float_4> sqFilterB[4];
  Cheby1_32_BandFilter<float_4> sawFilterA[4];
  Cheby1_32_BandFilter<float_4> sawFilterB[4];
  Cheby1_32_BandFilter<float_4> mixFilter[4];
  Cheby1_32_BandFilter<float_4> triFilterA[4];
  Cheby1_32_BandFilter<float_4> triFilterB[4];

  unsigned selectA = 0;
  unsigned selectB = 0;

  dsp::ClockDivider divider;

  Osc22() {
    config(PARAMS_LEN, INPUTS_LEN, OUTPUTS_LEN, LIGHTS_LEN);
    configParam(FREQ_A_PARAM, -8.f, 4.f, 0.f, "Freq A", " Hz", 2, dsp::FREQ_C4);
    configInput(VO_A_INPUT, "V/Oct A");
    configInput(FM_A_INPUT, "FM A");
    configInput(FM_A2_INPUT, "FM A 2");
    configButton(LIN_A_PARAM, "Linear A");
    configParam(FM_A_PARAM, 0, 3, 0, "FM A Amount", "%", 0, 100);
    configParam(FINE_A_PARAM, -100, 100, 0, "Fine tune A", " cents");
    configParam(FM_A2_PARAM, 0, 3, 0, "FM A 2 Amount", "%", 0, 100);

    configOutput(TRI_A_OUT, "Triangle A");
    configOutput(SAW_A_OUT, "Saw A");
    configOutput(SQ_A_OUT, "Square A");

    configParam(FREQ_B_PARAM, -8.f, 4.f, 0.f, "Freq B", " Hz", 2, dsp::FREQ_C4);
    configInput(VO_B_INPUT, "V/Oct B");
    configInput(FM_B_INPUT, "FM B");
    configInput(FM_B2_INPUT, "FM B 2");
    configButton(LIN_B_PARAM, "Linear B");
    configParam(FM_B_PARAM, 0, 3, 0, "FM B Amount", "%", 0, 100);
    configParam(FINE_B_PARAM, -100, 100, 0, "Fine tune B", " cents");
    configParam(FM_B2_PARAM, 0, 3, 0, "FM B 2 Amount", "%", 0, 100);

    configParam(WIN_PARAM, 0, 1, 0.01, "Window");
    configParam(PHS_PARAM, 0, 1, 0., "Phs");
    configParam(PHS_CV_PARAM, 0, 1, 0., "Phs CV");
    configParam(MIX_PARAM, 0, 1, 0.0, "Mix");

    configButton(RST_PARAM, "Rst");
    configInput(RST_INPUT, "RST");
    configInput(MIX_INPUT, "Mix CV");
    configInput(PHS_INPUT, "Phs");

    configOutput(TRI_B_OUT, "Tri B");
    configOutput(SAW_B_OUT, "Saw B");
    configOutput(SQ_B_OUT, "Square B");

    configOutput(LT_OUT, "EQ");
    configOutput(EQ_OUT, "EQ");
    configOutput(MAX_OUT, "Max");
    configOutput(MIX_OUT, "Mix");
    configOutput(CLIP_OUT, "Clip");
    divider.setDivision(64);
  }

  void checkConnected() {
    if(divider.process()) {
      for(int k=0;k<OUTPUTS_LEN;k++) {
        connected[k]=outputs[k].isConnected();
      }
    }
  }

  float_4 getFreqA(int c, float sampleRate) {
    float freqParam = params[FREQ_A_PARAM].getValue();
    float fmParam = params[FM_A_PARAM].getValue();
    float fmParam2 = params[FM_A2_PARAM].getValue();
    float fineTune = params[FINE_A_PARAM].getValue();
    bool linear = params[LIN_A_PARAM].getValue() > 0;
    float_4 pitch = freqParam + inputs[VO_A_INPUT].getPolyVoltageSimd<float_4>(c) + fineTune / 1200.f;
    float_4 freq;
    if(linear) {
      pitch += inputs[FM_A2_INPUT].getPolyVoltageSimd<float_4>(c) * fmParam2;
      freq = dsp::FREQ_C4 * dsp::approxExp2_taylor5(pitch + 30.f) / std::pow(2.f, 30.f);
      freq += dsp::FREQ_C4 * inputs[FM_A_INPUT].getPolyVoltageSimd<float_4>(c) * fmParam;
    } else {
      pitch += inputs[FM_A_INPUT].getPolyVoltageSimd<float_4>(c) * fmParam;
      pitch += inputs[FM_A2_INPUT].getPolyVoltageSimd<float_4>(c) * fmParam2;
      freq = dsp::FREQ_C4 * dsp::approxExp2_taylor5(pitch + 30.f) / std::pow(2.f, 30.f);
    }
    return simd::fmin(freq, sampleRate / 2);
  }

  float_4 getFreqB(int c, float sampleRate) {
    float freqParam = params[FREQ_B_PARAM].getValue();
    float fmParam = params[FM_B_PARAM].getValue();
    float fmParam2 = params[FM_B2_PARAM].getValue();
    float fineTune = params[FINE_B_PARAM].getValue();
    bool linear = params[LIN_B_PARAM].getValue() > 0;
    float_4 pitch = freqParam + inputs[VO_B_INPUT].getPolyVoltageSimd<float_4>(c) + fineTune / 1200.f;
    float_4 freq;
    if(linear) {
      pitch += inputs[FM_B2_INPUT].getPolyVoltageSimd<float_4>(c) * fmParam2;
      freq = dsp::FREQ_C4 * dsp::approxExp2_taylor5(pitch + 30.f) / std::pow(2.f, 30.f);
      freq += dsp::FREQ_C4 * inputs[FM_B_INPUT].getPolyVoltageSimd<float_4>(c) * fmParam;
    } else {
      pitch += inputs[FM_B_INPUT].getPolyVoltageSimd<float_4>(c) * fmParam;
      pitch += inputs[FM_B2_INPUT].getPolyVoltageSimd<float_4>(c) * fmParam2;
      freq = dsp::FREQ_C4 * dsp::approxExp2_taylor5(pitch + 30.f) / std::pow(2.f, 30.f);
    }
    return simd::fmin(freq, sampleRate / 2);
  }



  void process(const ProcessArgs &args) override {
    checkConnected();
    float ofs = params[PHS_PARAM].getValue();
    float eps = oversample ? params[WIN_PARAM].getValue() * 0.01 / 16 : params[WIN_PARAM].getValue() * 0.01;
    int count = oversample ? 16 : 1;
    int channels = std::max(std::min(inputs[VO_A_INPUT].getChannels(), inputs[VO_B_INPUT].getChannels()), 1);
    for(int c = 0;c < channels;c += 4) {
      float_4 rst=inputs[RST_INPUT].getPolyVoltageSimd<float_4>(c);
      float_4 resetTriggered=
        rstTriggers4[c/4].process(rst,0.1f,2.f)|manualRst.process(params[RST_PARAM].getValue());
      triOscA->reset(resetTriggered);
      triOscB->reset(resetTriggered);
      sqOscA->reset(resetTriggered);
      sqOscB->reset(resetTriggered);
      sawOscA->reset(resetTriggered);
      sawOscB->reset(resetTriggered);

      float_4 sqAOut, sqBOut, cmpOut, maxOut, clipOut, sawAOut, sawBOut, lt, triA, triB, triAOut, triBOut, mixOut;;
      float_4 freqA = getFreqA(c, args.sampleRate) / static_cast<float>(count);
      float_4 freqB = getFreqB(c, args.sampleRate) / static_cast<float>(count);
      float_4 phsIn=simd::clamp(ofs+0.1f*params[PHS_CV_PARAM].getValue()*inputs[PHS_INPUT].getPolyVoltageSimd<float_4>(c));
      float_4 mixIn = simd::clamp(inputs[MIX_INPUT].getPolyVoltageSimd<float_4>(c),-5,5)* 0.1f + 0.5f;
      float_4 mixCV = inputs[MIX_INPUT].isConnected()?mixIn * params[MIX_PARAM].getValue():params[MIX_PARAM].getValue();
      for(int k = 0;k < count;k++) {
        triOscA[c / 4].updatePhs(args.sampleTime, freqA);
        triA = triOscA[c / 4].process(phsIn);
        triOscB[c / 4].updatePhs(args.sampleTime, freqB);
        triB = triOscB[c / 4].process(0);
        sqOscA[c / 4].updatePhs(args.sampleTime, freqA);
        float_4 sqA = sqOscA[c / 4].process(phsIn);
        sqOscB[c / 4].updatePhs(args.sampleTime, freqB);
        float_4 sqB = sqOscB[c / 4].process(0);
        sawOscA[c / 4].updatePhs(args.sampleTime, freqA);
        float_4 sawA = sawOscA[c / 4].process(phsIn);
        sawOscB[c / 4].updatePhs(args.sampleTime, freqB);
        float_4 sawB = sawOscB[c / 4].process(0);
        float_4 m1, m2;
        m1 = selectA ? selectA > 1 ? sqA : sawA : triA;
        m2 = selectB ? selectB > 1 ? sqB : sawB : triB;
        float_4 max = simd::fmax(m1, m2);
        float_4 bAbs = simd::fabs(m2);
        float_4 clip = simd::ifelse(bAbs < m1, bAbs, simd::ifelse(m1 < -bAbs, -bAbs, m1));

        float_4 cmp = simd::ifelse(simd::abs(m1 - m2) < eps, 0.f, 10.f);
        shValue = simd::ifelse(shTrigger[c / 4].process(cmp), m1, shValue);
        lt = simd::ifelse(m1 < m2, -5, 5);

        float_4 mix= m1*mixCV + m2*(1-mixCV);
        if(count > 1) {
          if(connected[SQ_A_OUT]) sqAOut = sqFilterA[c / 4].process(sqA * 10.f - 5);
          if(connected[SQ_B_OUT]) sqBOut = sqFilterB[c / 4].process(sqB * 10.f - 5);
          if(connected[SAW_A_OUT]) sawAOut = sawFilterA[c / 4].process(sawA * 10.f - 5);
          if(connected[SAW_B_OUT]) sawBOut = sawFilterB[c / 4].process(sawB * 10.f - 5);
          if(connected[EQ_OUT]) cmpOut = cmpFilter[c / 4].process(shValue * 5);
          if(connected[MAX_OUT]) maxOut = maxFilter[c / 4].process(max * 5);
          if(connected[CLIP_OUT]) clipOut = clipFilter[c / 4].process(clip * 5);
          if(connected[MIX_OUT]) mixOut = mixFilter[c/4].process(mix*5);
          if(connected[TRI_A_OUT]) triAOut = triFilterA[c / 4].process(triA * 8);
          if(connected[TRI_B_OUT]) triBOut = triFilterB[c / 4].process(triB * 8);
          if(connected[LT_OUT]) lt = clipFilter[c / 4].process(lt);
        } else {
          sqAOut = sqA * 10.f - 5;
          sqBOut = sqB * 10.f - 5;
          cmpOut = shValue * 5;
          sawAOut = sawA * 10.f - 5;
          sawBOut = sawB * 10.f - 5;
          maxOut = max * 5;
          clipOut = clip * 5;
          mixOut=mix*5;
          triAOut = triA * 8;
          triBOut = triB * 8;
        }
      }

      outputs[MIX_OUT].setVoltageSimd(dcBlock ? dcbTriA[c / 4].process(mixOut ) : mixOut, c);
      outputs[TRI_A_OUT].setVoltageSimd(dcBlock ? dcbTriA[c / 4].process(triAOut ) : triA, c);
      outputs[TRI_B_OUT].setVoltageSimd(dcBlock ? dcbTriB[c / 4].process(triBOut ) : triB, c);
      outputs[SQ_A_OUT].setVoltageSimd(sqAOut, c);
      outputs[SQ_B_OUT].setVoltageSimd(sqBOut, c);
      outputs[SAW_A_OUT].setVoltageSimd(dcBlock ? dcbSawA[c / 4].process(sawAOut) : sawAOut, c);
      outputs[SAW_B_OUT].setVoltageSimd(dcBlock ? dcbSawB[c / 4].process(sawBOut) : sawBOut, c);
      outputs[EQ_OUT].setVoltageSimd(cmpOut, c);
      outputs[LT_OUT].setVoltageSimd(lt, c);
      outputs[MAX_OUT].setVoltageSimd(maxOut, c);
      outputs[CLIP_OUT].setVoltageSimd(clipOut, c);
    }
    for(int k = 0;k < OUTPUTS_LEN;k++) outputs[k].setChannels(channels);
  }


  json_t *dataToJson() override {
    json_t *data=json_object();
    json_object_set_new(data,"oversample",json_boolean(oversample));
    json_object_set_new(data,"dcblock",json_boolean(dcBlock));
    json_object_set_new(data,"selectA",json_integer(selectA));
    json_object_set_new(data,"selectB",json_integer(selectB));
    return data;
  }

  void dataFromJson(json_t *rootJ) override {
    json_t *jOverSample=json_object_get(rootJ,"oversample");
    if(jOverSample!=nullptr) oversample=json_boolean_value(jOverSample);
    json_t *jBlockDC=json_object_get(rootJ,"dcblock");
    if(jBlockDC!=nullptr) oversample=json_boolean_value(jBlockDC);
    json_t *jSelectA=json_object_get(rootJ,"selectA");
    if(jSelectA!=nullptr) selectA=json_integer_value(jSelectA);
    json_t *jSelectB=json_object_get(rootJ,"selectB");
    if(jSelectB!=nullptr) selectB=json_integer_value(jSelectB);
  }

};


struct Osc22Widget : ModuleWidget {
  explicit Osc22Widget(Osc22 *module) {
    setModule(module);
    setPanel(createPanel(asset::plugin(pluginInstance, "res/Osc22.svg")));

    addParam(createParam<TrimbotWhite9>(mm2px(Vec(4, 11)), module, Osc22::FREQ_A_PARAM));
    addParam(createParam<TrimbotWhite>(mm2px(Vec(16, 12.5)), module, Osc22::FINE_A_PARAM));

    addParam(createParam<TrimbotWhite9>(mm2px(Vec(47.5, 11)), module, Osc22::FREQ_B_PARAM));
    addParam(createParam<TrimbotWhite>(mm2px(Vec(38, 12)), module, Osc22::FINE_B_PARAM));

    addInput(createInput<SmallPort>(mm2px(Vec(11, 21)), module, Osc22::VO_A_INPUT));
    addInput(createInput<SmallPort>(mm2px(Vec(42.5, 21)), module, Osc22::VO_B_INPUT));

    addInput(createInput<SmallPort>(mm2px(Vec(11, 46)), module, Osc22::FM_A_INPUT));
    addParam(createParam<TrimbotWhite9>(mm2px(Vec(4, 35.5)), module, Osc22::FM_A_PARAM));
    addParam(createParam<MLED>(mm2px(Vec(16, 37.5)), module, Osc22::LIN_A_PARAM));

    addInput(createInput<SmallPort>(mm2px(Vec(42.5, 46)), module, Osc22::FM_B_INPUT));
    addParam(createParam<TrimbotWhite9>(mm2px(Vec(47.5, 35.5)), module, Osc22::FM_B_PARAM));
    addParam(createParam<MLED>(mm2px(Vec(38, 37.5)), module, Osc22::LIN_B_PARAM));

    addInput(createInput<SmallPort>(mm2px(Vec(16, 62.5)), module, Osc22::FM_A2_INPUT));
    addParam(createParam<TrimbotWhite9>(mm2px(Vec(4, 61)), module, Osc22::FM_A2_PARAM));

    addInput(createInput<SmallPort>(mm2px(Vec(38, 62.5)), module, Osc22::FM_B2_INPUT));
    addParam(createParam<TrimbotWhite9>(mm2px(Vec(47.5, 61)), module, Osc22::FM_B2_PARAM));

    addParam(createParam<MLEDM>(mm2px(Vec(5,78)),module,Osc22::RST_PARAM));
    addInput(createInput<SmallPort>(mm2px(Vec(16,78)),module,Osc22::RST_INPUT));

    addParam(createParam<TrimbotWhite9>(mm2px(Vec(47.5, 81)), module, Osc22::PHS_PARAM));
    addParam(createParam<TrimbotWhite>(mm2px(Vec(5, 90)), module, Osc22::WIN_PARAM));
    addParam(createParam<TrimbotWhite>(mm2px(Vec(27, 88)), module, Osc22::MIX_PARAM));
    addParam(createParam<TrimbotWhite>(mm2px(Vec(40, 87)), module, Osc22::PHS_CV_PARAM));
    addInput(createInput<SmallPort>(mm2px(Vec(27, 96)), module, Osc22::MIX_INPUT));
    addInput(createInput<SmallPort>(mm2px(Vec(40, 79)), module, Osc22::PHS_INPUT));

    addOutput(createOutput<SmallPort>(mm2px(Vec(8.5, 104)), module, Osc22::EQ_OUT));
    addOutput(createOutput<SmallPort>(mm2px(Vec(18, 104)), module, Osc22::LT_OUT));
    addOutput(createOutput<SmallPort>(mm2px(Vec(27, 104)), module, Osc22::MIX_OUT));
    addOutput(createOutput<SmallPort>(mm2px(Vec(36.5, 104)), module, Osc22::MAX_OUT));
    addOutput(createOutput<SmallPort>(mm2px(Vec(45.5, 104)), module, Osc22::CLIP_OUT));
    addOutput(createOutput<SmallPort>(mm2px(Vec(4, 116)), module, Osc22::TRI_A_OUT));
    addOutput(createOutput<SmallPort>(mm2px(Vec(13, 116)), module, Osc22::SAW_A_OUT));
    addOutput(createOutput<SmallPort>(mm2px(Vec(22, 116)), module, Osc22::SQ_A_OUT));
    addOutput(createOutput<SmallPort>(mm2px(Vec(32, 116)), module, Osc22::SQ_B_OUT));
    addOutput(createOutput<SmallPort>(mm2px(Vec(41, 116)), module, Osc22::SAW_B_OUT));
    addOutput(createOutput<SmallPort>(mm2px(Vec(50, 116)), module, Osc22::TRI_B_OUT));
  }

  void appendContextMenu(Menu *menu) override {
    const auto module = dynamic_cast<Osc22 *>(this->module);
    assert(module);
    menu->addChild(new MenuSeparator);
    menu->addChild(createBoolPtrMenuItem("Oversample", "", &module->oversample));
    menu->addChild(createBoolPtrMenuItem("Block DC", "", &module->dcBlock));
    auto selA = new LabelIntSelectItem(&module->selectA, {"Tri", "Saw", "Sqr"});
    selA->text = "Wave A";
    menu->addChild(selA);
    auto selB = new LabelIntSelectItem(&module->selectB, {"Tri", "Saw", "Sqr"});
    selB->text = "Wave B";
    menu->addChild(selB);
  }
};


Model *modelOsc22 = createModel<Osc22, Osc22Widget>("Osc22");
