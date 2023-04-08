#include "dcb.h"
#include "hexutil.h"

struct GenScale : Module {
  enum ParamIds {
    NOTE_PARAM,OCT_PARAM,SCALE_PARAM,NUM_PARAMS=SCALE_PARAM+12
  };
  enum InputIds {
    SCL_INPUT,NUM_INPUTS
  };
  enum OutputIds {
    VOCT_OUTPUT,NUM_OUTPUTS
  };
  enum LightIds {
    NUM_LIGHTS
  };

  int maxChannels=8;

  GenScale() {
    config(NUM_PARAMS,NUM_INPUTS,NUM_OUTPUTS,NUM_LIGHTS);

    configSwitch(NOTE_PARAM,0,11,0.f,"Base Note",{"C","C#/Db","D","D#/Eb","E","F","F#/Gb","G","G#/Ab","A","A#/Bb","B"});
    configParam(OCT_PARAM,-4,4,0.f,"Base Octave");
    configParam(SCALE_PARAM+0,0.f,1.f,1.f,"Perfect unison");
    configParam(SCALE_PARAM+1,0.f,1.f,0.f,"Minor Second");
    configParam(SCALE_PARAM+2,0.f,1.f,0.f,"Major Second");
    configParam(SCALE_PARAM+3,0.f,1.f,0.f,"Minor Third");
    configParam(SCALE_PARAM+4,0.f,1.f,0.f,"Major Third");
    configParam(SCALE_PARAM+5,0.f,1.f,0.f,"Perfect Forth");
    configParam(SCALE_PARAM+6,0.f,1.f,0.f,"Tritone");
    configParam(SCALE_PARAM+7,0.f,1.f,0.f,"Perfect Fifth");
    configParam(SCALE_PARAM+8,0.f,1.f,0.f,"Minor Sixth");
    configParam(SCALE_PARAM+9,0.f,1.f,0.f,"Major Sixth");
    configParam(SCALE_PARAM+10,0.f,1.f,0.f,"Minor Seventh");
    configParam(SCALE_PARAM+11,0.f,1.f,0.f,"Major Seventh");
    configInput(SCL_INPUT,"Scale");
    configOutput(VOCT_OUTPUT,"V/Oct");
  }

  void process(const ProcessArgs &args) override {
    if(inputs[SCL_INPUT].isConnected()) {
      for(int k=0;k<12;k++) {
        getParamQuantity(SCALE_PARAM+k)->setValue(inputs[SCL_INPUT].getVoltage(k)>1.f);
      }
    }
    float start=params[OCT_PARAM].getValue()+params[NOTE_PARAM].getValue()/12.f;
    int pos=0;
    float sum=0;
    for(int k=SCALE_PARAM;k<NUM_PARAMS;k++) {
      sum+=params[k].getValue();
    }
    if(sum==0)
      return;

    for(int k=0;k<maxChannels;k++) {
      while(params[SCALE_PARAM+pos%12].getValue()==0.f) {
        pos++;
      };
      int oct=pos/12;
      float out=start+(float)oct+(float)(pos%12)/12.f;
      if(out>10.f)
        break;
      outputs[VOCT_OUTPUT].setVoltage(out,k);
      pos++;
    }
    outputs[VOCT_OUTPUT].setChannels(maxChannels);

  }

  json_t *dataToJson() override {
    json_t *root=json_object();
    json_object_set_new(root,"channels",json_integer(maxChannels));
    return root;
  }

  void dataFromJson(json_t *root) override {
    json_t *jChannels=json_object_get(root,"channels");
    if(jChannels) {
      maxChannels=json_integer_value(jChannels);
    }
  }

};


struct GenScaleWidget : ModuleWidget {
  explicit GenScaleWidget(GenScale *module) {
    setModule(module);
    setPanel(APP->window->loadSvg(asset::plugin(pluginInstance,"res/GenScale.svg")));

    addChild(createWidget<ScrewSilver>(Vec(RACK_GRID_WIDTH,0)));
    addChild(createWidget<ScrewSilver>(Vec(box.size.x-2*RACK_GRID_WIDTH,0)));
    addChild(createWidget<ScrewSilver>(Vec(RACK_GRID_WIDTH,RACK_GRID_HEIGHT-RACK_GRID_WIDTH)));
    addChild(createWidget<ScrewSilver>(Vec(box.size.x-2*RACK_GRID_WIDTH,RACK_GRID_HEIGHT-RACK_GRID_WIDTH)));

    for(int k=0;k<12;k++) {
      addParam(createParam<SmallButton>(Vec(12,68+k*10),module,GenScale::SCALE_PARAM+(11-k)));
    }

    addInput(createInput<SmallPort>(Vec(13,190),module,GenScale::SCL_INPUT));

    addParam(createParam<TrimbotWhite9Snap>(Vec(9,236),module,GenScale::NOTE_PARAM));

    addParam(createParam<TrimbotWhite9Snap>(Vec(9,278),module,GenScale::OCT_PARAM));

    addOutput(createOutput<SmallPort>(Vec(13,320),module,GenScale::VOCT_OUTPUT));

  }

  void appendContextMenu(Menu *menu) override {
    GenScale *module=dynamic_cast<GenScale *>(this->module);
    assert(module);
    menu->addChild(new MenuSeparator);
    auto channelSelect=new IntSelectItem(&module->maxChannels,1,PORT_MAX_CHANNELS);
    channelSelect->text="Polyphonic Channels";
    channelSelect->rightText=rack::string::f("%d",module->maxChannels)+"  "+RIGHT_ARROW;
    menu->addChild(channelSelect);
  }
};


Model *modelGenScale=createModel<GenScale,GenScaleWidget>("GenScale");
