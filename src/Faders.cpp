#include "dcb.h"
#include "rnd.h"

extern Model *modelPad2;
#define MAX_PATS 100
struct SlewLimiter {
  float delta=0.f;
  float last=0.f;
  float target=0.f;

  void setParams(float sampleTime,float seconds,float to) {
    if(seconds==0)
      delta=0;
    else if(target!=to) {
      delta=(to-last)*sampleTime/seconds;
    }
    target=to;
  }

  float process() {
    if(delta==0.f) return target;
    if(delta<0.f)
      return last=std::max(last+delta,target);
    return last=std::min(last+delta,target);
  }
};
#define MAX_KNOBS 3
struct FadersPreset {
  float faderValues[48]={};
  float knobValues[MAX_KNOBS]={};

  int maxChannels[3]={16,16,16};
  float min[3]={-10,-10,-10};
  float max[3]={10,10,10};
  unsigned int snaps[3]={0,0,0};

  void reset() {
    for(int k=0;k<48;k++) {
      faderValues[k]=0.f;
    }
    for(int k=0;k<3;k++) {
      maxChannels[k]=16;
      min[k]=-10;
      max[k]=10;
      snaps[k]=0;
    }
    for(int k=0;k<MAX_KNOBS;k++) {
      knobValues[k]=0.f;
    }
  }

  void setValue(int row,int nr,float value) {
    faderValues[row*16+nr]=value;
  }

  void set(const FadersPreset &other) {
    for(int k=0;k<48;k++) {
      faderValues[k]=other.faderValues[k];
    }
    for(int k=0;k<MAX_KNOBS;k++) {
       knobValues[k]=other.knobValues[k];
    }
    for(int k=0;k<3;k++) {
      min[k]=other.min[k];
      max[k]=other.max[k];
      maxChannels[k]=other.maxChannels[k];
    }
  }

  json_t *toJson() {
    json_t *data=json_object();
    json_t *valueList=json_array();
    for(int k=0;k<48;k++) {
      json_array_append_new(valueList,json_real(faderValues[k]));
    }
    json_object_set_new(data,"values",valueList);
    json_t *minList=json_array();
    for(int k=0;k<3;k++) {
      json_array_append_new(minList,json_real(min[k]));
    }
    json_object_set_new(data,"min",minList);

    json_t *maxList=json_array();
    for(int k=0;k<3;k++) {
      json_array_append_new(maxList,json_real(max[k]));
    }
    json_object_set_new(data,"max",maxList);

    json_t *maxChannelList=json_array();
    for(int k=0;k<3;k++) {
      json_array_append_new(maxChannelList,json_integer(maxChannels[k]));
    }
    json_object_set_new(data,"maxChannels",maxChannelList);
    json_t *snapList=json_array();
    for(int k=0;k<3;k++) {
      json_array_append_new(snapList,json_integer(snaps[k]));
    }
    json_object_set_new(data,"snaps",snapList);

    json_t *knobValueList=json_array();
    for(int k=0;k<MAX_KNOBS;k++) {
      json_array_append_new(knobValueList,json_real(knobValues[k]));
    }
    json_object_set_new(data,"knobValues",knobValueList);
    return data;
  }

  void fromJson(json_t *data) {
    json_t *valueList=json_object_get(data,"values");
    for(int k=0;k<48;k++) {
      json_t *jValue=json_array_get(valueList,k);
      faderValues[k]=json_real_value(jValue);
    }
    json_t *minList=json_object_get(data,"min");
    for(int k=0;k<3;k++) {
      json_t *jValue=json_array_get(minList,k);
      min[k]=json_real_value(jValue);
    }
    json_t *maxList=json_object_get(data,"max");
    for(int k=0;k<3;k++) {
      json_t *jValue=json_array_get(maxList,k);
      max[k]=json_real_value(jValue);
    }
    json_t *maxChannelList=json_object_get(data,"maxChannels");
    for(int k=0;k<3;k++) {
      json_t *jValue=json_array_get(maxChannelList,k);
      maxChannels[k]=json_integer_value(jValue);
    }
    json_t *snapList=json_object_get(data,"snaps");
    for(int k=0;k<3;k++) {
      json_t *jValue=json_array_get(snapList,k);
      snaps[k]=json_integer_value(jValue);
    }
    json_t *knobValueList=json_object_get(data,"knobValues");
    for(int k=0;k<MAX_KNOBS;k++) {
      json_t *jValue=json_array_get(knobValueList,k);
      if(jValue)
        knobValues[k]=json_real_value(jValue);
    }
    json_t *jKnob1=json_object_get(data,"knob1");
    if(jKnob1) {
      knobValues[0]=json_real_value(jKnob1);
    }
    json_t *jKnob2=json_object_get(data,"knob2");
    if(jKnob2) {
      knobValues[1]=json_real_value(jKnob2);
    }
  }
};

struct Faders : Module {
  enum ParamId {
    FADERS_A,FADERS_B=16,FADERS_C=32,MOD_CV_A=48,MOD_CV_B,MOD_CV_C,PAT_PARAM,EDIT_PARAM,COPY_PARAM,PASTE_PARAM,GLIDE_PARAM,KNOB_PARAMS,PARAMS_LEN=KNOB_PARAMS+MAX_KNOBS
  };
  enum InputId {
    MOD_A_INPUT,MOD_B_INPUT,MOD_C_INPUT,PAT_INPUT,INPUTS_LEN
  };
  enum OutputId {
    OUT_A_OUTPUT,MOD_B_OUTPUT,MOD_C_OUTPUT,KNOB_OUTPUTS,OUTPUTS_LEN=KNOB_OUTPUTS+MAX_KNOBS
  };
  enum LightId {
    LIGHTS_LEN
  };

  FadersPreset presets[MAX_PATS]={};
  FadersPreset clipBoard={};
  int dirty=0;
  float currentVoltages[3][16]={};
  std::string rowLabels[3]={"A","B","C"};
  RND rnd;
  float snaps[8]={0,1.f/12.f,0.1,10.f/32.f,0.5,0.625,1,10.f/8.f};
  dsp::ClockDivider divider;
  Module *padModule=nullptr;
  SlewLimiter faderSlews[48]={};
  SlewLimiter knobSlews[MAX_KNOBS]={};

  Faders() {
    config(PARAMS_LEN,INPUTS_LEN,OUTPUTS_LEN,LIGHTS_LEN);
    divider.setDivision(32);

    for(int i=0;i<3;i++) {
      for(int k=0;k<16;k++) {
        configParam(i*16+k,-10,10,0,rowLabels[i] + " chn " + std::to_string(k+1));
      }
      configParam(MOD_CV_A+i,0,1,0,"Mod CV Amount "+rowLabels[i]);
      configOutput(i,"CV "+rowLabels[i]);
      configInput(i,"Mod CV "+rowLabels[i]);
    }
    configButton(COPY_PARAM,"Copy Pattern");
    configButton(PASTE_PARAM,"Paste Pattern");
    configButton(EDIT_PARAM,"LOCK");
    configParam(PAT_PARAM,0,MAX_PATS-1,0,"Preset Selection");
    configInput(PAT_INPUT,"Preset Select (0.1V per step)");
    configParam(GLIDE_PARAM,0.f,10.f,0,"Glide");
    for(int k=0;k<MAX_KNOBS;k++) {
      configParam(KNOB_PARAMS+k,-10.f,10,0,"Knob " + std::to_string(k+1));
      configOutput(KNOB_OUTPUTS+k,"Knob "+ std::to_string(k+1));
    }
  }

  void setValue(int row,int nr,float value) {
    int pat=params[PAT_PARAM].getValue();
    presets[pat].setValue(row,nr,value);
  }

  void copy() {
    int pat=params[PAT_PARAM].getValue();
    clipBoard.set(presets[pat]);
  }

  void paste() {
    int pat=params[PAT_PARAM].getValue();
    presets[pat].set(clipBoard);
    setCurrentPattern();
  }

  void setCurrentPattern() {
    int pat=params[PAT_PARAM].getValue();
    for(int nr=0;nr<3;nr++) {
      for(int k=0;k<16;k++) {
        configParam(nr*16+k,presets[pat].min[nr],presets[pat].max[nr],0,rowLabels[nr] +" chn "+std::to_string(k+1));
      }
    }
    for(int k=0;k<48;k++) {
      getParamQuantity(k)->setValue(presets[pat].faderValues[k]);
    }
    for(int k=0;k<MAX_KNOBS;k++) {
      getParamQuantity(KNOB_PARAMS+k)->setValue(presets[pat].knobValues[k]);
    }
  }

  float getSnapped(float value,int row,int c) {
    int pat=params[PAT_PARAM].getValue();
    if(presets[pat].snaps[row]>0) {
      float snapInv=1.f/snaps[presets[pat].snaps[row]];
      return roundf(value*snapInv)/snapInv;
    }
    return value;
  }

  void copyFromPad2() {
    int pat=params[PAT_PARAM].getValue();
    for(int k=0;k<3;k++) {
      presets[pat].min[k]=0;
      presets[pat].max[k]=10;
      presets[pat].maxChannels[k]=16;
      presets[pat].snaps[k]=0;
    }
    for(int k=0;k<48;k++) {
      presets[pat].faderValues[k]=padModule->params[k+8].getValue()*10.f;
    }
    presets[pat].knobValues[0]=rescale(padModule->params[0].getValue(),0.5f,60.f,0.f,10.f);
    presets[pat].knobValues[1]=rescale(padModule->params[2].getValue(),0.5f,4.f,0.f,10.f);
    setCurrentPattern();
  }

  float getMin(int nr) {
    int pat=params[PAT_PARAM].getValue();
    return presets[pat].min[nr];
  }

  float getMax(int nr) {
    int pat=params[PAT_PARAM].getValue();
    return presets[pat].max[nr];
  }

  int getMaxChannels(int nr) {
    int pat=params[PAT_PARAM].getValue();
    return presets[pat].maxChannels[nr];
  }

  int getSnap(int nr) {
    int pat=params[PAT_PARAM].getValue();
    return presets[pat].snaps[nr];
  }

  void setMin(int nr,float value) {
    int pat=params[PAT_PARAM].getValue();
    presets[pat].min[nr]=value;
  }

  void setMax(int nr,float value) {
    int pat=params[PAT_PARAM].getValue();
    presets[pat].max[nr]=value;
  }

  void setMaxChannels(int nr,int value) {
    int pat=params[PAT_PARAM].getValue();
    INFO("set maxchannels %d %d %d",pat,nr,value);
    presets[pat].maxChannels[nr]=value;
  }

  void setSnap(int nr,float value) {
    int pat=params[PAT_PARAM].getValue();
    presets[pat].snaps[nr]=value;
  }

  void setKnob(int nr) {
    int pat=params[PAT_PARAM].getValue();
    presets[pat].knobValues[nr]=params[KNOB_PARAMS+nr].getValue();
  }

  float getValue(int pat,int nr) {
    return faderSlews[nr].process();
  }
  float getKnobValue(int pat,int nr) {
    return knobSlews[nr].process();
  }

  void process(const ProcessArgs &args) override {
    if(leftExpander.module) {
      if(leftExpander.module->model==modelPad2) {
        padModule=leftExpander.module;
      } else {
        padModule=nullptr;
      }
    }

    if(divider.process()) {
      _process(args);
    }
  }

  void _process(const ProcessArgs &args) {
    if(inputs[PAT_INPUT].isConnected() && params[EDIT_PARAM].getValue() == 0) {
      int c=clamp(inputs[PAT_INPUT].getVoltage(),0.f,9.99f)*float(MAX_PATS)/10.f;
      getParamQuantity(PAT_PARAM)->setValue(c);
    }
    int pat=params[PAT_PARAM].getValue();
    float glide=params[GLIDE_PARAM].getValue();
    for(int k=0;k<48;k++) {
      faderSlews[k].setParams(args.sampleTime,glide/32.f,presets[pat].faderValues[k]);
    }
    for(int k=0;k<MAX_KNOBS;k++) {
      knobSlews[k].setParams(args.sampleTime,glide/32.f,presets[pat].knobValues[k]);
    }
    for(int i=0;i<3;i++) {
      if(outputs[i].isConnected()) {
        for(int c=0;c<presets[pat].maxChannels[i];c++) {
          currentVoltages[i][c]=clamp(getValue(pat,i*16+c)+inputs[i].getVoltage(c)*params[MOD_CV_A+i].getValue(),presets[pat].min[i],presets[pat].max[i]);
          currentVoltages[i][c]=getSnapped(currentVoltages[i][c],i,c);
          outputs[i].setVoltage(currentVoltages[i][c],c);
        }
        outputs[i].setChannels(presets[pat].maxChannels[i]);
      }
    }
    for(int k=0;k<MAX_KNOBS;k++) {
      outputs[KNOB_OUTPUTS+k].setVoltage(knobSlews[k].process());
    }
  }

  float getCurrentVoltagePct(int row,int c) {
    int pat=params[PAT_PARAM].getValue();
    return math::rescale(currentVoltages[row][c],presets[pat].min[row],presets[pat].max[row],0.f,1.f);
  }

  void reconfig(int nr) {
    int pat=params[PAT_PARAM].getValue();
    for(int k=0;k<16;k++) {
      float value=getParamQuantity(nr*16+k)->getValue();
      if(value>presets[pat].max[nr])
        value=presets[pat].max[nr];
      if(value<presets[pat].min[nr])
        value=presets[pat].min[nr];
      configParam(nr*16+k,presets[pat].min[nr],presets[pat].max[nr],0,rowLabels[nr] +" chn "+std::to_string(k+1));
      getParamQuantity(nr*16+k)->setValue(value);
    }
  }

  json_t *dataToJson() override {
    json_t *data=json_object();
    json_t *presetList=json_array();
    for(int k=0;k<MAX_PATS;k++) {
      json_array_append_new(presetList,presets[k].toJson());
    }
    json_object_set_new(data,"presets",presetList);
    return data;
  }

  void dataFromJson(json_t *root) override {
    json_t *data=json_object_get(root,"presets");
    if(data) {
      for(int k=0;k<MAX_PATS;k++) {
        json_t *jPreset=json_array_get(data,k);
        presets[k].fromJson(jPreset);
      }
      setCurrentPattern();
    }

  }

  void randomizeValues(int nr) {
    int pat=params[PAT_PARAM].getValue();
    for(int k=0;k<16;k++) {
      int id=nr*16+k;
      getParamQuantity(id)->setValue(math::rescale(float(rnd.nextDouble()),0.f,1.f,presets[pat].min[nr],presets[pat].max[nr]));
    }
  }

  void onRandomize(const RandomizeEvent &e) override {
    for(int k=0;k<3;k++) {
      randomizeValues(k);
      getParamQuantity(KNOB_PARAMS+k)->setValue(math::rescale(float(rnd.nextDouble()),0.f,1.f,-10.f,10.f));
    }
  }
  void onAdd(const AddEvent &e) override {
  }

  void onReset(const ResetEvent &e) override {
    for(int k=0;k<MAX_PATS;k++) {
      presets[k].reset();
    }
    setCurrentPattern();
  }

};

struct Fader : SliderKnob {
  Faders *module=nullptr;
  float pos=0;

  Fader() : SliderKnob() {
    box.size=mm2px(Vec(4.5f,33.f));
    forceLinear=true;
    speed=2.0;
  }

  void drawLayer(const DrawArgs &args,int layer) override {
    if(layer==1) {
      _draw(args);
    }
    Widget::drawLayer(args,layer);
  }

  void _draw(const DrawArgs &args) {
    int c=0;
    int id=-1;
    int row=0;
    ParamQuantity *pq=getParamQuantity();
    if(pq) {
      id=pq->paramId;
      c=id%16;
      row=id/16;
    }
    nvgBeginPath(args.vg);
    nvgRect(args.vg,0,0,box.size.x,box.size.y);
    if(id>=0&&module&&c<module->getMaxChannels(row)) {
      nvgFillColor(args.vg,nvgRGB(0x33,0x33,0x33));
    } else {
      nvgFillColor(args.vg,nvgRGB(0x55,0x55,0x55));
    }
    nvgStrokeColor(args.vg,nvgRGB(0x88,0x88,0x88));
    nvgFill(args.vg);
    nvgStroke(args.vg);

    if(pq) {
      pos=math::rescale(pq->getSmoothValue(),pq->getMinValue(),pq->getMaxValue(),0.f,1.f);
    }
    if(id>=0&&module&&c<module->getMaxChannels(row)) {
      nvgFillColor(args.vg,nvgRGB(0x81,0x7c,0xac));
    } else {
      nvgFillColor(args.vg,nvgRGB(0x77,0x77,0x77));
    }
    nvgBeginPath(args.vg);

    if(pq!=nullptr&&pq->getMinValue()<0) {
      if(pos>=0.5) {
        nvgRect(args.vg,1,box.size.y/2-(box.size.y)*(pos-0.5),box.size.x-2,(box.size.y)*(pos-0.5));
      } else {
        nvgRect(args.vg,1,box.size.y/2,box.size.x-2,-(box.size.y)*(pos-0.5));
      }
    } else {
      nvgRect(args.vg,1,box.size.y-box.size.y*pos,box.size.x-2,box.size.y*pos);
    }
    nvgFill(args.vg);
    if(pq&&module) {
      float vpos=module->getCurrentVoltagePct(row,c);
      nvgBeginPath(args.vg);
      if(id>=0&&module&&c<module->getMaxChannels(row)) {
        nvgFillColor(args.vg,nvgRGB(0x0,0xee,0x88));
      } else {
        nvgFillColor(args.vg,nvgRGB(0x99,0x99,0x99));
      }

      nvgRect(args.vg,1,box.size.y-box.size.y*vpos-1,box.size.x-2,2);
      nvgFill(args.vg);
    }


  }

  void onButton(const event::Button &e) override {
    if(e.button==GLFW_MOUSE_BUTTON_LEFT&&e.action==GLFW_PRESS&&(e.mods&RACK_MOD_MASK)==0) {
      float pct=(box.size.y-e.pos.y)/box.size.y;
      ParamQuantity *pq=getParamQuantity();
      if(pq) {
        float newValue=math::rescale(pct,0.f,1.f,pq->getMinValue(),pq->getMaxValue());
        pq->setValue(newValue);
      }
      e.consume(this);
    }
    Knob::onButton(e);
  }

  void onChange(const ChangeEvent &e) override {
    if(module) {
      ParamQuantity *pq=getParamQuantity();
      if(pq) {
        int id=pq->paramId;
        int c=id%16;
        int row=id/16;
        module->setValue(row,c,pq->getValue());
      }
    }
    ParamWidget::onChange(e);
  }

};

struct FaderRange {
  float min;
  float max;
};

struct FaderRangeSelectItem : MenuItem {
  Faders *module;
  std::vector<FaderRange> ranges;
  int nr;

  FaderRangeSelectItem(Faders *_module,std::vector<FaderRange> _ranges,int _nr) : module(_module),ranges(std::move(_ranges)),nr(_nr) {
  }

  Menu *createChildMenu() override {
    Menu *menu=new Menu;
    for(unsigned int k=0;k<ranges.size();k++) {
      menu->addChild(createCheckMenuItem(string::f("%d/%dV",(int)ranges[k].min,(int)ranges[k].max),"",[=]() {
        return module->getMin(nr)==ranges[k].min&&module->getMax(nr)==ranges[k].max;
      },[=]() {
        module->setMin(nr,ranges[k].min);
        module->setMax(nr,ranges[k].max);
        module->reconfig(nr);
      }));
    }
    return menu;
  }
};

struct SnapSelectItem : MenuItem {
  Faders *module;
  std::vector<std::string> labels;
  int nr;

  SnapSelectItem(Faders *_module,std::vector<std::string> _labels,int _nr) : module(_module),labels(std::move(_labels)),nr(_nr) {
  }

  Menu *createChildMenu() override {
    Menu *menu=new Menu;
    for(unsigned int k=0;k<labels.size();k++) {
      menu->addChild(createCheckMenuItem(labels[k],"",[=]() {
        return module->getSnap(nr)==k;
      },[=]() {
        module->setSnap(nr,k);
      }));
    }
    return menu;
  }
};

struct ChannelSelectItem : MenuItem {
  Faders *module;
  int nr;
  int min;
  int max;

  ChannelSelectItem(Faders *m,int n,int _min,int _max) : module(m),nr(n),min(_min),max(_max) {
  }

  Menu *createChildMenu() override {
    Menu *menu=new Menu;
    for(int c=min;c<=max;c++) {
      menu->addChild(createCheckMenuItem(string::f("%d",c),"",[=]() {
        return module->getMaxChannels(nr)==c;
      },[=]() {
        module->setMaxChannels(nr,c);
      }));
    }
    return menu;
  }
};

struct FadersPresetSelect : SpinParamWidget {
  Faders *module=nullptr;
  FadersPresetSelect() {
    init();
  }
  void onChange(const ChangeEvent &e) override {
    if(module) {
      module->setCurrentPattern();
    }
  }
};

struct SingleKnob : TrimbotWhite {
  Faders *module=nullptr;
  int nr=0;
  void onChange(const ChangeEvent &e) override {
    if(module) {
      module->setKnob(nr);
    }
    TrimbotWhite::onChange(e);
  }
};

struct FadersWidget : ModuleWidget {
  FadersWidget(Faders *module) {
    setModule(module);
    setPanel(createPanel(asset::plugin(pluginInstance,"res/Faders.svg")));
    addChild(createWidget<ScrewSilver>(Vec(0, 0)));
    addChild(createWidget<ScrewSilver>(Vec(box.size.x - 15, 0)));
    addChild(createWidget<ScrewSilver>(Vec(0, 365)));
    addChild(createWidget<ScrewSilver>(Vec(box.size.x - 15, 365)));

    for(int i=0;i<3;i++) {
      for(int k=0;k<16;k++) {
        auto faderParam=createParam<Fader>(mm2px(Vec(3+k*4.5,MHEIGHT-(9+38*(2-i))-33)),module,i*16+k);
        faderParam->module=module;
        addParam(faderParam);
      }
      addParam(createParam<TrimbotWhite>(mm2px(Vec(78,MHEIGHT-(23+38*(2-i)+6.273f))),module,Faders::MOD_CV_A+i));
      addInput(createInput<SmallPort>(mm2px(Vec(78,MHEIGHT-(32.75f+38*(2-i)+6.273f))),module,i));
      addOutput(createOutput<SmallPort>(mm2px(Vec(78,MHEIGHT-(9+38*(2-i)+6.273f))),module,i));
    }
    float x=88;
    float y=13.463;
    auto patParam=createParam<FadersPresetSelect>(mm2px(Vec(x,y)),module,Faders::PAT_PARAM);
    patParam->module=module;
    addParam(patParam);
    y+=10;
    auto editButton=createParam<SmallButtonWithLabel>(mm2px(Vec(x,y)),module,Faders::EDIT_PARAM);
    editButton->setLabel("Lock");
    addParam(editButton);
    y+=4;
    addInput(createInput<SmallPort>(mm2px(Vec(x+0.4,y)),module,Faders::PAT_INPUT));
    y+=8;
    auto copyButton=createParam<MCopyButton<Faders>>(mm2px(Vec(x,y)),module,Faders::COPY_PARAM);
    copyButton->label="Cpy";
    copyButton->module=module;
    addParam(copyButton);
    y+=4;
    auto pasteButton=createParam<MPasteButton<Faders>>(mm2px(Vec(x,y)),module,Faders::PASTE_PARAM);
    pasteButton->label="Pst";
    pasteButton->module=module;
    addParam(pasteButton);
    y+=12;
    for(int k=0;k<MAX_KNOBS;k++) {
      auto knob=createParam<SingleKnob>(mm2px(Vec(x,y)),module,Faders::KNOB_PARAMS+k);
      knob->module=module;
      knob->nr=k;
      addParam(knob);
      y+=8;
      addOutput(createOutput<SmallPort>(mm2px(Vec(x,y)),module,Faders::KNOB_OUTPUTS+k));
      y+=12;
    }
    addParam(createParam<TrimbotWhite>(mm2px(Vec(x,113.213f)),module,Faders::GLIDE_PARAM));

  }

  void appendContextMenu(Menu *menu) override {
    Faders *module=dynamic_cast<Faders *>(this->module);
    assert(module);
    for(int i=0;i<3;i++) {
      menu->addChild(new MenuSeparator);
      auto channelSelect=new ChannelSelectItem(module,i,1,PORT_MAX_CHANNELS);
      channelSelect->text="Polyphonic Channels";
      channelSelect->rightText=string::f("%d",module->getMaxChannels(i))+"  "+RIGHT_ARROW;
      menu->addChild(channelSelect);
      std::vector<FaderRange> ranges={{-10,10},
                                      {-5, 5},
                                      {-3, 3},
                                      {-2, 2},
                                      {-1, 1},
                                      {0,  10},
                                      {0,  5},
                                      {0,  3},
                                      {0,  2},
                                      {0,  1}};
      auto rangeSelectItem=new FaderRangeSelectItem(module,ranges,i);
      rangeSelectItem->text="Range";
      rangeSelectItem->rightText=string::f("%d/%dV",(int)module->getMin(i),(int)module->getMax(i))+"  "+RIGHT_ARROW;
      menu->addChild(rangeSelectItem);
      struct RandomizeRow : ui::MenuItem {
        Faders *module;
        int nr;

        RandomizeRow(Faders *m,int n) : module(m),nr(n) {
        }

        void onAction(const ActionEvent &e) override {
          if(!module)
            return;
          module->randomizeValues(nr);
        }
      };

      std::vector<std::string> labels={"None","Semi","0.1","Pat 32","0.5","Pat 16","1","Pat 8"};
      auto snapSelectItem=new SnapSelectItem(module,labels,i);
      snapSelectItem->text="Snap Size";
      snapSelectItem->rightText=labels[module->getSnap(i)]+"  "+RIGHT_ARROW;
      menu->addChild(snapSelectItem);

      auto rpMenu=new RandomizeRow(module,i);
      rpMenu->text="Randomize";
      menu->addChild(rpMenu);
    }
  }

  void onHoverKey(const event::HoverKey &e) override {
    if(e.action==GLFW_PRESS) {
      if(e.keyName=="f") {
        auto *fadersModule=dynamic_cast<Faders *>(this->module);
        fadersModule->copyFromPad2();
      }
    }
  }
};


Model *modelFaders=createModel<Faders,FadersWidget>("Faders");