#include "dcb.h"
#include "hexfield.h"

#define NUMSEQ 16
#define NUMPAT 16


struct HexSeqP : Module {
  enum ParamId {
    PATTERN_PARAM,DELAY_PARAM,COPY_PARAM,PASTE_PARAM,PARAMS_LEN
  };
  enum InputId {
    CLK_INPUT,RST_INPUT,PAT_INPUT,INPUTS_LEN
  };
  enum OutputId {
    GATE_OUTPUT,OUTPUTS_LEN
  };
  enum LightId {
    LIGHTS_LEN
  };
  int currentPattern=0;
  unsigned pos[NUMPAT][NUMSEQ]={};
  std::string hexs[NUMPAT][NUMSEQ]={};
  std::string clipBoard[NUMSEQ];
  bool dirty[NUMSEQ]={};
  dsp::PulseGenerator gatePulseGenerators[NUMSEQ];
  dsp::SchmittTrigger clockTrigger;
  dsp::SchmittTrigger rstTrigger;
  dsp::SchmittTrigger patTrigger[NUMSEQ];
  int clockCounter=-1;

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
    configParam(DELAY_PARAM,0.f,10.f,0.f,"Clock Delay");
    configParam(COPY_PARAM,0.f,1.f,0.f,"Copy");
    configParam(PASTE_PARAM,0.f,1.f,0.f,"Paste");
    configInput(CLK_INPUT,"Clock");
    configInput(RST_INPUT,"Reset");
    configInput(PAT_INPUT,"Pattern select");
    configOutput(GATE_OUTPUT,"Gate");
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
        unsigned spos=pos[currentPattern][k]%(len*4);
        unsigned charPos=spos/4;
        std::string hs=hexs[currentPattern][k].substr(charPos,1);
        unsigned int hex=hexToInt(hs);
        unsigned posInChar=spos%4;
        bool on=hex>>(3-posInChar)&0x01;
        if(on) {
          gatePulseGenerators[k].trigger(0.01f);
        }
        pos[currentPattern][k]=(pos[currentPattern][k]+1)%(len*4);
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
    }
  }

  void process(const ProcessArgs &args) override {
    if(rstTrigger.process(inputs[RST_INPUT].getVoltage())) {
      for(int k=0;k<NUMSEQ;k++)
        pos[currentPattern][k]=0;
    }
    if(inputs[PAT_INPUT].isConnected()) {
      /*
      int pat=inputs[PAT_INPUT].getVoltage();
      if(pat!=currentPattern) {
        params[PATTERN_PARAM].setValue(pat);
      }
       */
      for(int k=0;k<NUMPAT;k++) {
        if(patTrigger[k].process(inputs[PAT_INPUT].getVoltage(k))) {
          currentPattern = k;
          for(int j=0;j<NUMSEQ;j++) {
            setDirty(j,true);
            pos[currentPattern][j]=0;
          }
        }
      }
    }
    if(inputs[CLK_INPUT].isConnected()) {
      if(clockTrigger.process(inputs[CLK_INPUT].getVoltage())) {
        clockCounter=params[DELAY_PARAM].getValue();
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
      outputs[GATE_OUTPUT].setVoltage((trigger?10.0f:0.0f),k);
    }
    outputs[GATE_OUTPUT].setChannels(16);

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
};

struct HexFieldP : HexField<HexSeqP> {
  HexFieldP() : HexField<HexSeqP>() {
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
        module->pos[pattern][k]=0;
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
  HexSeqPWidget(HexSeqP *module) {
    setModule(module);
    setPanel(createPanel(asset::plugin(pluginInstance,"res/HexSeqP.svg")));

    addChild(createWidget<ScrewSilver>(Vec(RACK_GRID_WIDTH,0)));
    addChild(createWidget<ScrewSilver>(Vec(box.size.x-2*RACK_GRID_WIDTH,0)));
    addChild(createWidget<ScrewSilver>(Vec(RACK_GRID_WIDTH,RACK_GRID_HEIGHT-RACK_GRID_WIDTH)));
    addChild(createWidget<ScrewSilver>(Vec(box.size.x-2*RACK_GRID_WIDTH,RACK_GRID_HEIGHT-RACK_GRID_WIDTH)));

    addInput(createInput<SmallPort>(mm2px(Vec(51.5f,MHEIGHT-115.5f)),module,HexSeqP::CLK_INPUT));
    addInput(createInput<SmallPort>(mm2px(Vec(51.5f,MHEIGHT-100.5f)),module,HexSeqP::RST_INPUT));

    auto *patKnob=createParam<PatternKnob>(mm2px(Vec(51.5f,MHEIGHT-85.f)),module,HexSeqP::PATTERN_PARAM);
    patKnob->module=module;
    addParam(patKnob);

    addInput(createInput<SmallPort>(mm2px(Vec(51.5f,MHEIGHT-73.5f)),module,HexSeqP::PAT_INPUT));

    auto copyButton = createParam<CopyButton<HexSeqP>>(mm2px(Vec(51.5f,MHEIGHT-59.5f)),module,HexSeqP::COPY_PARAM);
    copyButton->module = module;
    addParam(copyButton);

    auto pasteButton = createParam<PasteButton<HexSeqP>>(mm2px(Vec(51.5f,MHEIGHT-44.5f)),module,HexSeqP::PASTE_PARAM);
    pasteButton->module = module;
    addParam(pasteButton);

    for(int k=0;k<NUMSEQ;k++) {
      auto *textField=createWidget<HexFieldP>(mm2px(Vec(3,MHEIGHT-(118.f-((float)k*7.f)))));
      textField->setModule(module);
      textField->nr=k;
      textField->multiline=false;
      addChild(textField);
    }
    addOutput(createOutput<SmallPort>(mm2px(Vec(51.5f,MHEIGHT-29.5f)),module,HexSeqP::GATE_OUTPUT));
    addParam(createParam<TrimbotWhiteSnap>(mm2px(Vec(51.5f,MHEIGHT-19.5f)),module,HexSeqP::DELAY_PARAM));
  }
};


Model *modelHexSeqP=createModel<HexSeqP,HexSeqPWidget>("HexSeqP");