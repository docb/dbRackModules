#include "dcb.h"
#include "rnd.h"
extern Model *modelPad2;
struct Faders : Module {
  enum ParamId {
    FADERS_A,FADERS_B=16,FADERS_C=32,MOD_CV_A=48,MOD_CV_B,MOD_CV_C,PARAMS_LEN
  };
  enum InputId {
    MOD_A_INPUT,MOD_B_INPUT,MOD_C_INPUT,INPUTS_LEN
  };
  enum OutputId {
    OUT_A_OUTPUT,MOD_B_OUTPUT,MOD_C_OUTPUT,OUTPUTS_LEN
  };
  enum LightId {
    LIGHTS_LEN
  };

  int maxChannels[3]={16,16,16};
  float min[3]={-10,-10,-10};
  float max[3]={10,10,10};
  int dirty=0;
  float currentVoltages[3][16]={};
  std::string rowLabels[3]={"A","B","C"};
  RND rnd;
  float snaps[8]={0,1.f/12.f,0.1,10.f/32.f,0.5,0.625,1,10.f/8.f};
  unsigned int currentSnaps[3] = {0,0,0};
  dsp::ClockDivider divider;
  Module *padModule=nullptr;

  Faders() {
    config(PARAMS_LEN,INPUTS_LEN,OUTPUTS_LEN,LIGHTS_LEN);
    divider.setDivision(32);

    for(int i=0;i<3;i++) {
      for(int k=0;k<16;k++) {
        configParam(i*16+k,min[i],max[i],0,"chn "+std::to_string(k+1));
      }
      configParam(MOD_CV_A+i,0,1,0,"Mod CV Amount "+rowLabels[i]);
      configOutput(i,"CV "+rowLabels[i]);
      configInput(i,"Mod CV "+rowLabels[i]);
    }
  }

  float getSnapped(float value,int row,int c) {
    if(currentSnaps[row]>0) {
      float snapInv=1.f/snaps[currentSnaps[row]];
      return roundf(value*snapInv)/snapInv;
    }
    return value;
  }
  void copyFromPad2() {
    for(int k=0;k<48;k++) {
      getParamQuantity(k)->setValue(padModule->params[k+8].getValue()*10.f);
    }
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
    for(int i=0;i<3;i++) {
      if(outputs[i].isConnected()) {
        for(int c=0;c<maxChannels[i];c++) {
          currentVoltages[i][c]=clamp(params[i*16+c].getValue()+inputs[i].getVoltage(c)*params[MOD_CV_A+i].getValue(),min[i],max[i]);
          currentVoltages[i][c]=getSnapped(currentVoltages[i][c],i,c);
          outputs[i].setVoltage(currentVoltages[i][c],c);
        }
        outputs[i].setChannels(maxChannels[i]);
      }
    }
  }

  float getCurrentVoltagePct(int row,int c) {
    return math::rescale(currentVoltages[row][c],min[row],max[row],0.f,1.f);
  }

  void reconfig(int nr) {
    for(int k=0;k<16;k++) {
      float value=getParamQuantity(nr*16+k)->getValue();
      if(value>max[nr])
        value=max[nr];
      if(value<min[nr])
        value=min[nr];
      configParam(nr*16+k,min[nr],max[nr],0,"chn "+std::to_string(k+1));
      getParamQuantity(nr*16+k)->setValue(value);
    }
  }

  void fromJson(json_t *root) override {
    min[0]=min[1]=min[2]=-10.f;
    max[0]=max[1]=max[2]=10.f;
    reconfig(0);
    reconfig(1);
    reconfig(2);
    Module::fromJson(root);
  }

  json_t *dataToJson() override {
    json_t *root=json_object();
    for(int i=0;i<3;i++) {
      json_object_set_new(root,("min"+std::to_string(i)).c_str(),json_real(min[i]));
      json_object_set_new(root,("max"+std::to_string(i)).c_str(),json_real(max[i]));
      json_object_set_new(root,("maxChannels"+std::to_string(i)).c_str(),json_integer(maxChannels[i]));
      json_object_set_new(root,("snap"+std::to_string(i)).c_str(),json_integer(currentSnaps[i]));
    }
    return root;
  }

  void dataFromJson(json_t *root) override {
    for(int i=0;i<3;i++) {
      json_t *jMin=json_object_get(root,("min"+std::to_string(i)).c_str());
      if(jMin) {
        min[i]=json_real_value(jMin);
      }

      json_t *jMax=json_object_get(root,("max"+std::to_string(i)).c_str());
      if(jMax) {
        max[i]=json_real_value(jMax);
      }
      json_t *jMaxChannels=json_object_get(root,("maxChannels"+std::to_string(i)).c_str());
      if(jMaxChannels) {
        maxChannels[i]=json_integer_value(jMaxChannels);
      }
      json_t *jSnap=json_object_get(root,("snap"+std::to_string(i)).c_str());
      if(jSnap) {
        currentSnaps[i]=json_integer_value(jSnap);
      }
      reconfig(i);
    }
  }

  void randomizeValues(int nr) {
    for(int k=0;k<16;k++) {
      int id=nr*16+k;
      getParamQuantity(id)->setValue(math::rescale(float(rnd.nextDouble()),0.f,1.f,min[nr],max[nr]));
    }
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
    if(id>=0&&module&&c<module->maxChannels[row]) {
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
    if(id>=0&&module&&c<module->maxChannels[row]) {
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
      if(id>=0&&module&&c<module->maxChannels[row]) {
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


};

struct FaderRange {
  float min;
  float max;
};

struct FaderRangeSelectItem : MenuItem {
  Faders *module;
  std::vector <FaderRange> ranges;
  int nr;

  FaderRangeSelectItem(Faders *_module,std::vector <FaderRange> _ranges,int _nr) : module(_module),ranges(std::move(_ranges)),nr(_nr) {
  }

  Menu *createChildMenu() override {
    Menu *menu=new Menu;
    for(unsigned int k=0;k<ranges.size();k++) {
      menu->addChild(createCheckMenuItem(string::f("%d/%dV",(int)ranges[k].min,(int)ranges[k].max),"",[=]() {
        return module->min[nr]==ranges[k].min&&module->max[nr]==ranges[k].max;
      },[=]() {
        module->min[nr]=ranges[k].min;
        module->max[nr]=ranges[k].max;
        module->reconfig(nr);
      }));
    }
    return menu;
  }
};

struct SnapSelectItem : MenuItem {
  Faders *module;
  std::vector <std::string> labels;
  int nr;

  SnapSelectItem(Faders *_module,std::vector <std::string> _labels,int _nr) : module(_module),labels(std::move(_labels)),nr(_nr) {
  }

  Menu *createChildMenu() override {
    Menu *menu=new Menu;
    for(unsigned int k=0;k<labels.size();k++) {
      menu->addChild(createCheckMenuItem(labels[k],"",[=]() {
        return module->currentSnaps[nr]==k;
      },[=]() {
        module->currentSnaps[nr]=k;
      }));
    }
    return menu;
  }
};

struct FadersWidget : ModuleWidget {
  FadersWidget(Faders *module) {
    setModule(module);
    setPanel(createPanel(asset::plugin(pluginInstance,"res/Faders.svg")));

    addChild(createWidget<ScrewSilver>(Vec(RACK_GRID_WIDTH,0)));
    addChild(createWidget<ScrewSilver>(Vec(box.size.x-2*RACK_GRID_WIDTH,0)));
    addChild(createWidget<ScrewSilver>(Vec(RACK_GRID_WIDTH,RACK_GRID_HEIGHT-RACK_GRID_WIDTH)));
    addChild(createWidget<ScrewSilver>(Vec(box.size.x-2*RACK_GRID_WIDTH,RACK_GRID_HEIGHT-RACK_GRID_WIDTH)));
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
  }

  void appendContextMenu(Menu *menu) override {
    Faders *module=dynamic_cast<Faders *>(this->module);
    assert(module);
    for(int i=0;i<3;i++) {
      menu->addChild(new MenuSeparator);
      auto channelSelect=new IntSelectItem(&module->maxChannels[i],1,PORT_MAX_CHANNELS);
      channelSelect->text="Polyphonic Channels";
      channelSelect->rightText=string::f("%d",module->maxChannels[i])+"  "+RIGHT_ARROW;
      menu->addChild(channelSelect);
      std::vector <FaderRange> ranges={{-10,10},
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
      rangeSelectItem->rightText=string::f("%d/%dV",(int)module->min[i],(int)module->max[i])+"  "+RIGHT_ARROW;
      menu->addChild(rangeSelectItem);
      struct RandomizeRow : ui::MenuItem {
        Faders *module;
        int nr;
        RandomizeRow(Faders *m,int n) : module(m),nr(n) {}
        void onAction(const ActionEvent& e) override {
          if (!module)
            return;
          module->randomizeValues(nr);
        }
      };

      std::vector <std::string> labels={"None","Semi","0.1","Pat 32","0.5","Pat 16","1","Pat 8"};
      auto snapSelectItem=new SnapSelectItem(module,labels,i);
      snapSelectItem->text="Snap Size";
      snapSelectItem->rightText=labels[module->currentSnaps[i]]+"  "+RIGHT_ARROW;
      menu->addChild(snapSelectItem);

      auto rpMenu = new RandomizeRow(module,i);
      rpMenu->text = "Randomize";
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