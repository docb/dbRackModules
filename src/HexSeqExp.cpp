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
    OUTPUTS_LEN=NUMSEQ*3+3
  };
  enum LightId {
    LIGHTS_LEN
  };

  HexSeqExp() {
    config(PARAMS_LEN,INPUTS_LEN,OUTPUTS_LEN,LIGHTS_LEN);
    for(int k=0;k<NUMSEQ;k++) {
      configOutput(k,"Gate "+std::to_string(k+1));
      configOutput(k+NUMSEQ,"Clock "+std::to_string(k+1));
      configOutput(k+NUMSEQ*2,"Inverted "+std::to_string(k+1));
    }
    configOutput(NUMSEQ*3,"Polyphonic Gate");
    configOutput(NUMSEQ*3+1,"Polyphonic Clock");
    configOutput(NUMSEQ*3+2,"Polyphonic Inverted");
  }

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
        outputs[NUMSEQ*3].setVoltage(mother->state[k]?10.0f:0.0f,k);
      }
      for(int k=0;k<NUMSEQ;k++) {
        outputs[k+NUMSEQ].setVoltage(mother->state[k]&&lastClock>1.f?10.0f:0.0f);
        outputs[NUMSEQ*3+1].setVoltage(mother->state[k]&&lastClock>1.f?10.0f:0.0f,k);
        lastClock = mother->inputs[HexSeq::CLK_INPUT].getVoltage();
      }
      for(int k=0;k<NUMSEQ;k++) {
        bool trigger=mother->gatePulseInvGenerators[k].process(1.0/args.sampleRate);
        outputs[NUMSEQ*2+k].setVoltage((trigger?10.0f:0.0f));
        outputs[NUMSEQ*3+2].setVoltage(trigger?10.0f:0.0f,k);
      }
      for(int k=0;k<3;k++)
        outputs[NUMSEQ*3+k].setChannels(12);
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
    addOutput(createOutput<SmallPort>(mm2px(Vec(2,MHEIGHT-115.5f)),module,NUMSEQ*3));
    addOutput(createOutput<SmallPort>(mm2px(Vec(12,MHEIGHT-115.5f)),module,NUMSEQ*3+1));
    addOutput(createOutput<SmallPort>(mm2px(Vec(22,MHEIGHT-115.5f)),module,NUMSEQ*3+2));
  }
};


Model *modelHexSeqExp=createModel<HexSeqExp,HexSeqExpWidget>("HexSeqExp");