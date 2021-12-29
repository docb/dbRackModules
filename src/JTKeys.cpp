#include "dcb.h"
#include <string>

struct JTKeys : Module {
  enum ParamIds {
    NOTE_PARAM,OCT_PARAM,SCALE_PARAM,SCALE_START_PARAM,NUM_PARAMS=SCALE_START_PARAM+93
  };
  enum InputIds {
    NUM_INPUTS
  };
  enum OutputIds {
    VOCT_OUTPUT,GATE_OUTPUT,RST_OUTPUT,NUM_OUTPUTS
  };
  enum LightIds {
    NUM_LIGHTS
  };
  dsp::PulseGenerator gatePulse;

  int on[16]={-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1};
  bool loading = false;
  std::vector<Scale<31>> scaleVector;

  void parseJson(json_t *rootJ) {
    json_t *data=json_object_get(rootJ,"scales");
    size_t len=json_array_size(data);
    for(uint i=0;i<len;i++) {
      json_t *scaleObj=json_array_get(data,i);
      scaleVector.emplace_back(scaleObj);
    }
  }

  bool load(const std::string &path) {
    INFO("Loading scale file %s",path.c_str());
    FILE *file=fopen(path.c_str(),"r");
    if(!file)
      return false;
    DEFER({ fclose(file); });

    json_error_t error;
    json_t *rootJ=json_loadf(file,0,&error);
    if(!rootJ) {
      WARN("%s",string::f("Scales file has invalid JSON at %d:%d %s",error.line,error.column,error.text).c_str());
      return false;
    }
    try {
      parseJson(rootJ);
    } catch(Exception &e) {
      WARN("%s",e.msg.c_str());
      return false;
    }
    json_decref(rootJ);
    return true;
  }

  std::vector<std::string> getLabels() {
    std::vector<std::string> ret;
    for(const auto &scl:scaleVector) {
      ret.push_back(scl.name);
    }
    return ret;
  }

  JTKeys() {
    if(!load(asset::plugin(pluginInstance,"res/scales_31.json"))) {
      INFO("user scale file %s does not exist or failed to load. using default_scales ....","res/scales_31.json");
      if(!load(asset::plugin(pluginInstance,"res/default_scales_31.json"))) {
        throw Exception(string::f("Default Scales are damaged, try reinstalling the plugin"));
      }
    }
    config(NUM_PARAMS,NUM_INPUTS,NUM_OUTPUTS,NUM_LIGHTS);
    configSwitch(SCALE_PARAM,0,scaleVector.size()-1,0.f,"Scales",getLabels());
    configSwitch(NOTE_PARAM,0,11,0.f,"Base Note",{"C","C#/Db","D","D#/Eb","E","F","F#/Gb","G","G#/Ab","A","A#/Bb","B"});
    configSwitch(OCT_PARAM,-4,4,0.f,"Base Octave",{"-4","-3","-2","-1","0","1","2","3","4"});
    for(int k=0;k<93;k++) {
      configSwitch(SCALE_START_PARAM+k,0.f,1.f,0.f,scaleVector[0].labels[k%31],{"off","on"});
    }
    configOutput(RST_OUTPUT,"Trig");
    configOutput(GATE_OUTPUT,"Gate");
    configOutput(VOCT_OUTPUT,"V/Oct");
  }
  void updateLabels() {
    for(int k=0;k<93;k++) {
      paramQuantities[SCALE_START_PARAM+k]->name = scaleVector[(int)params[SCALE_PARAM].getValue()].labels[k%31];
    }
  }

  void updateKey(int key) {
    if(loading) return;
    int keyOn=(int)params[SCALE_START_PARAM+key].getValue();
    int index=-1;
    for(int j=0;j<16;j++) {
      if(on[j]==key)
        index=j;
    }
    if(keyOn==0) {
      if(index>=0)
        on[index]=-1;
    } else {
      if(index==-1) {
        for(int l=0;l<16;l++) {
          if(on[l]==-1) {
            on[l]=key;
            break;
          }
        }
      }
    }
    //INFO("update %d -- %d - %d %d %d %d %d %d %d %d %d %d %d %d %d %d %d %d",key, keyOn,on[0],on[1],on[2],on[3],on[4],on[5],on[6],on[7],on[8],on[9],on[10],on[11],on[12],on[13],on[14],on[15]);
  }

  void process(const ProcessArgs &args) override {
    int currentScale = params[SCALE_PARAM].getValue();
    int note=params[NOTE_PARAM].getValue();
    int oct=params[OCT_PARAM].getValue();
    float baseVolt=(float)note/12.f+oct;

    int maxChannels=0;
    for(int k=0;k<16;k++) {
      int key=on[k];
      if(key>=0) {
        maxChannels=k+1;
        int n=key%31;
        int o=key/31;
        //float t=float(scale[n][0])/float(scale[n][1]);
        float t = scaleVector[currentScale].values[n];
        float lt=log2f(t);
        float voltage=baseVolt+(float)o+lt;
        outputs[VOCT_OUTPUT].setVoltage(voltage,k);
        float gate=outputs[GATE_OUTPUT].getVoltage(k);
        if(gate==0.f)
          gatePulse.trigger();
        bool pulse=gatePulse.process(1.0f/args.sampleRate);
        if(!pulse)
          outputs[RST_OUTPUT].setVoltage(10.0f,k);
        else
          outputs[RST_OUTPUT].setVoltage(0.f,k);
        outputs[GATE_OUTPUT].setVoltage(10.f,k);
      } else {
        outputs[GATE_OUTPUT].setVoltage(0.f,k);
        outputs[VOCT_OUTPUT].setVoltage(0.f,k);
      }
    }
    outputs[VOCT_OUTPUT].setChannels(maxChannels);
    outputs[GATE_OUTPUT].setChannels(maxChannels);
    outputs[RST_OUTPUT].setChannels(maxChannels);

  }
  json_t *dataToJson() override {
    json_t *data=json_object();
    json_t *onList=json_array();
    for(int k=0;k<16;k++) {
      json_array_append_new(onList,json_integer(on[k]));
    }
    json_object_set_new(data,"on",onList);
    return data;
  }

  void dataFromJson(json_t *rootJ) override {
    json_t *data=json_object_get(rootJ,"on");
    if(!data) return;
    for(int k=0;k<16;k++) {
      json_t *onKey=json_array_get(data,k);
      on[k]=json_integer_value(onKey);
    }
    loading = true;// states that the on values are loaded and they should not be overwritten by button change event
  }

};
struct ScaleKnob : TrimbotWhite9 {
  JTKeys *module;
  void onChange(const event::Change& e) override {
    if(module) module->updateLabels();
    SvgKnob::onChange(e);
  }
};

struct KeyButton : SmallButton {
  int key;
  JTKeys *module;
  void onChange(const event::Change& e) override {
    SvgSwitch::onChange(e);
    if(module) module->updateKey(key);
  }
};

struct JTKeysWidget : ModuleWidget {
  JTKeysWidget(JTKeys *module) {
    setModule(module);
    setPanel(APP->window->loadSvg(asset::plugin(pluginInstance,"res/JTKeys.svg")));

    addChild(createWidget<ScrewSilver>(Vec(RACK_GRID_WIDTH,0)));
    addChild(createWidget<ScrewSilver>(Vec(box.size.x-2*RACK_GRID_WIDTH,0)));
    addChild(createWidget<ScrewSilver>(Vec(RACK_GRID_WIDTH,RACK_GRID_HEIGHT-RACK_GRID_WIDTH)));
    addChild(createWidget<ScrewSilver>(Vec(box.size.x-2*RACK_GRID_WIDTH,RACK_GRID_HEIGHT-RACK_GRID_WIDTH)));
    for(int j=0;j<3;j++) {
      for(int k=0;k<31;k++) {
        auto keyButton = createParam<KeyButton>(Vec(10+j*20,30+k*10),module,JTKeys::SCALE_START_PARAM+j*31+(30-k));
        keyButton->module = module;
        keyButton->key =j*31+(30-k);
        addParam(keyButton);
      }
    }
    if(module) module->loading = false;
    int off=-35;
    auto sknob=createParam<ScaleKnob>(Vec(83,110+off),module,JTKeys::SCALE_PARAM);
    sknob->module = module;
    sknob->snap=true;
    addParam(sknob);

    addParam(createParam<TrimbotWhite9Snap>(Vec(83,152+off),module,JTKeys::NOTE_PARAM));

    addParam(createParam<TrimbotWhite9Snap>(Vec(83,194+off),module,JTKeys::OCT_PARAM));

    addOutput(createOutput<SmallPort>(Vec(87,236+off),module,JTKeys::RST_OUTPUT));
    addOutput(createOutput<SmallPort>(Vec(87,278+off),module,JTKeys::GATE_OUTPUT));
    addOutput(createOutput<SmallPort>(Vec(87,320+off),module,JTKeys::VOCT_OUTPUT));
  }
};


Model *modelJTKeys=createModel<JTKeys,JTKeysWidget>("JTKeys");