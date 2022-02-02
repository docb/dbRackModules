
#include "HexSeq.hpp"



struct HexSeqWidget : ModuleWidget {
  std::vector<HexField<HexSeq,HexSeqWidget>*> fields;
  std::vector<DBSmallLight<GreenLight>*> lights;
  bool showLights = true;
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
      for(int j=0;j<16;j++) {
        auto light = createLight<DBSmallLight<GreenLight>>(mm2px(Vec(3.7f+2.75f*j,MHEIGHT-(105.f-((float)k*8.3f)+2.f))), module, k*16+j);
        light->setVisible(module!=nullptr&&module->showLights);
        addChild(light);
        lights.push_back(light);
      }
    }
    addOutput(createOutput<SmallPort>(mm2px(Vec(50,MHEIGHT-115.5f)),module,HexSeq::GATE_OUTPUTS+NUMSEQ));

    addChild(createWidget<Widget>(mm2px(Vec(-0.0,14.585))));
  }
  void onHoverKey(const event::HoverKey &e) override {
    if (e.action == GLFW_PRESS) {
      int k=e.key-48;
      if(k>=1&&k<10) {
        fields[k-1]->onWidgetSelect=true;
        APP->event->setSelectedWidget(fields[k-1]);
      }
    }
    ModuleWidget::onHoverKey(e);
  }
  void moveFocusDown(int current) {
    APP->event->setSelectedWidget(fields[(current+1)%NUMSEQ]);
  }
  void moveFocusUp(int current) {
    APP->event->setSelectedWidget(fields[(NUMSEQ+current-1)%NUMSEQ]);
  }
  void toggleLights() {
    HexSeq* module = dynamic_cast<HexSeq*>(this->module);
    if(!module->showLights) {
      for(auto light: lights) {
        light->setVisible(true);
      }
      module->showLights = true;
    } else {
      for(auto light: lights) {
        light->setVisible(false);
      }
      module->showLights = false;
    }
  }
  void appendContextMenu(Menu* menu) override {
    HexSeq* module = dynamic_cast<HexSeq*>(this->module);
    assert(module);

    menu->addChild(new MenuSeparator);

    menu->addChild(createCheckMenuItem("ShowLights", "",
                                       [=]() {return module->showLights;},
                                       [=]() { toggleLights();}));
    menu->addChild(new DensMenuItem<HexSeq>(module));
    auto* randomLengthFromItem = new IntSelectItem(&module->randomLengthFrom,1,16);
    randomLengthFromItem->text = "Random length from";
    randomLengthFromItem->rightText = string::f("%d", module->randomLengthFrom) + "  " + RIGHT_ARROW;
    menu->addChild(randomLengthFromItem);
    auto* randomLengthToItem = new IntSelectItem(&module->randomLengthTo,1,16);
    randomLengthToItem->text = "Random length to";
    randomLengthToItem->rightText = string::f("%d", module->randomLengthTo) + "  " + RIGHT_ARROW;
    menu->addChild(randomLengthToItem);
  }
};


Model *modelHexSeq=createModel<HexSeq,HexSeqWidget>("HexSeq");