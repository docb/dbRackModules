
#ifndef DBRACKMODULES_HEXSEQ_HPP
#define DBRACKMODULES_HEXSEQ_HPP
#include "dcb.h"
#include "hexfield.h"
#include "rnd.h"
#define NUMSEQ 12
struct HexSeq : Module {
  enum ParamIds {
    NUM_PARAMS
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
  dsp::PulseGenerator gatePulseInvGenerators[NUMSEQ];
  dsp::SchmittTrigger clockTrigger;
  dsp::SchmittTrigger rstTrigger;

  RND rnd;

  bool state[NUMSEQ]={};
  bool dirty[NUMSEQ]={};

  void setDirty(int nr,bool _dirty) {
    dirty[nr]=_dirty;
  }

  bool isDirty(int nr,int pattern=0) {
    return dirty[nr];
  }

  void setHex(int nr,const std::string &hexStr) {
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
          state[k] = true; // used by expander
         } else {
          state[k] = false;
          gatePulseInvGenerators[k].trigger(0.01f); // process by expander
        }
        pos[k]=(pos[k]+1)%(len*4);
      } else {
        state[k]=false;
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
      dirty[k]=true;
    }

  }

  void randomizeField(int k) {
    std::stringstream stream;
    stream<<std::uppercase<<std::setfill('0')<<std::setw(8)<<std::hex<<(rnd.next()&0xFFFFFFFF);
    hexs[k]=stream.str();
    dirty[k]=true;
  }

  void onRandomize(const RandomizeEvent &e) override {
    for(int k=0;k<NUMSEQ;k++) {
      randomizeField(k);
    }
  }

  void onReset(const ResetEvent &e) override {
    for(int k=0;k<NUMSEQ;k++) {
      hexs[k]="";
      dirty[k]=true;
    }
  }
};

#endif //DBRACKMODULES_HEXSEQ_HPP
