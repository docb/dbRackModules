
#include "HexSeq.hpp"



struct HexSeqWidget : ModuleWidget {
  std::vector<HexField<HexSeq,HexSeqWidget>*> fields;
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
      auto *textField=createWidget<HexField<HexSeq,HexSeqWidget>>(mm2px(Vec(3,MHEIGHT-(105.f-((float)k*8.3f)))));
      textField->setModule(module);
      textField->setModuleWidget(this);
      textField->nr=k;
      textField->multiline=false;
      addChild(textField);
      fields.push_back(textField);
      addOutput(createOutput<SmallPort>(mm2px(Vec(50,MHEIGHT-(105.5f-(k*8.3f)))),module,HexSeq::GATE_OUTPUTS+k));
    }

    addChild(createWidget<Widget>(mm2px(Vec(-0.0,14.585))));
  }
  void onHoverKey(const event::HoverKey &e) override {
    int k = e.key - 48;
    if(k>=0 && k<10) {
      fields[k]->onWidgetSelect = true;
      APP->event->setSelectedWidget(fields[k]);
    }
    ModuleWidget::onHoverKey(e);
  }
  void moveFocusDown(int current) {
    APP->event->setSelectedWidget(fields[(current+1)%NUMSEQ]);
  }
  void moveFocusUp(int current) {
    APP->event->setSelectedWidget(fields[(NUMSEQ+current-1)%NUMSEQ]);
  }
};


Model *modelHexSeq=createModel<HexSeq,HexSeqWidget>("HexSeq");