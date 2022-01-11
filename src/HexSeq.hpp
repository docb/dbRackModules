
#ifndef DBRACKMODULES_HEXSEQ_HPP
#define DBRACKMODULES_HEXSEQ_HPP
#include "dcb.h"
#include "hexfield.h"
#include "rnd.h"
#include "hexutil.h"
#define NUMSEQ 12
struct HexSeq : Module {
  enum ParamIds {
    NUM_PARAMS
  };
  enum InputIds {
    CLK_INPUT,RST_INPUT,NUM_INPUTS
  };
  enum OutputIds {
    GATE_OUTPUTS,NUM_OUTPUTS=GATE_OUTPUTS+NUMSEQ+1
  };
  enum LightIds {
    NUM_LIGHTS = NUMSEQ*16
  };
  unsigned long songpos = 0;
  std::string hexs[NUMSEQ]={};
  dsp::PulseGenerator gatePulseGenerators[NUMSEQ];
  dsp::PulseGenerator gatePulseInvGenerators[NUMSEQ];
  dsp::SchmittTrigger clockTrigger;
  dsp::SchmittTrigger rstTrigger;

  RND rnd;
  float randomDens = 0.3;
  int randomLengthFrom = 8;
  int randomLengthTo = 8;
  bool state[NUMSEQ]={};
  bool dirty[NUMSEQ]={};
  bool showLights = true;

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
      configOutput(GATE_OUTPUTS+k,"Trigger " + std::to_string(k+1));
    }
    configOutput(GATE_OUTPUTS+NUMSEQ,"Polyphonic Trigger");
    configInput(CLK_INPUT,"Clock");
    configInput(RST_INPUT,"Reset");
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
      int len=hexs[k].length();
      if(len>0) {
        unsigned spos=songpos%(len*4);
        unsigned charPos=spos/4;
        lights[k*16+charPos].setBrightness(1.f);
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
      } else {
        state[k]=false;
      }
    }
    songpos++;
  }

  void process(const ProcessArgs &args) override {
    if(rstTrigger.process(inputs[RST_INPUT].getVoltage())) {
      for(int k=0;k<NUMSEQ;k++) {
        songpos = 0;
      }
    }
    if(inputs[CLK_INPUT].isConnected()) {
      if(clockTrigger.process(inputs[CLK_INPUT].getVoltage())) {
        next();
      }
    }
    for(int k=0;k<NUMSEQ;k++) {
      bool trigger=gatePulseGenerators[k].process(1.0/args.sampleRate);
      outputs[GATE_OUTPUTS+k].setVoltage((trigger?10.0f:0.0f));
      outputs[GATE_OUTPUTS+NUMSEQ].setVoltage(trigger?10.0f:0.0f,k);
    }
    outputs[GATE_OUTPUTS+NUMSEQ].setChannels(12);
  }

  json_t *dataToJson() override {
    json_t *data=json_object();
    json_t *hexList=json_array();
    for(int k=0;k<NUMSEQ;k++) {
      json_array_append_new(hexList,json_string(hexs[k].c_str()));
    }
    json_object_set_new(data,"hexStrings",hexList);
    json_object_set_new(data,"showLights",json_boolean(showLights));
    json_object_set_new(data,"randomDens",json_real(randomDens));
    json_object_set_new(data,"randomLengthFrom",json_integer(randomLengthFrom));
    json_object_set_new(data,"randomLengthTo",json_integer(randomLengthTo));
    return data;
  }

  void dataFromJson(json_t *rootJ) override {
    json_t *data=json_object_get(rootJ,"hexStrings");
    for(int k=0;k<NUMSEQ;k++) {
      json_t *hexStr=json_array_get(data,k);
      hexs[k]=json_string_value(hexStr);
      dirty[k]=true;
    }
    json_t *jShowLights = json_object_get(rootJ,"showLights");
    if(jShowLights!=nullptr) showLights = json_boolean_value(jShowLights);
    json_t *jDens =json_object_get(rootJ,"randomDens");
    if(jDens!=nullptr) randomDens = json_real_value(jDens);
    json_t *jRandomLengthFrom =json_object_get(rootJ,"randomLengthFrom");
    if(jRandomLengthFrom!=nullptr) randomLengthFrom= json_integer_value(jRandomLengthFrom);
    json_t *jRandomLengthTo =json_object_get(rootJ,"randomLengthTo");
    if(jRandomLengthTo!=nullptr) randomLengthTo = json_integer_value(jRandomLengthTo);
  }

  void randomizeField(int k) {
    hexs[k]=getRandomHex(rnd,randomDens,randomLengthFrom,randomLengthTo);
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
