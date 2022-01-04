#include "dcb.h"
#include "hexfield.h"
#include "rnd.h"
#define NUMSEQ 12

struct HexSeq : Module {
  enum ParamIds {
    /*MODE_PARAM,*/NUM_PARAMS
  };
  enum InputIds {
    CLK_INPUT,RST_INPUT,NUM_INPUTS
  };
  enum OutputIds {
    GATE_OUTPUTS,NUM_OUTPUTS=GATE_OUTPUTS+NUMSEQ
  };
  enum LightIds {
    NUM_LIGHTS
  };
  unsigned pos[NUMSEQ]={};
  std::string hexs[NUMSEQ]={};
  dsp::PulseGenerator gatePulseGenerators[NUMSEQ];
  dsp::SchmittTrigger clockTrigger;
  dsp::SchmittTrigger rstTrigger;

  RND rnd;

  bool state[NUMSEQ];

  bool dirty[NUMSEQ] = {};

  void setDirty(int nr, bool _dirty) {
    dirty[nr] = _dirty;
  }
  bool isDirty(int nr, int pattern = 0) {
    return dirty[nr];
  }


  void setHex(int nr,const std::string& hexStr) {
    printf("SET %d %s\n",nr,hexStr.c_str());
    hexs[nr]=hexStr;
  }

  std::string getHex(int nr) {
    return hexs[nr];
  }

  HexSeq() {
    config(NUM_PARAMS,NUM_INPUTS,NUM_OUTPUTS,NUM_LIGHTS);
    for(int k=0;k<NUMSEQ;k++) {
      configOutput(GATE_OUTPUTS+k,std::to_string(k));
    }
    configInput(CLK_INPUT,"Clock");
    configInput(RST_INPUT,"Reset");
    //configSwitch(MODE_PARAM, 0.0, 2.0, 0, "Mode", {"T", "G", "L"});
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
      int len=hexs[k].length();
      if(len>0) {
        unsigned spos=pos[k]%(len*4);
        unsigned charPos=spos/4;
        std::string hs=hexs[k].substr(charPos,1);
        unsigned int hex=hexToInt(hs);
        unsigned posInChar=spos%4;
        bool on=hex>>(3-posInChar)&0x01;
        if(on) {
          gatePulseGenerators[k].trigger(0.01f);
        }
        pos[k]=(pos[k]+1)%(len*4);
      }
    }
  }

  void process(const ProcessArgs &args) override {
    if(rstTrigger.process(inputs[RST_INPUT].getVoltage())) {
      for(int k=0;k<NUMSEQ;k++)
        pos[k]=0;
    }
    if(inputs[CLK_INPUT].isConnected()) {
      if(clockTrigger.process(inputs[CLK_INPUT].getVoltage())) {
        next();
      }
    }
    for(int k=0;k<NUMSEQ;k++) {
      bool trigger=gatePulseGenerators[k].process(1.0/args.sampleRate);
      outputs[GATE_OUTPUTS+k].setVoltage((trigger?10.0f:0.0f));
    }

  }

  json_t *dataToJson() override {
    json_t *data=json_object();
    json_t *hexList=json_array();
    for(int k=0;k<NUMSEQ;k++) {
      json_array_append_new(hexList,json_string(hexs[k].c_str()));
    }
    json_object_set_new(data,"hexStrings",hexList);
    return data;
  }

  void dataFromJson(json_t *rootJ) override {
    json_t *data=json_object_get(rootJ,"hexStrings");
    for(int k=0;k<NUMSEQ;k++) {
      json_t *hexStr=json_array_get(data,k);
      hexs[k]=json_string_value(hexStr);
      dirty[k] = true;
    }

  }

  void onRandomize(const RandomizeEvent& e) override {
    for(int k=0;k<NUMSEQ;k++) {
      std::stringstream stream;
      stream << std::uppercase << std::setfill ('0') << std::setw(4)  << std::hex << (rnd.next()&0xFFFFFFFF);
      hexs[k] = stream.str();
      dirty[k] = true;
    }
  }

  void onReset(const ResetEvent& e) override {
    for(int k=0;k<NUMSEQ;k++) {
      hexs[k]="";
      dirty[k]=true;
    }
  }
};



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
    //addParam(createParamCentered<CKSSThreeHorizontal>(mm2px(Vec(30, MHEIGHT-115.5f)), module, HexSeq::MODE_PARAM));

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