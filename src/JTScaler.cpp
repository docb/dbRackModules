#include "dcb.h"



struct ScaleUtils {

  std::vector<Scale<12>> scaleVector;

  float transfer(float in,int curScale,int baseKey=0) {
    float octave=floorf(in);
    float sf=in-octave;
    int semi=(int)roundf(sf*12);
    int sb=(int)(semi+12-baseKey)%12;
    float t=scaleVector[curScale].values[sb];
    float lt=log2f(t);
    float out=octave+(float)baseKey/12.f+lt-(semi<baseKey?1.f:0.f);
    return out;
  }

  void fromJson(json_t *rootJ) {
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
      fromJson(rootJ);
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

};

struct JTScaler : Module,ScaleUtils {

  enum ParamIds {
    BASEKEY_PARAM,SCALE_PARAM,NUM_PARAMS//=SCALE_START_PARAM+ScaleUtils::NUM_SCALES
  };
  enum InputIds {
    VOCT_INPUT,NUM_INPUTS
  };
  enum OutputIds {
    VOCT_OUTPUT,NUM_OUTPUTS
  };
  enum LightIds {
    NUM_LIGHTS
  };
  std::string path;

  JTScaler() {
    if(!load(asset::plugin(pluginInstance,"res/scales.json"))) {
      INFO("user scale file %s does not exist or failed to load. using default_scales ....","res/scales.json");
      if(!load(asset::plugin(pluginInstance,"res/default_scales.json"))) {
        throw Exception(string::f("Default Scales are damaged, try reinstalling the plugin"));
      }
    }

    config(NUM_PARAMS,NUM_INPUTS,NUM_OUTPUTS,NUM_LIGHTS);
    configSwitch(BASEKEY_PARAM,0,11,0.f,"Basekey",{"C","C#/Db","D","D#/Eb","E","F","F#/Gb","G","G#/Ab","A","A#/Bb","B"});
    getParamQuantity(BASEKEY_PARAM)->snapEnabled = true;
    configSwitch(SCALE_PARAM,0,(float)scaleVector.size()-1,0,"Scales",getLabels());
    getParamQuantity(SCALE_PARAM)->snapEnabled = true;
    configInput(VOCT_INPUT,"V/Oct");
    configOutput(VOCT_OUTPUT,"V/Oct");
    configBypass(VOCT_INPUT,VOCT_OUTPUT);

  }

  void process(const ProcessArgs &args) override {
    int channels=inputs[VOCT_INPUT].getChannels();
    for(int k=0;k<channels;k++) {
      outputs[VOCT_OUTPUT].setVoltage(transfer(inputs[VOCT_INPUT].getVoltage(k),params[SCALE_PARAM].getValue(),params[BASEKEY_PARAM].getValue()),k);
    }
    outputs[VOCT_OUTPUT].setChannels(channels);
  }
};

struct ScaleDisplay : TransparentWidget {
  JTScaler *_module;
  std::string _fontPath;
  explicit ScaleDisplay(JTScaler *module) : _module(module),_fontPath(asset::plugin(pluginInstance,"res/FreeMonoBold.ttf")) {}

  void drawLayer(const DrawArgs& args, int layer) override {
    if (layer == 1) {
      _draw(args);
    }
    Widget::drawLayer(args, layer);
  }
  void _draw(const DrawArgs &args)  {
    if(!_module) return;
    std::shared_ptr<Font> font = APP->window->loadFont(_fontPath);
    auto vg=args.vg;
    nvgScissor(vg,0,0,box.size.x,box.size.y);
    Scale<12> s = _module->scaleVector[(int)_module->getParam(JTScaler::SCALE_PARAM).getValue()];
    nvgFontSize(args.vg, 10);
    nvgFontFaceId(args.vg, font->handle);
    NVGcolor textColor = nvgRGB(0xff, 0xff, 0xaa);
    nvgTextAlign(args.vg, NVG_ALIGN_CENTER);
    nvgFillColor(args.vg, textColor);
    float x = box.size.x/2.f;
    for(uint k=0; k<12;k++) {
      nvgText(args.vg, x, 9+k*12, s.labels[k].c_str(), nullptr);
    }

  }
};

struct JTScalerWidget : ModuleWidget {

  explicit JTScalerWidget(JTScaler *module) {
    setModule(module);
    setPanel(APP->window->loadSvg(asset::plugin(pluginInstance,"res/JTScaler.svg")));

    addChild(createWidget<ScrewSilver>(Vec(RACK_GRID_WIDTH,0)));
    addChild(createWidget<ScrewSilver>(Vec(box.size.x-2*RACK_GRID_WIDTH,0)));
    addChild(createWidget<ScrewSilver>(Vec(RACK_GRID_WIDTH,RACK_GRID_HEIGHT-RACK_GRID_WIDTH)));
    addChild(createWidget<ScrewSilver>(Vec(box.size.x-2*RACK_GRID_WIDTH,RACK_GRID_HEIGHT-RACK_GRID_WIDTH)));

    addParam(createParam<TrimbotWhite9>(Vec(9,48),module,JTScaler::SCALE_PARAM));
    auto sd = new ScaleDisplay(module);
    sd->setPosition(Vec(0,78));
    sd->setSize(Vec(44,144));
    addChild(sd);
    addParam(createParam<TrimbotWhite9>(Vec(9,238),module,JTScaler::BASEKEY_PARAM));

    addInput(createInput<SmallPort>(Vec(14,278),module,JTScaler::VOCT_INPUT));
    addOutput(createOutput<SmallPort>(Vec(14,320),module,JTScaler::VOCT_OUTPUT));

    addChild(createWidget<Widget>(mm2px(Vec(-0.0,14.585))));
  }


};


Model *modelJTScaler=createModel<JTScaler,JTScalerWidget>("JTScaler");
