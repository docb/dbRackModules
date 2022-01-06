#include "dcb.h"
#include "hexfield.h"

#define NUMSEQ 16
#define NUMPAT 16


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
    LIGHTS_LEN
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
  int delay;
  bool polySelect = true;
  float lastClock=0.f;
  RND rnd;
  void setDirty(int nr,bool _dirty) {
    dirty[nr]=_dirty;
  }

  bool isDirty(int nr) {
    return dirty[nr];
  }


  void setHex(int nr,const std::string &hexStr,int pattern=0) {
    printf("SET %d %s\n",nr,hexStr.c_str());
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
    configOutput(TRG_OUTPUT,"Trig");
    configOutput(GATE_OUTPUT,"Gate");
    configOutput(CLK_OUTPUT,"Clock");
    configOutput(INV_OUTPUT,"Inverted");
  }

  static unsigned int hexToInt(const std::string &hex) {
    unsigned int x;
    std::stringstream ss;
    ss<<std::hex<<hex;
    ss>>x;
    return x;
  }

  void next() {
    for(int k=0;k<NUMSEQ;k++) {
      unsigned int len=hexs[currentPattern][k].length();
      if(len>0) {
        //unsigned spos=pos[currentPattern][k]%(len*4);
        unsigned spos=songpos%(len*4);
        unsigned charPos=spos/4;
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
              songpos = 0;
            }
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

  }
  void randomizeField(int j,int k) {
    std::stringstream stream;
    stream<<std::uppercase<<std::setfill('0')<<std::setw(8)<<std::hex<<(rnd.next()&0xFFFFFFFF);
    hexs[j][k]=stream.str();
    dirty[k]=true;
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
    font_size=13;
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
  std::vector<HexField<HexSeqP,HexSeqPWidget>*> fields;

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
    }
    addOutput(createOutput<SmallPort>(mm2px(Vec(51.5f,MHEIGHT-48.f- 6.287f)),module,HexSeqP::TRG_OUTPUT));
    addOutput(createOutput<SmallPort>(mm2px(Vec(51.5f,MHEIGHT-36.f- 6.287f)),module,HexSeqP::GATE_OUTPUT));
    addOutput(createOutput<SmallPort>(mm2px(Vec(51.5f,MHEIGHT-24.f- 6.287f)),module,HexSeqP::CLK_OUTPUT));
    addOutput(createOutput<SmallPort>(mm2px(Vec(51.5f,MHEIGHT-12.f- 6.287f)),module,HexSeqP::INV_OUTPUT));
    //addParam(createParam<TrimbotWhiteSnap>(mm2px(Vec(51.5f,MHEIGHT-19.5f)),module,HexSeqP::DELAY_PARAM));
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
  void appendContextMenu(Menu* menu) override {
    HexSeqP* module = dynamic_cast<HexSeqP*>(this->module);
    assert(module);

    menu->addChild(new MenuSeparator);

    menu->addChild(createBoolPtrMenuItem("Pattern Poly Select", "", &module->polySelect));
    struct DelayItem : MenuItem {
      HexSeqP* module;
      Menu* createChildMenu() override {
        Menu* menu = new Menu;
        for (int c = 0; c <= 10; c++) {
          menu->addChild(createCheckMenuItem(string::f("%d", c), "",
                                             [=]() {return module->delay == c;},
                                             [=]() {module->delay = c;}
          ));
        }
        return menu;
      }
    };
    auto* delayItem = new DelayItem;
    delayItem->text = "Clock In Delay";
    delayItem->rightText = string::f("%d", module->delay) + "  " + RIGHT_ARROW;
    delayItem->module = module;
    menu->addChild(delayItem);

  }
};


Model *modelHexSeqP=createModel<HexSeqP,HexSeqPWidget>("HexSeqP");