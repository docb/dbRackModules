#include "dcb.h"
//#include "hexfield.h"
//#include "hexutil.h"
#include "HexSeq.hpp"
#undef NUMSEQ
#define NUMSEQ 16
#define NUMPAT 16
extern Model *modelHexSeq;

struct HexSeqP : Module {
  enum ParamId {
    PATTERN_PARAM,COPY_PARAM,PASTE_PARAM,PARAMS_LEN
  };
  enum InputId {
    CLK_INPUT,RST_INPUT,PAT_INPUT,INPUTS_LEN
  };
  enum OutputId {
    TRG_OUTPUT,GATE_OUTPUT,CLK_OUTPUT,INV_OUTPUT,OUTPUTS_LEN
  };
  enum LightId {
    LIGHTS_LEN = NUMSEQ*16
  };
  int currentPattern=0;
  unsigned long songpos = 0;
  std::string hexs[NUMPAT][NUMSEQ]={};
  std::string clipBoard[NUMSEQ];
  bool dirty[NUMSEQ]={};
  bool state[NUMSEQ]={};
  dsp::PulseGenerator gatePulseGenerators[NUMSEQ];
  dsp::PulseGenerator gatePulseInvGenerators[NUMSEQ];
  dsp::SchmittTrigger clockTrigger;
  dsp::SchmittTrigger rstTrigger;
  dsp::SchmittTrigger patTrigger[NUMSEQ];
  int clockCounter=-1;
  unsigned int delay = 0;
  bool polySelect = true;
  bool showLights = true;
  float lastClock=0.f;
  float randomDens = 0.3;
  int randomLengthFrom = 8;
  int randomLengthTo = 8;
  RND rnd;
  HexSeq *hexSeq = nullptr;
  Module *CMGate16 = nullptr;
  Module *EuclidSeq = nullptr;
  void setDirty(int nr,bool _dirty) {
    dirty[nr]=_dirty;
  }

  bool isDirty(int nr) {
    return dirty[nr];
  }


  void setHex(int nr,const std::string &hexStr) {
    hexs[currentPattern][nr]=hexStr;
  }

  std::string getHex(int nr) {
    return hexs[currentPattern][nr];
  }

  HexSeqP() {
    config(PARAMS_LEN,INPUTS_LEN,OUTPUTS_LEN,LIGHTS_LEN);
    configSwitch(PATTERN_PARAM,0.f,15.f,0.f,"Pattern",{"1","2","3","4","5","6","7","8","9","10","11","12","13","14","15","16"});
    getParamQuantity(PATTERN_PARAM)->snapEnabled=true;
    configParam(COPY_PARAM,0.f,1.f,0.f,"Copy");
    configParam(PASTE_PARAM,0.f,1.f,0.f,"Paste");
    configInput(CLK_INPUT,"Clock");
    configInput(RST_INPUT,"Reset");
    configInput(PAT_INPUT,"Pattern select");
    configOutput(TRG_OUTPUT,"Trigger");
    configOutput(GATE_OUTPUT,"Gate");
    configOutput(CLK_OUTPUT,"Clock");
    configOutput(INV_OUTPUT,"Inverted");
  }

  unsigned int hexToInt(const std::string &hex) {
    if(hex == "*") return rnd.nextRange(0,15);
    unsigned int x;
    std::stringstream ss;
    ss<<std::hex<<hex;
    ss>>x;
    return x;
  }

  void next() {
    for(int k=0;k<NUMSEQ;k++) {
      for(int j=0;j<16;j++) {
        lights[k*16+j].setBrightness(0.f);
      }
      unsigned int len=hexs[currentPattern][k].length();
      if(len>0) {
        unsigned spos=songpos%(len*4);
        unsigned charPos=spos/4;
        lights[k*16+charPos].setBrightness(1.f);
        std::string hs=hexs[currentPattern][k].substr(charPos,1);
        unsigned int hex=hexToInt(hs);
        unsigned posInChar=spos%4;
        bool on=hex>>(3-posInChar)&0x01;
        if(on) {
          gatePulseGenerators[k].trigger(0.01f);
          state[k] = true; // used by expander
        } else {
          state[k] = false;
          gatePulseInvGenerators[k].trigger(0.01f); // process by expander
        }
      }
    }
    songpos++;
  }

  void copyFromHexSeq() {
    INFO("copy from hexSeq");
    if(hexSeq) {
      for(int k=0;k<hexSeq->numSeq;k++) {
        hexs[currentPattern][k] = hexSeq->hexs[k];
        dirty[k]=true;
      }
    }
  }

  void copyFromEuclid(int field) {
    EuclideanAlgorithm euclid;
    if(EuclidSeq) {
      int length = EuclidSeq->params[0].getValue();
      if(length%4!=0 || length>64) return;
      float _hits = (float)length * (EuclidSeq->params[1].getValue());
      int hits = clamp((int)_hits, 0, length);
      int shift = EuclidSeq->params[2].getValue();
      shift = clamp(shift, 0, length);
      INFO("%d %d %d",length,hits,shift);
      euclid.set(hits,length,-shift);
      hexs[currentPattern][field] = euclid.getPattern();
      dirty[field]=true;
    }
  }

  void copyFromCMGateSeq16() {
    if(CMGate16) {
      for(int r=0;r<8;r++) {
        long value = 0;
        for(int c=0;c<16;c++) {
          value |=  ((CMGate16->params[(r * 16) + c].getValue() > 0.5f) << (15-c));
        }
        std::stringstream stream;
        stream<<std::uppercase<<std::setfill('0')<<std::setw(4)<<std::hex<<(value);
        INFO("%s",stream.str().c_str());
        hexs[currentPattern][r] =stream.str().c_str();
        dirty[r]=true;
        //std::string str=stream.str().substr(0,text.size());
      }

    }
  }

  void copy() {
    for(int k=0;k<NUMSEQ;k++) {
      clipBoard[k] = hexs[currentPattern][k];
    }
  }

  void paste() {
    for(int k=0;k<NUMSEQ;k++) {
      hexs[currentPattern][k] = clipBoard[k];
      dirty[k]=true;
    }
  }

  void process(const ProcessArgs &args) override {
    if(leftExpander.module) {
      if(leftExpander.module->model==modelHexSeq) {
        hexSeq=reinterpret_cast<HexSeq *>(leftExpander.module);
      } else {
        hexSeq = nullptr;
      }
      if(leftExpander.module->model->slug == "GateSequencer16") {
          CMGate16 = leftExpander.module;
      } else {
          CMGate16 = nullptr;
      }
      if(leftExpander.module->model->slug == "Euclid") {
        EuclidSeq = leftExpander.module;
      } else {
        EuclidSeq = nullptr;
      }
    } else {
      hexSeq =nullptr;
      CMGate16 = nullptr;
      EuclidSeq = nullptr;
    }
    if(rstTrigger.process(inputs[RST_INPUT].getVoltage())) {
      for(int k=0;k<NUMSEQ;k++) {
        songpos=0;
      }
    }
    if(inputs[PAT_INPUT].isConnected()) {
      if(polySelect) {
        for(int k=0;k<NUMPAT;k++) {
          if(patTrigger[k].process(inputs[PAT_INPUT].getVoltage(k))) {
            currentPattern=k;
            getParamQuantity(PATTERN_PARAM)->setValue(k);
            for(int j=0;j<NUMSEQ;j++) {
              setDirty(j,true);
            }
            songpos = 0;
          }
        }
      } else {
        int k = clamp(inputs[PAT_INPUT].getVoltage()*1.6f-0.1,0.f,15.9f);
        if(k!=currentPattern) {
          currentPattern=k;
          songpos = 0;
          getParamQuantity(PATTERN_PARAM)->setValue(k);
          for(int j=0;j<NUMSEQ;j++) {
            setDirty(j,true);
          }
        }
      }
    }
    if(inputs[CLK_INPUT].isConnected()) {
      if(clockTrigger.process(inputs[CLK_INPUT].getVoltage())) {
        clockCounter=delay;
      }
      if(clockCounter>0) {
        clockCounter--;
      }
      if(clockCounter==0) {
        clockCounter--;
        next();
      }
    }
    for(int k=0;k<NUMSEQ;k++) {
      bool trigger=gatePulseGenerators[k].process(1.0/args.sampleRate);
      outputs[TRG_OUTPUT].setVoltage((trigger?10.0f:0.0f),k);
      outputs[GATE_OUTPUT].setVoltage(state[k]?10.0f:0.0f,k);
      outputs[CLK_OUTPUT].setVoltage(state[k]&&lastClock>1.f?10.0f:0.0f,k);
      lastClock = inputs[CLK_INPUT].getVoltage();
      trigger=gatePulseInvGenerators[k].process(1.0/args.sampleRate);
      outputs[INV_OUTPUT].setVoltage((trigger?10.0f:0.0f),k);
    }
    outputs[TRG_OUTPUT].setChannels(16);
    outputs[GATE_OUTPUT].setChannels(16);
    outputs[CLK_OUTPUT].setChannels(16);
    outputs[INV_OUTPUT].setChannels(16);

  }

  json_t *dataToJson() override {
    json_t *data=json_object();
    json_t *patList=json_array();
    for(int j=0;j<NUMPAT;j++) {
      json_t *hexList=json_array();
      for(int k=0;k<NUMSEQ;k++) {
        json_array_append_new(hexList,json_string(hexs[j][k].c_str()));
      }
      json_array_append_new(patList,hexList);
    }
    json_object_set_new(data,"hexStrings",patList);
    json_object_set_new(data,"polySelect",json_boolean(polySelect));
    json_object_set_new(data,"showLights",json_boolean(showLights));
    json_object_set_new(data,"delay",json_integer(delay));
    json_object_set_new(data,"randomDens",json_real(randomDens));
    json_object_set_new(data,"randomLengthFrom",json_integer(randomLengthFrom));
    json_object_set_new(data,"randomLengthTo",json_integer(randomLengthTo));
    return data;
  }

  void dataFromJson(json_t *rootJ) override {
    json_t *patList=json_object_get(rootJ,"hexStrings");
    for(int k=0;k<NUMPAT;k++) {
      json_t *hexList = json_array_get(patList,k);
      for(int j=0;j<NUMSEQ;j++) {
        json_t *hexStr=json_array_get(hexList,j);
        hexs[k][j]=json_string_value(hexStr);
        dirty[j]=true;
      }
    }
    json_t *jPolySelect = json_object_get(rootJ,"polySelect");
    if(jPolySelect!=nullptr) polySelect = json_boolean_value(jPolySelect);
    json_t *jShowLights = json_object_get(rootJ,"showLights");
    if(jShowLights!=nullptr) showLights = json_boolean_value(jShowLights);
    json_t *jDelay =json_object_get(rootJ,"delay");
    if(jDelay!=nullptr) delay = json_integer_value(jDelay);
    json_t *jDens =json_object_get(rootJ,"randomDens");
    if(jDens!=nullptr) randomDens = json_real_value(jDens);
    json_t *jRandomLengthFrom =json_object_get(rootJ,"randomLengthFrom");
    if(jRandomLengthFrom!=nullptr) randomLengthFrom= json_integer_value(jRandomLengthFrom);
    json_t *jRandomLengthTo =json_object_get(rootJ,"randomLengthTo");
    if(jRandomLengthTo!=nullptr) randomLengthTo = json_integer_value(jRandomLengthTo);
  }
  void randomizeField(int j,int k) {
    hexs[j][k]=getRandomHex(rnd,randomDens,randomLengthFrom,randomLengthTo);
    dirty[k]=true;
  }
  void randomizeCurrentPattern() {
    for(int k=0;k<NUMSEQ;k++) {
      randomizeField(currentPattern,k);
    }
  }
  void initializeCurrentPattern() {
    for(int k=0;k<NUMSEQ;k++) {
      hexs[currentPattern][k]="";
      dirty[k]=true;
    }
  }
  void onRandomize(const RandomizeEvent &e) override {
    for(int j=0;j<NUMPAT;j++) {
      for(int k=0;k<NUMSEQ;k++) {
        randomizeField(j,k);
      }
    }
  }

  void onReset(const ResetEvent &e) override {
    for(int j=0;j<NUMPAT;j++) {
      for(int k=0;k<NUMSEQ;k++) {
        hexs[j][k]="";
        dirty[k]=true;
      }
    }
  }
};
struct HexSeqPWidget;

struct HexFieldP : HexField<HexSeqP,HexSeqPWidget> {
  HexFieldP() : HexField<HexSeqP,HexSeqPWidget>() {
    font_size=14;
    textOffset=Vec(2.f,1.f);
    box.size=mm2px(Vec(45.5,5.f));
  }
};

struct PatternKnob : TrimbotWhite {
  HexSeqP *module;

  PatternKnob() : TrimbotWhite() {

  }

  void onChange(const event::Change &ev) override {
    if(module==nullptr)
      return;
    int pattern=module->params[HexSeqP::PATTERN_PARAM].getValue();
    if(pattern!=module->currentPattern) {
      module->currentPattern=pattern;
      for(int k=0;k<NUMSEQ;k++) {
        module->setDirty(k,true);
        module->songpos = 0;
      }
    }
    SvgKnob::onChange(ev);
  }
};

template<typename T>
struct CopyButton : SvgSwitch {
  T *module;
  CopyButton() {
    momentary = true;
    addFrame(APP->window->loadSvg(asset::plugin(pluginInstance, "res/SmallButton0.svg")));
    addFrame(APP->window->loadSvg(asset::plugin(pluginInstance, "res/SmallButton1.svg")));
  }
  void onChange(const ChangeEvent& e) override {
    if(module) module->copy();
    SvgSwitch::onChange(e);
  }
};
template<typename T>
struct PasteButton : SvgSwitch {
  T *module;
  PasteButton() {
    momentary = true;
    addFrame(APP->window->loadSvg(asset::plugin(pluginInstance, "res/SmallButton0.svg")));
    addFrame(APP->window->loadSvg(asset::plugin(pluginInstance, "res/SmallButton1.svg")));
  }
  void onChange(const ChangeEvent& e) override {
    if(module) module->paste();
    SvgSwitch::onChange(e);
  }
};

struct HexSeqPWidget : ModuleWidget {
  std::vector<HexFieldP*> fields;
  std::vector<DBSmallLight<GreenLight>*> lights;


  HexSeqPWidget(HexSeqP *module) {
    setModule(module);
    setPanel(createPanel(asset::plugin(pluginInstance,"res/HexSeqP.svg")));

    addChild(createWidget<ScrewSilver>(Vec(RACK_GRID_WIDTH,0)));
    addChild(createWidget<ScrewSilver>(Vec(box.size.x-2*RACK_GRID_WIDTH,0)));
    addChild(createWidget<ScrewSilver>(Vec(RACK_GRID_WIDTH,RACK_GRID_HEIGHT-RACK_GRID_WIDTH)));
    addChild(createWidget<ScrewSilver>(Vec(box.size.x-2*RACK_GRID_WIDTH,RACK_GRID_HEIGHT-RACK_GRID_WIDTH)));

    addInput(createInput<SmallPort>(mm2px(Vec(51.5f,MHEIGHT-108.8f-6.287)),module,HexSeqP::CLK_INPUT));
    addInput(createInput<SmallPort>(mm2px(Vec(51.5f,MHEIGHT-99.f-6.287)),module,HexSeqP::RST_INPUT));

    auto *patKnob=createParam<PatternKnob>(mm2px(Vec(51.5f,MHEIGHT-87.7f-6.274)),module,HexSeqP::PATTERN_PARAM);
    patKnob->module=module;
    addParam(patKnob);

    addInput(createInput<SmallPort>(mm2px(Vec(51.5f,MHEIGHT-80.f - 6.287f)),module,HexSeqP::PAT_INPUT));

    auto copyButton = createParam<CopyButton<HexSeqP>>(mm2px(Vec(51.5f,MHEIGHT-71.f-3.2f)),module,HexSeqP::COPY_PARAM);
    copyButton->module = module;
    addParam(copyButton);

    auto pasteButton = createParam<PasteButton<HexSeqP>>(mm2px(Vec(51.5f,MHEIGHT-62.f-3.2f)),module,HexSeqP::PASTE_PARAM);
    pasteButton->module = module;
    addParam(pasteButton);

    for(int k=0;k<NUMSEQ;k++) {
      auto *textField=createWidget<HexFieldP>(mm2px(Vec(3,MHEIGHT-(118.f-((float)k*7.f)))));
      textField->setModule(module);
      textField->setModuleWidget(this);
      textField->nr=k;
      textField->multiline=false;
      fields.push_back(textField);
      addChild(textField);
      for(int j=0;j<16;j++) {
        auto light = createLight<DBSmallLight<GreenLight>>(mm2px(Vec(3.7f+2.75f*j,MHEIGHT-(118.f-((float)k*7.f)+2.f))), module, k*16+j);
        addChild(light);
        light->setVisible(module!=nullptr&&module->showLights);
        lights.push_back(light);
      }
    }
    addOutput(createOutput<SmallPort>(mm2px(Vec(51.5f,MHEIGHT-48.f- 6.287f)),module,HexSeqP::TRG_OUTPUT));
    addOutput(createOutput<SmallPort>(mm2px(Vec(51.5f,MHEIGHT-36.f- 6.287f)),module,HexSeqP::GATE_OUTPUT));
    addOutput(createOutput<SmallPort>(mm2px(Vec(51.5f,MHEIGHT-24.f- 6.287f)),module,HexSeqP::CLK_OUTPUT));
    addOutput(createOutput<SmallPort>(mm2px(Vec(51.5f,MHEIGHT-12.f- 6.287f)),module,HexSeqP::INV_OUTPUT));
    //addParam(createParam<TrimbotWhiteSnap>(mm2px(Vec(51.5f,MHEIGHT-19.5f)),module,HexSeqP::DELAY_PARAM));
  }
  void onHoverKey(const event::HoverKey &e) override {
    if (e.action == GLFW_PRESS) {
      int k=e.key-48;
      if(k>=1&&k<10) {
        fields[k-1]->onWidgetSelect=true;
        APP->event->setSelectedWidget(fields[k-1]);
      }
      if(e.keyName=="f") {
        auto *hexSeqModule=dynamic_cast<HexSeqP *>(this->module);
        hexSeqModule->copyFromHexSeq();
        if(hexSeqModule->CMGate16) {
          INFO("CM found");
          hexSeqModule->copyFromCMGateSeq16();
        }
        if(hexSeqModule->EuclidSeq) {
          INFO("EuclidSeq found");
          hexSeqModule->copyFromEuclid(0);
        }
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
    HexSeqP* module = dynamic_cast<HexSeqP*>(this->module);
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
    HexSeqP* module = dynamic_cast<HexSeqP*>(this->module);
    assert(module);

    menu->addChild(new MenuSeparator);

    menu->addChild(createBoolPtrMenuItem("Pattern Poly Select", "", &module->polySelect));
    menu->addChild(createCheckMenuItem("ShowLights", "",
                                       [=]() {return module->showLights;},
                                       [=]() { toggleLights();}));
    struct DelayItem : MenuItem {
      HexSeqP* module;
      Menu* createChildMenu() override {
        Menu* menu = new Menu;
        for (unsigned int c = 0; c <= 10; c++) {
          menu->addChild(createCheckMenuItem(rack::string::f("%d", c), "",
                                             [=]() {return module->delay == c;},
                                             [=]() {module->delay = c;}
          ));
        }
        return menu;
      }
    };
    auto* delayItem = new DelayItem;
    delayItem->text = "Clock In Delay";
    delayItem->rightText = rack::string::f("%d", module->delay) + "  " + RIGHT_ARROW;
    delayItem->module = module;
    menu->addChild(delayItem);
    menu->addChild(new DensMenuItem<HexSeqP>(module));
    auto* randomLengthFromItem = new IntSelectItem(&module->randomLengthFrom,1,16);
    randomLengthFromItem->text = "Random length from";
    randomLengthFromItem->rightText = rack::string::f("%d", module->randomLengthFrom) + "  " + RIGHT_ARROW;
    menu->addChild(randomLengthFromItem);
    auto* randomLengthToItem = new IntSelectItem(&module->randomLengthTo,1,16);
    randomLengthToItem->text = "Random length to";
    randomLengthToItem->rightText = rack::string::f("%d", module->randomLengthTo) + "  " + RIGHT_ARROW;
    menu->addChild(randomLengthToItem);
    struct RandomizePattern : ui::MenuItem {
      HexSeqP *module;
      RandomizePattern(HexSeqP *m) : module(m) {}
      void onAction(const ActionEvent& e) override {
        if (!module)
          return;
        module->randomizeCurrentPattern();
      }
    };
    auto rpMenu = new RandomizePattern(module);
    rpMenu->text = "Randomize Pattern";
    menu->addChild(rpMenu);
    struct InitializePattern : ui::MenuItem {
      HexSeqP *module;
      InitializePattern(HexSeqP *m) : module(m) {}
      void onAction(const ActionEvent& e) override {
        if (!module)
          return;
        module->initializeCurrentPattern();
      }
    };
    auto ipMenu = new InitializePattern(module);
    ipMenu->text = "Initialize Pattern";
    menu->addChild(ipMenu);

  }
};


Model *modelHexSeqP=createModel<HexSeqP,HexSeqPWidget>("HexSeqP");