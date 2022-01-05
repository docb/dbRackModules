
#include "HexSeq.hpp"



struct HexSeqWidget : ModuleWidget {
  HexSeqWidget(HexSeq *module) {
    setModule(module);
    setPanel(APP->window->loadSvg(asset::plugin(pluginInstance,"res/HexSeq.svg")));

    addChild(createWidget<ScrewSilver>(Vec(RACK_GRID_WIDTH,0)));
    addChild(createWidget<ScrewSilver>(Vec(box.size.x-2*RACK_GRID_WIDTH,0)));
    addChild(createWidget<ScrewSilver>(Vec(RACK_GRID_WIDTH,RACK_GRID_HEIGHT-RACK_GRID_WIDTH)));
    addChild(createWidget<ScrewSilver>(Vec(box.size.x-2*RACK_GRID_WIDTH,RACK_GRID_HEIGHT-RACK_GRID_WIDTH)));

    addInput(createInput<SmallPort>(mm2px(Vec(5.f,MHEIGHT-115.5f)),module,HexSeq::CLK_INPUT));
    addInput(createInput<SmallPort>(mm2px(Vec(15.f,MHEIGHT-115.5f)),module,HexSeq::RST_INPUT));

    for(int k=0;k<NUMSEQ;k++) {
      auto *textField=createWidget<HexField<HexSeq>>(mm2px(Vec(3,MHEIGHT-(105.f-((float)k*8.3f)))));
      textField->setModule(module);
      textField->nr=k;
      textField->multiline=false;
      addChild(textField);
      addOutput(createOutput<SmallPort>(mm2px(Vec(50,MHEIGHT-(105.5f-(k*8.3f)))),module,HexSeq::GATE_OUTPUTS+k));
    }

    addChild(createWidget<Widget>(mm2px(Vec(-0.0,14.585))));
  }
};


Model *modelHexSeq=createModel<HexSeq,HexSeqWidget>("HexSeq");