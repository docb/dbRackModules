#include "dcb.h"
#include <initializer_list>

#define TWOPIF 6.2831853f

static float getNumber(json_t *value) {
  float ret;
  if(json_is_array(value)) {
    unsigned int vLen=json_array_size(value);
    if(vLen!=2) {
      WARN("array but length = %d\n",vLen);
      return 0.f;
    }
    unsigned int n=json_integer_value(json_array_get(value,0));
    unsigned int p=json_integer_value(json_array_get(value,1));
    ret=float(n)/float(p);
  } else {
    ret=(float)json_number_value(value);
  }
  //INFO("getNumber %f",ret);
  return ret;
}

template<unsigned int S>
struct Table {
  float data[S]={};
  std::string name;

  explicit Table(json_t *jWave) {
    name=json_string_value(json_object_get(jWave,"name"));
    json_t *list=json_object_get(jWave,"values");
    size_t pLen=json_array_size(list);
    printf("parsing %s len=%zu\n",name.c_str(),pLen);
    float partl[128];
    for(unsigned int k=0;k<pLen;k++) {
      partl[k]=getNumber(json_array_get(list,k));
    }
    for(unsigned int i=0;i<S;i++) {
      float val=0.f;
      for(unsigned int k=0;k<pLen;k++) {
        float elem=partl[k];
        val+=(float)(elem*sin((k+1)*2*M_PI*i/65536.0));
      }
      data[i]=val;
    }
  }

  const float &operator[](std::size_t idx) const {
    return data[idx];
  }


};

struct SinLookup {
  std::vector<Table<65536>> tables;
  const float od2p=1/(2*M_PI);
  const float mpih=M_PI/2;

  void initTables(json_t *jWaves) {
    unsigned int wLen=json_array_size(jWaves);
    for(unsigned int k=0;k<wLen;k++) {
      tables.emplace_back(json_array_get(jWaves,k));
    }
  }

  simd::float_4 fsinf(simd::float_4 phs) {
    return simd::sin(phs);
  }

  float fsinf(float fphs,int tblNr=0) {
    int sign=fphs>=0?1:-1;
    int phs=(int)(fphs*od2p*65536);
    return (float)sign*tables[tblNr][(sign*phs)&0xFFFF];
  }

  simd::float_4 fcosf(simd::float_4 phs) {
    return simd::cos(phs);
  }

  float fcosf(float fphs,int tblNr=0) {
    return fsinf(mpih-fphs,tblNr);
  }

  std::vector<std::string> getWaveLabels() {
    std::vector<std::string> ret;
    for(const auto &tbl:tables) {
      ret.push_back(tbl.name);
    }
    return ret;
  }

  unsigned int getSize() {
    return tables.size();
  }
};

template<unsigned int S>
struct Ratio {
  float values[S];
  std::string name;

  Ratio(json_t *jRatio) {
    name=json_string_value(json_object_get(jRatio,"name"));
    json_t *scale=json_object_get(jRatio,"values");
    size_t scaleLen=json_array_size(scale);
    printf("parsing %s len=%zu\n",name.c_str(),scaleLen);
    if(scaleLen!=S)
      throw Exception(string::f("Scale must have exact %d entries",S));
    for(size_t k=0;k<scaleLen;k++) {
      json_t *entry=json_array_get(scale,k);
      values[k]=getNumber(entry);
    }
  }

  const float &operator[](std::size_t idx) const {
    return values[idx];
  }
};

struct AddSynth : Module {
  enum ParamIds {
    RATIO_PARAM,WAVE_PARAM,DMP_PARAM,NUM_PARAMS
  };
  enum InputIds {
    AMP_1_16_INPUT,AMP_16_32_INPUT,VOCT_INPUT,DMP_INPUT,NUM_INPUTS
  };
  enum OutputIds {
    MONO_OUTPUT,NUM_OUTPUTS
  };
  enum LightIds {
    NUM_LIGHTS
  };
  SinLookup sinLookup;
  std::vector<Ratio<32>> ratioVector;
  float phase[32]={0};

  void parseJson(json_t *rootJ) {
    json_t *waves=json_object_get(rootJ,"waves");
    json_t *ratios=json_object_get(rootJ,"ratios");
    sinLookup.initTables(waves);
    size_t len=json_array_size(ratios);
    for(unsigned int i=0;i<len;i++) {
      json_t *ratioObj=json_array_get(ratios,i);
      ratioVector.emplace_back(ratioObj);
    }
  }

  bool load(const std::string &path) {
    INFO("Loading addsynth config file %s",path.c_str());
    FILE *file=fopen(path.c_str(),"r");
    if(!file)
      return false;
    DEFER({
            fclose(file);
          });

    json_error_t error;
    json_t *rootJ=json_loadf(file,0,&error);
    if(!rootJ) {
      WARN("%s",string::f("Addsynth file has invalid JSON at %d:%d %s",error.line,error.column,error.text).c_str());
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

  std::vector<std::string> getRatioLabels() {
    std::vector<std::string> ret;
    for(const auto &scl:ratioVector) {
      ret.push_back(scl.name);
    }
    return ret;
  }

  std::vector<std::string> getWaveLabels() {
    return sinLookup.getWaveLabels();
  }

  AddSynth() {
    if(!load(asset::plugin(pluginInstance,"res/addsynth.json"))) {
      INFO("user addsynth file %s does not exist or failed to load. using default_addsynth.json ....","res/addsynth.json");
      if(!load(asset::plugin(pluginInstance,"res/default_addsynth.json"))) {
        throw Exception(string::f("Default Addsynth config is damaged, try reinstalling the plugin"));
      }
    }
    config(NUM_PARAMS,NUM_INPUTS,NUM_OUTPUTS,NUM_LIGHTS);
    configSwitch(WAVE_PARAM,0,(float)sinLookup.getSize()-0.9f,0.f,"WAVE",getWaveLabels());
    getParamQuantity(WAVE_PARAM)->snapEnabled = true;
    configSwitch(RATIO_PARAM,0,(float)ratioVector.size()-0.9f,0.f,"RATIO",getRatioLabels());
    getParamQuantity(RATIO_PARAM)->snapEnabled = true;
    configParam(DMP_PARAM,0,10,0.f,"DMP");
    configInput(DMP_INPUT,"DMP");
    configInput(AMP_1_16_INPUT,"AMP 0-15");
    configInput(AMP_16_32_INPUT,"AMP 16-31");
    configInput(VOCT_INPUT,"V/Oct");
    configOutput(MONO_OUTPUT,"Mono");
  }


  void process(const ProcessArgs &args) override {
    float amps[32];
    float voct=inputs[VOCT_INPUT].getVoltage();
    float freq=261.626f*powf(2.0f,voct);
    int wave=int(floorf(params[WAVE_PARAM].getValue()));
    int rtNr=int(floorf(params[RATIO_PARAM].getValue()));
    float dmp=10.f;
    if(inputs[DMP_INPUT].isConnected()) {
      dmp=10.f-clamp(inputs[DMP_INPUT].getVoltage(),0.f,10.f);
      setImmediateValue(getParamQuantity(DMP_PARAM),10.f-dmp);
    } else {
      dmp=10.f-clamp(params[DMP_PARAM].getValue(),0.f,10.f);
    }

    for(int k=0;k<32;k++) {
      float dPhase=clamp((freq*args.sampleTime*(ratioVector[rtNr][k]))*TWOPIF,0.f,0.5f);
      phase[k]=eucMod(phase[k]+dPhase,TWOPIF);
    }
    for(int k=0;k<16;k++) {
      amps[k]=inputs[AMP_1_16_INPUT].getVoltage(k)/2;
    }
    for(int k=0;k<16;k++) {
      amps[k+16]=inputs[AMP_16_32_INPUT].getVoltage(k)/2;
    }
    float out=0;
    for(int k=0;k<32;k++) {
      out+=(amps[k]*expf(-dmp*((float)k/32.f))*sinLookup.fsinf(phase[k],wave));
    }
    //out = sinf(phase);
    outputs[MONO_OUTPUT].setVoltage(out);
  }
};


struct AddSynthWidget : ModuleWidget {
  AddSynthWidget(AddSynth *module) {
    setModule(module);
    setPanel(APP->window->loadSvg(asset::plugin(pluginInstance,"res/AddSynth.svg")));



    float xpos=1.9f;
    addParam(createParam<TrimbotWhite>(mm2px(Vec(xpos,12)),module,AddSynth::WAVE_PARAM));
    addParam(createParam<TrimbotWhite>(mm2px(Vec(xpos,24)),module,AddSynth::RATIO_PARAM));
    addParam(createParam<TrimbotWhite>(mm2px(Vec(xpos,42)),module,AddSynth::DMP_PARAM));

    addInput(createInput<SmallPort>(mm2px(Vec(xpos,52)),module,AddSynth::DMP_INPUT));
    addInput(createInput<SmallPort>(mm2px(Vec(xpos,64)),module,AddSynth::VOCT_INPUT));

    addInput(createInput<SmallPort>(mm2px(Vec(xpos,78)),module,AddSynth::AMP_1_16_INPUT));
    addInput(createInput<SmallPort>(mm2px(Vec(xpos,92)),module,AddSynth::AMP_16_32_INPUT));

    addOutput(createOutput<SmallPort>(mm2px(Vec(xpos,116)),module,AddSynth::MONO_OUTPUT));


  }
};


Model *modelAddSynth=createModel<AddSynth,AddSynthWidget>("AddSynth");
