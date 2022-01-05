#include "HexSeq.hpp"

extern Model *modelHexSeq;

struct HexSeqExp : Module {
  enum ParamId {
    PARAMS_LEN
  };
  enum InputId {
    INPUTS_LEN
  };
  enum OutputId {
    OUTPUTS_LEN=NUMSEQ*3
  };
  enum LightId {
    LIGHTS_LEN
  };

  HexSeqExp() {
    config(PARAMS_LEN,INPUTS_LEN,OUTPUTS_LEN,LIGHTS_LEN);
  }

  dsp::PulseGenerator gatePulseInvGenerators[NUMSEQ];
  dsp::SchmittTrigger invTrigger;
  bool last[NUMSEQ]={};
  float lastClock=0.f;
  void process(const ProcessArgs &args) override {
    HexSeq *mother=nullptr;
    if(leftExpander.module) {
      if(leftExpander.module->model==modelHexSeq) {
        mother=reinterpret_cast<HexSeq *>(leftExpander.module);
      }
    }
    if(mother) {
      for(int k=0;k<NUMSEQ;k++) {
        outputs[k].setVoltage(mother->state[k]?10.0f:0.0f);
      }
      for(int k=0;k<NUMSEQ;k++) {
        outputs[k+NUMSEQ].setVoltage(mother->state[k]&&lastClock>1.f?10.0f:0.0f);
        lastClock = mother->inputs[HexSeq::CLK_INPUT].getVoltage();
      }
      for(int k=0;k<NUMSEQ;k++) {
        bool trigger=mother->gatePulseInvGenerators[k].process(1.0/args.sampleRate);
        outputs[NUMSEQ*2+k].setVoltage((trigger?10.0f:0.0f));
      }

    }
  }
};


struct HexSeqExpWidget : ModuleWidget {
  HexSeqExpWidget(HexSeqExp *module) {
    setModule(module);
    setPanel(createPanel(asset::plugin(pluginInstance,"res/HexSeqExp.svg")));

    addChild(createWidget<ScrewSilver>(Vec(RACK_GRID_WIDTH,0)));
    addChild(createWidget<ScrewSilver>(Vec(box.size.x-2*RACK_GRID_WIDTH,0)));
    addChild(createWidget<ScrewSilver>(Vec(RACK_GRID_WIDTH,RACK_GRID_HEIGHT-RACK_GRID_WIDTH)));
    addChild(createWidget<ScrewSilver>(Vec(box.size.x-2*RACK_GRID_WIDTH,RACK_GRID_HEIGHT-RACK_GRID_WIDTH)));
    for(int k=0;k<NUMSEQ;k++) {
      addOutput(createOutput<SmallPort>(mm2px(Vec(2,MHEIGHT-(105.5f-(k*8.3f)))),module,k));
      addOutput(createOutput<SmallPort>(mm2px(Vec(12,MHEIGHT-(105.5f-(k*8.3f)))),module,NUMSEQ + k));
      addOutput(createOutput<SmallPort>(mm2px(Vec(22,MHEIGHT-(105.5f-(k*8.3f)))),module,2*NUMSEQ + k));
    }
  }
};


Model *modelHexSeqExp=createModel<HexSeqExp,HexSeqExpWidget>("HexSeqExp");