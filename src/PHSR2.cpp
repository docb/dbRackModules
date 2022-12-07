#include "dcb.h"
#include "rnd.h"

struct LPoint {
  float x;
  float y;

  LPoint() {
    x=0;
    y=0;
  }

  LPoint(float _x,float _y) : x(_x),y(_y) {
  }

  void set(float _x,float _y) {
    x=_x;
    y=_y;
  }
};

struct LSegOsc {
  float phs=0.f;
  float *py;
  float *px;
  int *plen;
  std::vector<LPoint> *mpoints;

  void init(std::vector<LPoint> *_points,int *length) {
    mpoints=_points;
    plen=length;
  }

  float linseg() {
    if(*plen==0)
      return 0.f;
    if(*plen==1)
      return mpoints->at(0).y;
    float s=mpoints->at(0).x;
    float pre=0;
    int j=1;
    for(;j<*plen;j++) {
      pre=s;
      s=mpoints->at(j).x;
      if(phs<s)
        break;
    }
    float ivl=s-pre;
    float pct=ivl==0?1:(phs-pre)/ivl;
    return mpoints->at(j-1).y+pct*(mpoints->at(j<*plen?j:0).y-mpoints->at(j-1).y);
  }

  void updatePhs(float sampleTime,float freq) {
    phs+=(sampleTime*freq);
    phs-=simd::floor(phs);
  }

  float process() {
    return linseg();
  }

  void reset() {
    phs=0;
  }
};


struct PHSR2 : Module {
  enum ParamId {
    NODES_PARAM,FREQ_PARAM,FM_PARAM,LIN_PARAM,PARAMS_LEN
  };
  enum InputId {
    Y_INPUT,X_INPUT=Y_INPUT+14,VOCT_INPUT=X_INPUT+14,FM_INPUT,RST_INPUT,INPUTS_LEN
  };
  enum OutputId {
    CV_OUTPUT,OUTPUTS_LEN
  };
  enum LightId {
    LIGHTS_LEN
  };
  float py[16]={-5,-2.5,0,2.5,5};
  float px[16]={0,0.25,0.5,0.75,1};
  int len=5;
  bool changed=false;
  bool random=false;
  LSegOsc lsegOsc[16];
  std::vector<LPoint> points={{0,0},{0,0},{0,0},{0,0},{0,0},{0,0},{0,0},{0,0},{0,0},{0,0},{0,0},{0,0},{0,0},{0,0},{0,0},{0,0}};
  RND rnd;
  dsp::SchmittTrigger rstTrigger;
  PHSR2() {
    config(PARAMS_LEN,INPUTS_LEN,OUTPUTS_LEN,LIGHTS_LEN);
    configParam(FREQ_PARAM,-14.f,4.f,0.f,"Frequency"," Hz",2,dsp::FREQ_C4);
    configParam(NODES_PARAM,3,16,5,"Length");
    configButton(LIN_PARAM,"Linear");
    configParam(FM_PARAM,0,1,0,"FM Amount","%",0,100);
    configInput(FM_INPUT,"FM");
    getParamQuantity(NODES_PARAM)->snapEnabled=true;
    for(int k=0;k<14;k++) {
      configInput(Y_INPUT+k,"Y " + std::to_string(k+2));
      configInput(X_INPUT+k,"X " + std::to_string(k+2));
    }
    configInput(VOCT_INPUT,"V/Oct");
    configInput(RST_INPUT,"Reset/Sync");
    configOutput(CV_OUTPUT,"CV");
    changed=true;
    for(int k=0;k<16;k++) {
      lsegOsc[k].init(&points,&len);
    }
  }

  void updatePoint(int idx,float x,float y) {
    if(idx>0) {
      if(x<px[idx-1])
        x=px[idx-1];
    } else {
      x=0;
    }
    if(idx<len-1) {
      if(x>px[idx+1])
        x=px[idx+1];
    } else {
      x=1;
    }
    px[idx]=x;
    py[idx]=y;
    changed=true;
  }

  void process(const ProcessArgs &args) override {
    int _len=params[NODES_PARAM].getValue();
    if(_len<len && !changed) {
      px[_len-1]=1;
      py[_len-1]=py[len-1];
      changed=true;
    } else if(_len>len && !changed) {
      for(int k=0;k<_len;k++) {
        px[k]=float(k)/float(_len-1);
        py[k]=float(k)*10.f/float(_len-1)-5;
      }
      changed=true;
    }
    if(random) {
      _len=rnd.nextRange(3,16);
      getParamQuantity(NODES_PARAM)->setValue(_len);
      for(int k=0;k<_len;k++) {
        px[k]=float(k)/float(_len-1);
        if(k==0) {
          py[k]=-5;
        } else if(k==_len-1) {
          py[k]=5;
        } else {
          py[k]=float(rnd.nextDouble())*10.f-5.f;
        }
      }
      changed=true;
      random=false;
    }
    if(changed) {
      len=_len;
      for(int k=0;k<len;k++) {
        points[k].x=px[k];
        points[k].y=py[k];
      }
      changed=false;
    }
    for(int k=0;k<14;k++) {
      if(k>=len-2) break;
      if(inputs[Y_INPUT+k].isConnected()) {
        py[k+1]=clamp(inputs[Y_INPUT+k].getVoltage(),-5.f,5.f);
        points[k+1].x=px[k+1];
        points[k+1].y=py[k+1];
      }
      if(inputs[X_INPUT+k].isConnected()) {
        float pct=clamp(inputs[X_INPUT+k].getVoltage()*0.1f,0.f,1.f);
        px[k+1]=points[k].x+pct*(points[k+2].x-points[k].x);
        points[k+1].y=py[k+1];
        points[k+1].x=px[k+1];
      }
    }
    if(rstTrigger.process(inputs[RST_INPUT].getVoltage())) {
      for(auto &k:lsegOsc) {
        k.reset();
      }
    }

    int channels=std::max(inputs[VOCT_INPUT].getChannels(),1);
    for(int c=0;c<channels;c++) {
      float freqParam=params[FREQ_PARAM].getValue();
      float fmParam=params[FM_PARAM].getValue();
      bool linear=params[LIN_PARAM].getValue()>0;
      float pitch=freqParam+inputs[VOCT_INPUT].getPolyVoltage(c);
      float freq;
      if(linear) {
        freq=dsp::FREQ_C4*dsp::approxExp2_taylor5(pitch+30.f)/std::pow(2.f,30.f);
        freq+=dsp::FREQ_C4*inputs[FM_INPUT].getPolyVoltage(c)*fmParam;
      } else {
        pitch+=inputs[FM_INPUT].getPolyVoltage(c)*fmParam;
        freq=dsp::FREQ_C4*dsp::approxExp2_taylor5(pitch+30.f)/std::pow(2.f,30.f);
      }
      freq=fmin(freq,args.sampleRate/2);
      float out=lsegOsc[c].process();
      lsegOsc[c].updatePhs(args.sampleTime,freq);
      outputs[CV_OUTPUT].setVoltage(out,c);
    }
    outputs[CV_OUTPUT].setChannels(channels);
  }

  void dataFromJson(json_t *root) override {
    json_t *jLen=json_object_get(root,"len");
    if(jLen) {
      len=json_integer_value(jLen);
      json_t *jPX=json_object_get(root,"px");
      if(jPX) {
        int _len=json_array_size(jPX);
        for(int k=0;k<_len;k++) {
          px[k]=json_real_value(json_array_get(jPX,k));
        }
      }
      json_t *jPY=json_object_get(root,"py");
      if(jPY) {
        int _len=json_array_size(jPY);
        for(int k=0;k<_len;k++) {
          py[k]=json_real_value(json_array_get(jPY,k));
        }
      }
      changed=true;
    }

  }

  json_t *dataToJson() override {
    json_t *root=json_object();
    json_object_set_new(root,"len",json_integer(len));
    json_t *jPX=json_array();
    json_t *jPY=json_array();
    for(int k=0;k<len;k++) {
      json_array_append_new(jPX,json_real(px[k]));
      json_array_append_new(jPY,json_real(py[k]));
    }
    json_object_set_new(root,"px",jPX);
    json_object_set_new(root,"py",jPY);
    return root;
  }

  void onReset(const ResetEvent &e) override {
    getParamQuantity(NODES_PARAM)->setValue(5);
    len=5;
    for(int k=0;k<len;k++) {
      px[k]=float(k)/float(len-1);
      py[k]=float(k)*10.f/float(len-1)-5;
    }
    changed=true;
  }

  void onRandomize(const RandomizeEvent &e) override {
    random=true;
  }
};

#define PSIZE 6

struct LSegDisplay : OpaqueWidget {
  PHSR2 *module=nullptr;
  Vec center;
  NVGcolor lineColor=nvgRGB(0x22,0xaa,0x22);
  NVGcolor bgColor=nvgRGB(0x22,0x22,0x22);
  NVGcolor pointColor=nvgRGB(0x22,0xaa,0xaa);

  int currentIdx=-1;

  float posX=0;
  float posY=0;

  float dragX,dragY;
  float startX,startY;
  bool isDragging=false;

  LSegDisplay(PHSR2 *m,Vec pos,Vec size) : module(m) {
    box.size=size;
    box.pos=pos;
    center={box.size.x/2,box.size.y/2};
  }

  void drawBG(const DrawArgs &args,NVGcolor color) {
    nvgBeginPath(args.vg);
    nvgRect(args.vg,0,0,box.size.x,box.size.y);
    nvgFillColor(args.vg,color);
    nvgFill(args.vg);
  }

  void drawCirc(const DrawArgs &args,float x,float y) {
    nvgBeginPath(args.vg);
    nvgCircle(args.vg,x,y,PSIZE);
    nvgFillColor(args.vg,pointColor);
    nvgFill(args.vg);
  }

  void drawLayer(const DrawArgs &args,int layer) override {
    if(layer==1) {
      _draw(args);
    }
    Widget::drawLayer(args,layer);
  }

  void _draw(const DrawArgs &args) {

    drawBG(args,bgColor);
    std::vector<LPoint> pts={{0,   -5},
                             {0.25,2.5},
                             {0.5, -1},
                             {0.75,-0.5},
                             {1,   5}};
    int len=5;
    if(module) {
      pts=module->points;
      len=module->len;
    }
    nvgBeginPath(args.vg);
    bool first=true;
    for(int k=0;k<len;k++) {
      LPoint lp=pts[k];
      if(first) {
        nvgMoveTo(args.vg,lp.x*box.size.x,box.size.y-(lp.y/10.f+0.5f)*box.size.y);
        first=false;
      } else
        nvgLineTo(args.vg,lp.x*box.size.x,box.size.y-(lp.y/10.f+0.5f)*box.size.y);
    }
    nvgStrokeColor(args.vg,lineColor);
    nvgStrokeWidth(args.vg,3);
    nvgStroke(args.vg);
    for(int k=0;k<len;k++) {
      LPoint lp=pts[k];
      drawCirc(args,lp.x*box.size.x,box.size.y-(lp.y/10.f+0.5f)*box.size.y);
    }
  }

  int findPoint(float x,float y) {
    int j=0;
    auto pts=module->points;
    for(int k=0;k<module->len;k++) {
      LPoint lp=module->points[k];
      float px=lp.x*box.size.x;
      float py=box.size.y-(lp.y/10.f+0.5f)*box.size.y;
      if(x>=px-PSIZE&&x<=px+PSIZE&&y>=py-PSIZE&&y<=py+PSIZE) {
        return j;
      }
      j++;
    }
    return -1;
  }

  void onButton(const event::Button &e) override {
    if(!(e.button==GLFW_MOUSE_BUTTON_LEFT&&(e.mods&RACK_MOD_MASK)==0)) {
      return;
    }
    if(e.action==GLFW_PRESS) {
      e.consume(this);
      currentIdx=findPoint(e.pos.x,e.pos.y);
      if(currentIdx>=0) {
        startX=e.pos.x;
        startY=e.pos.y;
        isDragging=true;
      }
    }
  }

  void onDragStart(const event::DragStart &e) override {
    if(isDragging) {
      dragX=APP->scene->rack->getMousePos().x;
      dragY=APP->scene->rack->getMousePos().y;
    }
  }

  void onDragMove(const event::DragMove &e) override {
    float newDragX=APP->scene->rack->getMousePos().x;
    float newDragY=APP->scene->rack->getMousePos().y;
    if(isDragging) {
      posX=startX+(newDragX-dragX);
      posY=startY+(newDragY-dragY);
      if(posX>box.size.x)
        posX=box.size.x;
      if(posY>box.size.y)
        posY=box.size.y;
      if(posX<0)
        posX=0;
      if(posY<0)
        posY=0;
      float x=posX/box.size.x;
      float y=((box.size.y-posY)/box.size.y-0.5)*10.f;
      module->updatePoint(currentIdx,x,y);
    }
  }

  void onDragEnd(const event::DragEnd &e) override {
    isDragging=false;
  }

};

struct PHSR2Widget : ModuleWidget {
  PHSR2Widget(PHSR2 *module) {
    setModule(module);
    setPanel(createPanel(asset::plugin(pluginInstance,"res/PHSR2.svg")));

    auto display=new LSegDisplay(module,mm2px(Vec(4,8)),mm2px(Vec(94,80)));
    addChild(display);

    float x=2;
    float y=94;
    for(int k=0;k<14;k++) {
      addInput(createInput<SmallPort>(mm2px(Vec(x,y)),module,PHSR2::Y_INPUT+k));
      x+=7;
    }
    y=102;
    x=2;
    for(int k=0;k<14;k++) {
      addInput(createInput<SmallPort>(mm2px(Vec(x,y)),module,PHSR2::X_INPUT+k));
      x+=7;
    }
    y=116;
    addParam(createParam<TrimbotWhite>(mm2px(Vec(6,y)),module,PHSR2::FREQ_PARAM));
    addInput(createInput<SmallPort>(mm2px(Vec(17,y)),module,PHSR2::VOCT_INPUT));
    addInput(createInput<SmallPort>(mm2px(Vec(28,y)),module,PHSR2::FM_INPUT));
    addParam(createParam<TrimbotWhite>(mm2px(Vec(36,y)),module,PHSR2::FM_PARAM));
    addParam(createParam<MLED>(mm2px(Vec(44,y)),module,PHSR2::LIN_PARAM));
    addInput(createInput<SmallPort>(mm2px(Vec(57,y)),module,PHSR2::RST_INPUT));
    addParam(createParam<TrimbotWhite>(mm2px(Vec(65,y)),module,PHSR2::NODES_PARAM));
    addOutput(createOutput<SmallPort>(mm2px(Vec(86,y)),module,PHSR2::CV_OUTPUT));
  }
};


Model *modelPHSR2=createModel<PHSR2,PHSR2Widget>("PHSR2");
