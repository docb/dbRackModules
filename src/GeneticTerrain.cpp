#include "dcb.h"
#include "Computer.hpp"

//#define USE_SIMD
template<typename T>
struct GeneticTerrainOSC {
  T phase=0.f;
  T freq=220.f;
  Computer<T> *computer;
  DCBlocker<T> dcBlock;
  float phs=TWOPIF;

  void reset() {
    dcBlock.reset();
  }

  void setPitch(T voct) {
    //freq = dsp::FREQ_C4 * dsp::approxExp2_taylor5(voct + 30) / 1073741824;
    freq=261.626f*simd::pow(2.0f,voct);
    //freq = 207.652f * simd::pow(2.0f, voct);

  }

  void updateExtPhase(T extPhs,bool backWards) {
    if(backWards) {
      phase=simd::fmod(-extPhs*phs,phs);
    } else {
      phase=simd::fmod(extPhs*phs,phs);
    }
  }

  void updatePhase(float oneSampleTime,bool backWards) {
    T dPhase=simd::clamp(freq*oneSampleTime*phs,0.f,0.5f);
    if(backWards) {
      phase=simd::fmod(phase-dPhase,phs);
    } else {
      phase=simd::fmod(phase+dPhase,phs);
    }
  }

  T process(int *genParam,int len,int curve,T curveParam,T cx,T cy,T rx,T ry,T rot) {
    T xc,yc;
    computer->crv(curve,phase,cx,cy,rx,ry,curveParam,xc,yc);
    computer->rotate_point(cx,cy,rot,xc,yc);
    return dcBlock.process(computer->genomFunc(genParam,4,xc,yc));
  }

  T process(int *genParam,int len,T cx,T cy,T rx,T ry,T rot,T y,T z,T n1,T n2,T n3,T a,T b) {
    T xc,yc;
    computer->superformula(phase,cx,cy,rx,ry,y,z,n1,n2,n3,a,b,xc,yc);
    computer->rotate_point(cx,cy,rot,xc,yc);
    return dcBlock.process(computer->genomFunc(genParam,4,xc,yc));
  }
};


struct GeneticTerrain : Module {
  enum ParamIds {
    G1_PARAM,G2_PARAM,G3_PARAM,G4_PARAM,X_PARAM,Y_PARAM,RX_PARAM,RY_PARAM,ROT_PARAM,CURVE_PARAM,CURVE_TYPE_PARAM,ZOOM_PARAM,X_SCL_PARAM,Y_SCL_PARAM,RX_SCL_PARAM,RY_SCL_PARAM,ROT_SCL_PARAM,CP_SCL_PARAM,GATE_OSC_PARAM,CENTER_PARAM,SIMD_PARAM,NUM_PARAMS
  };
  enum InputIds {
    VOCT_INPUT,GATE_INPUT,X_INPUT,Y_INPUT,RX_INPUT,RY_INPUT,ROT_INPUT,CURVE_TYPE_INPUT,CURVE_PARAM_INPUT,PHS_INPUT,NUM_INPUTS
  };
  enum OutputIds {
    LEFT_OUTPUT,RIGHT_OUTPUT,NUM_OUTPUTS
  };
  enum LightIds {
    NUM_LIGHTS
  };
  int genParam[4];
  int lastParam[4];
  Computer<float_4> computer;
  GeneticTerrainOSC<float_4> osc[4];
  GeneticTerrainOSC<float_4> oscBw[4];
  Computer<float> fcomputer;
  GeneticTerrainOSC<float> fosc[16];
  GeneticTerrainOSC<float> foscBw[16];
  Averager<16> xavg;
  Averager<16> yavg;
  dsp::ClockDivider paramUpdaterDivider;

  float cx,cy;
  float rx,ry,rot,curveParam;
  float_4 voct,cpIn,xIn,yIn,rxIn,ryIn,rotIn;
  float voctf,cpInf,xInf,yInf,rxInf,ryInf,rotInf;
  int curve;

  bool state=false;
  bool center=false;
  bool reset;
  RND rnd;
  std::vector<std::string> labels={"NONE","sin(x)","cos(y)","sin(x+y)","cos(x-y)","sin(x*x)","cos(y*y)","sin(x*cos(y))","cos(y*sin(x)","sin(sqr(y+x))*x+sqr(sin(x)*cos(y))","Simplex Noise","Value Noise Uni","Value Noise Discrete 0.5","Value Noise Discrete 0.7","Value Noise Discrete 0.9","Value Noise Cauchy","Value Noise ArcSin","cos(x/y)","tri(x*x+y*y)","saw(x*x+y*y)","pls(x*x+y*y)","tri(x*x)*tri(y*y)","saw(x*x)*saw(y*y)","pls(x*x)*pls(y*y)",
                                   "atan(((y)+tan((x+y)-sin(x+PI)-sin(x*y/PI)*sin((y*x+PI)))))","tanh(sin(sqr(y+x))*x+sqr(sin(x)*cos(y)))","Noise","Weibull Noise"};


  GeneticTerrain() {
    config(NUM_PARAMS,NUM_INPUTS,NUM_OUTPUTS,NUM_LIGHTS);
    configSwitch(G1_PARAM,-1,Computer<float>::NUM_TERRAINS-1,0.f,"T1",labels);
    configSwitch(G2_PARAM,-1,Computer<float>::NUM_TERRAINS-1,1.f,"T2",labels);
    configSwitch(G3_PARAM,-1,Computer<float>::NUM_TERRAINS-1,-1.f,"T3",labels);
    configSwitch(G4_PARAM,-1,Computer<float>::NUM_TERRAINS-1,-1.f,"T4",labels);
    configParam(X_PARAM,-20.f,20.f,0.f,"X");
    configParam(Y_PARAM,-20.f,20.f,0.f,"Y");
    configParam(CURVE_PARAM,-2.f,4.f,0,"CP");
    configSwitch(CURVE_TYPE_PARAM,0,Computer<float>::NUM_CURVES-1,0,"Curve",{"Ellipse","Lemniskate","Limacon","Cornoid","Trisec","Scarabeus","Folium","Talbot","Lissajous","Hypocycloid","Rect"});
    configParam(ROT_PARAM,0.f,2.f*M_PI,0,"ROT");
    configParam(RX_PARAM,0.f,4.f,0.5f,"RX");
    configParam(RY_PARAM,0.f,4.f,0.5f,"RY");

    configParam(X_SCL_PARAM,0.f,1.f,0.f,"X_CV","%",0.f,100.f);
    configParam(Y_SCL_PARAM,0.f,1.f,0.f,"Y_CV","%",0.f,100.f);
    configParam(CP_SCL_PARAM,0.f,1.f,0,"CP_CV","%",0.f,100.f);
    configParam(ROT_SCL_PARAM,0.f,1.f,0,"ROT_CV","%",0.f,100.f);
    configParam(RX_SCL_PARAM,0.f,1.f,0.0f,"RX_CV","%",0.f,100.f);
    configParam(RY_SCL_PARAM,0.f,1.f,0.0f,"RY_CV","%",0.f,100.f);
    configParam(GATE_OSC_PARAM,0.f,1.0f,0.0f,"GATE_OSC");
    configParam(CENTER_PARAM,0.f,1.0f,0.0f,"CENTER");
    configParam(ZOOM_PARAM,0.1f,10.f,5.f,"ZOOM");
    configInput(VOCT_INPUT,"1V/oct pitch");
    configInput(CURVE_TYPE_INPUT,"Curve (0-9V)");
    configInput(GATE_INPUT,"Gate");
    configInput(X_INPUT,"Modulation X");
    configInput(Y_INPUT,"Modulation Y");
    configInput(ROT_INPUT,"Modulation Rotation");
    configInput(RX_INPUT,"Modulation Scale X");
    configInput(RY_INPUT,"Modulation Scale Y");
    configInput(CURVE_PARAM_INPUT,"Modulation Curve Parameter");
    configInput(PHS_INPUT,"Ext Phase");
    configOutput(RIGHT_OUTPUT,"Right Out");
    configOutput(LEFT_OUTPUT,"Left Out");

    //configButton(SIMD_PARAM,"use simd");

    for(int k=0;k<4;k++) {
      osc[k].computer=&computer;
      oscBw[k].computer=&computer;
    }
    for(int k=0;k<16;k++) {
      fosc[k].computer=&fcomputer;
      foscBw[k].computer=&fcomputer;
    }
    paramUpdaterDivider.setDivision(32);
  }

  void onReset(const ResetEvent &e) override {
    Module::onReset(e);
    reset=true;
  }

  void onRandomize(const RandomizeEvent &e) override {

    params[RX_PARAM].setValue((float)rnd.nextDouble());
    params[RY_PARAM].setValue((float)rnd.nextDouble());
    params[CURVE_PARAM].setValue((float)rnd.nextDouble());
    int _curve=(int)(rnd.nextDouble()*Computer<float>::NUM_CURVES);
    params[CURVE_TYPE_PARAM].setValue((float)_curve);
    int cdist[20]={1,2,2,2,2,2,2,2,2,3,3,3,3,3,3,3,4,4,4,4};
    int count=cdist[(int)(rnd.nextDouble()*20)];
    for(int k=0;k<4;k++) { // the first 4 params are the terrain parameters
      if(k<count) {
        params[k].setValue(rnd.nextDouble()*Computer<float>::NUM_TERRAINS);
      } else {
        params[k].setValue(-1);
      }
    }
    params[X_PARAM].setValue(((float)rnd.nextTri(12)-0.5f)*40.f);// approx gauss dist
    params[Y_PARAM].setValue(((float)rnd.nextTri(12)-0.5f)*40.f);// approx gauss dist

    // tell the view it should be centered
    center=true;
  }

  void changed() {
    if(state)
      return;
    if(!(lastParam[0]==genParam[0]&&lastParam[1]==genParam[1]&&lastParam[2]==genParam[2]&&lastParam[3]==genParam[3])) {
      state=true;
      lastParam[0]=genParam[0];
      lastParam[1]=genParam[1];
      lastParam[2]=genParam[2];
      lastParam[3]=genParam[3];
    }
  }


  bool isOn(float_4 gate) {
    return gate[0]>0.f||gate[1]>0.f||gate[2]>0.f||gate[3]>0.f;
  }

  int getCurve() {
    int _curve=clamp(inputs[CURVE_TYPE_INPUT].getNormalVoltage(floorf(params[CURVE_TYPE_PARAM].getValue())),0.f,10.99f);
    getParamQuantity(CURVE_TYPE_PARAM)->setValue(_curve);
    return _curve;
  }

  void process(const ProcessArgs &args) override {
    if(reset) {
      reset=false;
      for(int k=0;k<4;k++) {
        osc[k].reset();
        oscBw[k].reset();
      }
      for(int k=0;k<16;k++) {
        outputs[LEFT_OUTPUT].setVoltage(0.f,k);
        outputs[RIGHT_OUTPUT].setVoltage(0.f,k);
      }
    }
    if(paramUpdaterDivider.process()) { // seems to save 0.5% CPU
      genParam[0]=floorf(params[G1_PARAM].getValue());
      genParam[1]=floorf(params[G2_PARAM].getValue());
      genParam[2]=floorf(params[G3_PARAM].getValue());
      genParam[3]=floorf(params[G4_PARAM].getValue());
      changed();
      curve=getCurve();
      curveParam=params[CURVE_PARAM].getValue();
      cx=xavg.process(params[X_PARAM].getValue());
      cy=yavg.process(params[Y_PARAM].getValue());

      rx=params[RX_PARAM].getValue();
      ry=params[RY_PARAM].getValue();
      rot=params[ROT_PARAM].getValue();
    }
    bool extMode=inputs[PHS_INPUT].isConnected();
    int channels=std::max(extMode?inputs[PHS_INPUT].getChannels():inputs[VOCT_INPUT].getChannels(),1);
    for(int c=0;c<channels;c+=4) {
      auto *oscil=&osc[c/4];
      float_4 gate=inputs[GATE_INPUT].getVoltageSimd<float_4>(c);
      float gateOsc=params[GATE_OSC_PARAM].getValue();
      if(isOn(gate)||gateOsc==0.0f) {
        voct=inputs[VOCT_INPUT].getVoltageSimd<float_4>(c);
        cpIn=inputs[CURVE_PARAM_INPUT].getVoltageSimd<float_4>(c)*params[CP_SCL_PARAM].getValue()+curveParam;
        xIn=inputs[X_INPUT].getVoltageSimd<float_4>(c)*params[X_SCL_PARAM].getValue()+cx;
        yIn=inputs[Y_INPUT].getVoltageSimd<float_4>(c)*params[Y_SCL_PARAM].getValue()+cy;
        rxIn=inputs[RX_INPUT].getVoltageSimd<float_4>(c)*params[RX_SCL_PARAM].getValue()+rx;
        ryIn=inputs[RY_INPUT].getVoltageSimd<float_4>(c)*params[RY_SCL_PARAM].getValue()+ry;
        rotIn=inputs[ROT_INPUT].getVoltageSimd<float_4>(c)*params[ROT_SCL_PARAM].getValue()+rot;
        if(extMode) {
          oscil->updateExtPhase(inputs[PHS_INPUT].getVoltageSimd<float_4>(c)/5.f,false);
        } else {
          oscil->setPitch(voct);
          oscil->updatePhase(args.sampleTime,false);
        }
        float_4 outL=oscil->process(genParam,4,curve,cpIn,xIn,yIn,rxIn,ryIn,rotIn);
        outputs[LEFT_OUTPUT].setVoltageSimd(outL*5.f,c);

        if(outputs[RIGHT_OUTPUT].isConnected()) {
          auto *oscilBw=&oscBw[c/4];
          if(extMode) {
            oscilBw->updateExtPhase(inputs[PHS_INPUT].getVoltageSimd<float_4>(c)/5.f,true);
          } else {
            oscilBw->setPitch(voct);
            oscilBw->updatePhase(args.sampleTime,true);
          }
          float_4 outR=oscilBw->process(genParam,4,curve,cpIn,xIn,yIn,rxIn,ryIn,rotIn);
          outputs[RIGHT_OUTPUT].setVoltageSimd(outR*5.f,c);
        }
      } else {
        outputs[LEFT_OUTPUT].setVoltageSimd(float_4(0.f),c);
        outputs[RIGHT_OUTPUT].setVoltageSimd(float_4(0.f),c);
      }
    }
    outputs[LEFT_OUTPUT].setChannels(channels);
    if(outputs[RIGHT_OUTPUT].isConnected()) {
      outputs[RIGHT_OUTPUT].setChannels(channels);
    }
  }
};

struct GenImage {
  unsigned char *data;
  Vec _size;
  Computer<float> *computer;

  GenImage(Vec size) : _size(size) {
    data=(unsigned char *)malloc(size.x*size.y*4);
  }

  virtual ~GenImage() {
    free(data);
  }

  void updateImage(const int *g,float minX,float minY,float maxX,float maxY) const {
    int w=_size.x;
    int h=_size.y;
    float z=M_PI;
    for(int x=0;x<w;x++) {
      for(int y=0;y<h;y++) {
        float v=computer->genomFunc(g,4,minX+(maxX-minX)*((float)x/w),minY+(maxY-minY)*((float)(y)/h));
        data[4*(y*w+x)+3]=255;
        data[4*(y*w+x)]=((sinf(z*v)+1)*0.5*0.1+0.1)*255;
        data[4*(y*w+x)+1]=((sinf(z*v)+1)*0.5*0.3+0.3)*255;
        data[4*(y*w+x)+2]=((sinf(z*v)+1)*0.5*0.4+0.2)*255;
      }
    }
  }
};

struct TerrainDisplay : TransparentWidget {
  GenImage image;
  Computer<float> computer;
  GeneticTerrain *_module;
  Vec _size;
  int imageHandle=-1;
  float posX=0;
  float posY=0;
  float offX=0;
  float offY=0;
  float startOffX=0;
  float startOffY=0;

  float dragX,dragY;
  float startX,startY;
  bool isDragging=false;
  bool isMoving=false;
  float sx,sy;
  float lastZoom;
  float colors[16][3]={{255,0,  0},
                       {0,  255,0},
                       {0,  0,  255},
                       {255,255,0},
                       {255,0,  255},
                       {0,  255,255},
                       {128,0,  0},
                       {0,  128,0},
                       {128,128,128},
                       {255,128,0},
                       {255,0,  128},
                       {0,  128,255},
                       {128,0,  128},
                       {128,255,0},
                       {128,128,255},
                       {128,255,255}};

  TerrainDisplay(GeneticTerrain *module,Vec size) : image(size) {
    image.computer=&computer;
    _module=module;
    _size=size;
    sx=_size.x/2;
    sy=_size.y/2;

    lastZoom=module->params[GeneticTerrain::ZOOM_PARAM].getValue();
    image.updateImage(module->genParam,-offX-lastZoom,-offY-lastZoom,-offX+lastZoom,-offY+lastZoom);
    paramToCoord();
  }

  void centerView() {
    float px=_module->params[GeneticTerrain::X_PARAM].getValue();
    float py=_module->params[GeneticTerrain::Y_PARAM].getValue();
    offX=-px;
    offY=-py;
    image.updateImage(_module->genParam,-offX-lastZoom,-offY-lastZoom,-offX+lastZoom,-offY+lastZoom);
  }

  void coordToParam() const {
    float zoom=_module->params[GeneticTerrain::ZOOM_PARAM].getValue();
    float px=zoom*(posX-sx)/sx;
    float py=zoom*(posY-sy)/sy;
    _module->params[GeneticTerrain::X_PARAM].setValue(px);
    _module->params[GeneticTerrain::Y_PARAM].setValue(py);
  }

  void paramToCoord() {
    float zoom=_module->params[GeneticTerrain::ZOOM_PARAM].getValue();
    float px=_module->params[GeneticTerrain::X_PARAM].getValue();
    float py=_module->params[GeneticTerrain::Y_PARAM].getValue();
    posX=(px/zoom)*sx+sx;
    posY=(py/zoom)*sy+sy;
  }

  void onButton(const event::Button &e) override {
    if(!(e.button==GLFW_MOUSE_BUTTON_LEFT&&(e.mods&RACK_MOD_MASK)==0)) {
      return;
    }
    if(e.action==GLFW_PRESS) {
      float zoom=_module->params[GeneticTerrain::ZOOM_PARAM].getValue();
      e.consume(this);
      float realPosX=posX+(offX/zoom)*sx;
      float realPosY=posY+(offY/zoom)*sy;
      if(e.pos.x>=realPosX-5&&e.pos.x<=realPosX+5&&e.pos.y>=realPosY-5&&e.pos.y<=realPosY+5) {
        startX=e.pos.x;
        startY=e.pos.y;
        isDragging=true;
      } else {
        isMoving=true;
        startOffX=offX;
        startOffY=offY;
        dragX=APP->scene->rack->getMousePos().x;
        dragY=APP->scene->rack->getMousePos().y;
        //printf("START %f %f %f %f %f %f %f %f\n",startOffX, startOffY,offX,offY,posX,posY,e.pos.x,e.pos.y);
      }
    } else {
      isDragging=false;
      isMoving=false;
    }
  }

  void onDragStart(const event::DragStart &e) override {
    if(isDragging||isMoving) {
      dragX=APP->scene->rack->getMousePos().x;
      dragY=APP->scene->rack->getMousePos().y;
    }
  }

  void onDragMove(const event::DragMove &e) override {
    float newDragX=APP->scene->rack->getMousePos().x;
    float newDragY=APP->scene->rack->getMousePos().y;
    float zoom=_module->params[GeneticTerrain::ZOOM_PARAM].getValue();
    if(isDragging) {
      posX=startX+(newDragX-dragX)-(offX/zoom)*sx;
      posY=startY+(newDragY-dragY)-(offY/zoom)*sy;
      coordToParam();
    }
    if(isMoving) {
      float mX=(newDragX-dragX);
      float mY=(newDragY-dragY);
      offX=(zoom*mX/sx)+startOffX;
      offY=(zoom*mY/sy)+startOffY;
      //printf("MOVE %f %f %f %f %f %f\n",mX, mY,offX,offY,posX,posY);
      image.updateImage(_module->genParam,-offX-zoom,-offY-zoom,-offX+zoom,-offY+zoom);

    }
  }

  void onDragEnd(const event::DragEnd &e) override {
    isDragging=false;
    isMoving=false;
  }

  void drawCurve(const DrawArgs &args,int channel=-1) {
    int curve=_module->getCurve();
    float param=_module->params[GeneticTerrain::CURVE_PARAM].getValue();
    float rx=_module->params[GeneticTerrain::RX_PARAM].getValue();
    float ry=_module->params[GeneticTerrain::RY_PARAM].getValue();
    float rot=_module->params[GeneticTerrain::ROT_PARAM].getValue();
    float zoom=_module->params[GeneticTerrain::ZOOM_PARAM].getValue();
    float xc=0;
    float yc=0;
    float cx=0;
    float cy=0;
    if(channel>=0) {
      param=_module->inputs[GeneticTerrain::CURVE_PARAM_INPUT].getVoltage(channel)*_module->params[GeneticTerrain::CP_SCL_PARAM].getValue()+param;
      cx=_module->inputs[GeneticTerrain::X_INPUT].getVoltage(channel)*_module->params[GeneticTerrain::X_SCL_PARAM].getValue()+cx;
      cy=_module->inputs[GeneticTerrain::Y_INPUT].getVoltage(channel)*_module->params[GeneticTerrain::Y_SCL_PARAM].getValue()+cy;
      rx=_module->inputs[GeneticTerrain::RX_INPUT].getVoltage(channel)*_module->params[GeneticTerrain::RX_SCL_PARAM].getValue()+rx;
      ry=_module->inputs[GeneticTerrain::RY_INPUT].getVoltage(channel)*_module->params[GeneticTerrain::RY_SCL_PARAM].getValue()+ry;
      rot=_module->inputs[GeneticTerrain::ROT_INPUT].getVoltage(channel)*_module->params[GeneticTerrain::ROT_SCL_PARAM].getValue()+rot;
    }
    float delta=0.01;
    float len=2*M_PI;
    float t=0;
    int k=0;
    float oX=(offX/zoom)*sx;
    float oY=(offY/zoom)*sy;
    NVGcolor curveColor=channel==-1?nvgRGB(255,255,255):nvgRGB(colors[channel][0],colors[channel][1],colors[channel][2]);
    nvgStrokeColor(args.vg,curveColor);
    nvgBeginPath(args.vg);
    nvgStrokeWidth(args.vg,10/zoom);
    while(t<len) {
      computer.crv(curve,t,cx,cy,rx,ry,param,xc,yc);
      computer.rotate_point(cx,cy,rot,xc,yc);
      xc=(sx/zoom)*xc;
      yc=(sy/zoom)*yc;
      if(k==0)
        nvgMoveTo(args.vg,xc+oX+posX,yc+oY+posY);
      else
        nvgLineTo(args.vg,xc+oX+posX,yc+oY+posY);

      k++;
      t+=delta;
    }
    nvgStroke(args.vg);
  }

  void drawLayer(const DrawArgs &args,int layer) override {
    if(layer==1) {
      _draw(args);
    }
    Widget::drawLayer(args,layer);
  }

  void _draw(const DrawArgs &args) {
    if(_module->center) {
      centerView();
      _module->center=false;
    }
    nvgScissor(args.vg,0,0,_size.x,_size.y);
    float zoom=_module->params[GeneticTerrain::ZOOM_PARAM].getValue();
    if(_module->state) {
      image.updateImage(_module->genParam,-offX-zoom,-offY-zoom,-offX+zoom,-offY+zoom);
      _module->state=false;
    }
    if(zoom!=lastZoom) {
      image.updateImage(_module->genParam,-offX-zoom,-offY-zoom,-offX+zoom,-offY+zoom);
      lastZoom=zoom;
      paramToCoord();
    }
    if(imageHandle==-1) {
      imageHandle=nvgCreateImageRGBA(args.vg,_size.x,_size.y,0,image.data);
    } else {
      nvgUpdateImage(args.vg,imageHandle,image.data);
    }

    NVGpaint imgPaint=nvgImagePattern(args.vg,0,0,_size.x,_size.y,0,imageHandle,1);
    nvgBeginPath(args.vg);
    nvgRect(args.vg,0,0,_size.x,_size.y);
    nvgFillPaint(args.vg,imgPaint);
    nvgFill(args.vg);

    NVGcolor ballColor=nvgRGB(25,150,252);
    nvgStrokeColor(args.vg,ballColor);

    float oX=(offX/zoom)*sx;
    float oY=(offY/zoom)*sy;

    nvgFillColor(args.vg,ballColor);
    nvgBeginPath(args.vg);
    nvgCircle(args.vg,posX+oX,posY+oY,20/zoom);
    nvgFill(args.vg);
    drawCurve(args);
    for(int k=0;k<16;k++) {
      if(_module->inputs[GeneticTerrain::GATE_INPUT].getVoltage(k)>0) {
        drawCurve(args,k);
      }
    }
  }
};

template<typename T>
struct CenterButton : SvgSwitch {
  T *module;

  CenterButton() {
    momentary=true;
    addFrame(APP->window->loadSvg(asset::plugin(pluginInstance,"res/SmallButton0.svg")));
    addFrame(APP->window->loadSvg(asset::plugin(pluginInstance,"res/SmallButton1.svg")));
  }

  void onChange(const ChangeEvent &e) override {
    if(module)
      module->centerView();
    SvgSwitch::onChange(e);
  }
};

template<typename T>
struct XYKnob : TrimbotWhite9 {
  T *td;

  XYKnob() : TrimbotWhite9() {

  }

  void onChange(const ChangeEvent &e) override {
    if(td)
      td->paramToCoord();
    TrimbotWhite9::onChange(e);
  }
};

struct GeneticTerrainWidget : ModuleWidget {
  GeneticTerrainWidget(GeneticTerrain *module) {
    setModule(module);
    setPanel(APP->window->loadSvg(asset::plugin(pluginInstance,"res/GeneticTerrain.svg")));

    addChild(createWidget<ScrewSilver>(Vec(RACK_GRID_WIDTH,0)));
    addChild(createWidget<ScrewSilver>(Vec(box.size.x-2*RACK_GRID_WIDTH,0)));
    addChild(createWidget<ScrewSilver>(Vec(RACK_GRID_WIDTH,RACK_GRID_HEIGHT-RACK_GRID_WIDTH)));
    addChild(createWidget<ScrewSilver>(Vec(box.size.x-2*RACK_GRID_WIDTH,RACK_GRID_HEIGHT-RACK_GRID_WIDTH)));

    if(module!=nullptr) {
      auto display=new TerrainDisplay(module,Vec(360,360));
      display->box.pos=mm2px(Vec(15,MHEIGHT-121.92f-2.5f));
      display->box.size=Vec(360,360);
      addChild(display);
      auto cb=createParam<CenterButton<TerrainDisplay>>(mm2px(Vec(140.75,MHEIGHT-25.65f-3.2f)),module,GeneticTerrain::CENTER_PARAM);
      cb->module=display;
      addChild(cb);
      auto xparam=createParam<XYKnob<TerrainDisplay>>(mm2px(Vec(139.75f,MHEIGHT-112.5f-4.5f)),module,GeneticTerrain::X_PARAM);
      xparam->td=display;
      addParam(xparam);
      auto yparam=createParam<XYKnob<TerrainDisplay>>(mm2px(Vec(139.75f,MHEIGHT-100.f-4.5f)),module,GeneticTerrain::Y_PARAM);
      yparam->td=display;
      addParam(yparam);
    }
    addParam(createParam<TrimbotWhite9Snap>(mm2px(Vec(3,MHEIGHT-108.f-9.f)),module,GeneticTerrain::G1_PARAM));
    addParam(createParam<TrimbotWhite9Snap>(mm2px(Vec(3,MHEIGHT-93.f-9.f)),module,GeneticTerrain::G2_PARAM));
    addParam(createParam<TrimbotWhite9Snap>(mm2px(Vec(3,MHEIGHT-78.f-9.f)),module,GeneticTerrain::G3_PARAM));
    addParam(createParam<TrimbotWhite9Snap>(mm2px(Vec(3,MHEIGHT-63.f-9.f)),module,GeneticTerrain::G4_PARAM));


    addParam(createParam<TrimbotWhite9>(mm2px(Vec(139.75,MHEIGHT-7.75f-9.f)),module,GeneticTerrain::ZOOM_PARAM));
    addOutput(createOutputCentered<SmallPort>(mm2px(Vec(156.75,MHEIGHT-12.f)),module,GeneticTerrain::LEFT_OUTPUT));
    addOutput(createOutputCentered<SmallPort>(mm2px(Vec(166.75,MHEIGHT-12.f)),module,GeneticTerrain::RIGHT_OUTPUT));

    addInput(createInputCentered<SmallPort>(mm2px(Vec(156.75f,MHEIGHT-112.5f)),module,GeneticTerrain::X_INPUT));
    addParam(createParam<TrimbotWhite>(mm2px(Vec(166.814-3.15f,MHEIGHT-112.5f-3.15)),module,GeneticTerrain::X_SCL_PARAM));


    addInput(createInputCentered<SmallPort>(mm2px(Vec(156.75f,MHEIGHT-100)),module,GeneticTerrain::Y_INPUT));
    addParam(createParam<TrimbotWhite>(mm2px(Vec(166.814-3.15f,MHEIGHT-100-3.15)),module,GeneticTerrain::Y_SCL_PARAM));

    addParam(createParam<TrimbotWhite9Snap>(mm2px(Vec(139.75f,MHEIGHT-87.5f-4.5f)),module,GeneticTerrain::CURVE_TYPE_PARAM));
    addInput(createInputCentered<SmallPort>(mm2px(Vec(156.75f,MHEIGHT-87.5)),module,GeneticTerrain::CURVE_TYPE_INPUT));

    addParam(createParam<TrimbotWhite9>(mm2px(Vec(139.75f,MHEIGHT-70.5f-9)),module,GeneticTerrain::CURVE_PARAM));
    addInput(createInputCentered<SmallPort>(mm2px(Vec(156.75f,MHEIGHT-75)),module,GeneticTerrain::CURVE_PARAM_INPUT));
    addParam(createParam<TrimbotWhite>(mm2px(Vec(166.814-3.15f,MHEIGHT-75-3.15)),module,GeneticTerrain::CP_SCL_PARAM));

    addParam(createParam<TrimbotWhite9>(mm2px(Vec(139.75f,MHEIGHT-58-9)),module,GeneticTerrain::ROT_PARAM));
    addInput(createInputCentered<SmallPort>(mm2px(Vec(156.75f,MHEIGHT-62.5)),module,GeneticTerrain::ROT_INPUT));
    addParam(createParam<TrimbotWhite>(mm2px(Vec(166.75-3.15f,MHEIGHT-62.5-3.15)),module,GeneticTerrain::ROT_SCL_PARAM));


    addParam(createParam<TrimbotWhite9>(mm2px(Vec(139.75f,MHEIGHT-45.25f-9)),module,GeneticTerrain::RX_PARAM));
    addInput(createInputCentered<SmallPort>(mm2px(Vec(156.75f,MHEIGHT-50)),module,GeneticTerrain::RX_INPUT));
    addParam(createParam<TrimbotWhite>(mm2px(Vec(166.75-3.15f,MHEIGHT-50-3.15)),module,GeneticTerrain::RX_SCL_PARAM));

    addParam(createParam<TrimbotWhite9>(mm2px(Vec(139.75f,MHEIGHT-32.75-9)),module,GeneticTerrain::RY_PARAM));
    addInput(createInputCentered<SmallPort>(mm2px(Vec(156.75f,MHEIGHT-37.5f)),module,GeneticTerrain::RY_INPUT));
    addParam(createParam<TrimbotWhite>(mm2px(Vec(166.75-3.15f,MHEIGHT-37.5f-3.15)),module,GeneticTerrain::RY_SCL_PARAM));

    addInput(createInputCentered<SmallPort>(mm2px(Vec(8,MHEIGHT-50)),module,GeneticTerrain::PHS_INPUT));
    addInput(createInputCentered<SmallPort>(mm2px(Vec(8,MHEIGHT-28)),module,GeneticTerrain::GATE_INPUT));
    addParam(createParam<SmallRoundButton>(mm2px(Vec(7,MHEIGHT-38)),module,GeneticTerrain::GATE_OSC_PARAM));
    addInput(createInputCentered<SmallPort>(mm2px(Vec(8,MHEIGHT-14)),module,GeneticTerrain::VOCT_INPUT));
    //addParam(createParam<SmallRoundButton>(mm2px(Vec(166,MHEIGHT-25.65f-3.2f)),module,GeneticTerrain::SIMD_PARAM));

  }
};


Model *modelGeneticTerrain=createModel<GeneticTerrain,GeneticTerrainWidget>("GeneticTerrain");

/****************************** SUPERTERRAIN ************************************/

struct GeneticSuperTerrain : Module {
  enum ParamIds {
    G1_PARAM,G2_PARAM,G3_PARAM,G4_PARAM,X_PARAM,Y_PARAM,RX_PARAM,RY_PARAM,ROT_PARAM,M0_PARAM,M1_PARAM,N1_PARAM,N1_SGN_PARAM,N2_PARAM,N3_PARAM,A_PARAM,B_PARAM,X_SCL_PARAM,Y_SCL_PARAM,RX_SCL_PARAM,RY_SCL_PARAM,ROT_SCL_PARAM,N1_SCL_PARAM,N2_SCL_PARAM,N3_SCL_PARAM,A_SCL_PARAM,B_SCL_PARAM,CENTER_PARAM,ZOOM_PARAM,GATE_OSC_PARAM,SIMD_PARAM,NUM_PARAMS
  };
  enum InputIds {
    VOCT_INPUT,GATE_INPUT,X_INPUT,Y_INPUT,RX_INPUT,RY_INPUT,ROT_INPUT,N1_INPUT,N2_INPUT,N3_INPUT,A_INPUT,B_INPUT,PHS_INPUT,NUM_INPUTS
  };
  enum OutputIds {
    LEFT_OUTPUT,RIGHT_OUTPUT,NUM_OUTPUTS
  };
  enum LightIds {
    NUM_LIGHTS
  };
  int genParam[4];
  int lastParam[4];
  float m0=4;
  float m1=4;
  float n1=1;
  float n2=1;
  float n3=1;
  float a=1;
  float b=1;
  float rx=0.5;
  float ry=0.5;
  float rot=0;
  Computer<float_4> computer;
  GeneticTerrainOSC<float_4> osc[4];
  GeneticTerrainOSC<float_4> oscBw[4];
  Computer<float> fcomputer;
  GeneticTerrainOSC<float> fosc[16];
  GeneticTerrainOSC<float> foscBw[16];
  Averager<16> xavg;
  Averager<16> yavg;
  dsp::ClockDivider paramUpdaterDivider;
  float cx,cy;
  bool state=false;
  bool center=false;
  bool reset=false;
  RND rnd;
  std::vector<std::string> labels={"NONE","sin(x)","cos(y)","sin(x+y)","cos(x-y)","sin(x*x)","cos(y*y)","sin(x*cos(y))","cos(y*sin(x)","sin(sqr(y+x))*x+sqr(sin(x)*cos(y))","Simplex Noise","Value Noise Uni","Value Noise Discrete 0.5","Value Noise Discrete 0.7","Value Noise Discrete 0.9","Value Noise Cauchy","Value Noise ArcSin","cos(x/y)","tri(x*x+y*y)","saw(x*x+y*y)","pls(x*x+y*y)","tri(x*x)*tri(y*y)","saw(x*x)*saw(y*y)","pls(x*x)*pls(y*y)",
                                   "atan(((y)+tan((x+y)-sin(x+PI)-sin(x*y/PI)*sin((y*x+PI)))))","tanh(sin(sqr(y+x))*x+sqr(sin(x)*cos(y)))","Noise","Weibull Noise"};

  GeneticSuperTerrain() {
    config(NUM_PARAMS,NUM_INPUTS,NUM_OUTPUTS,NUM_LIGHTS);
    configSwitch(G1_PARAM,-1,Computer<float>::NUM_TERRAINS-1,0.f,"T1",labels);
    configSwitch(G2_PARAM,-1,Computer<float>::NUM_TERRAINS-1,-1.f,"T2",labels);
    configSwitch(G3_PARAM,-1,Computer<float>::NUM_TERRAINS-1,-1.f,"T3",labels);
    configSwitch(G4_PARAM,-1,Computer<float>::NUM_TERRAINS-1,-1.f,"T4",labels);
    configParam(M0_PARAM,0.f,16.f,4.f,"M0");
    configParam(M1_PARAM,0.f,16.f,4.f,"M1");
    configParam(X_PARAM,-10.f,10.f,0.f,"X");
    configParam(Y_PARAM,-10.f,10.f,0.f,"Y");
    configParam(ROT_PARAM,0.f,2.f*M_PI,0,"ROT");
    configParam(RX_PARAM,0.f,4.f,0.5f,"RX");
    configParam(RY_PARAM,0.f,4.f,0.5f,"RY");
    configParam(N1_PARAM,0.05f,5.f,1.f,"N1");
    configParam(N1_SGN_PARAM,0.f,1.f,0.f,"N1_SGN");
    configParam(N2_PARAM,-5.f,5.f,1.f,"N2");
    configParam(N3_PARAM,-5.f,5.f,1.f,"N3");
    configParam(A_PARAM,0.05f,5.f,1.f,"A");
    configParam(B_PARAM,0.05f,5.f,1.f,"B");

    configParam(X_SCL_PARAM,0.f,1.f,0.f,"X_CV","%",0,100);
    configParam(Y_SCL_PARAM,0.f,1.f,0.f,"Y_CV","%",0,100);
    configParam(ROT_SCL_PARAM,0.f,1.f,0,"ROT_CV","%",0,100);
    configParam(RX_SCL_PARAM,0.f,1.f,0.0f,"RX_CV","%",0,100);
    configParam(RY_SCL_PARAM,0.f,1.f,0.0f,"RY_CV","%",0,100);
    configParam(N1_SCL_PARAM,0.f,1.f,0.0f,"N1_CV","%",0,100);
    configParam(N2_SCL_PARAM,0.f,1.f,0.0f,"N2_CV","%",0,100);
    configParam(N3_SCL_PARAM,0.f,1.f,0.0f,"N3_CV","%",0,100);
    configParam(A_SCL_PARAM,0.f,1.f,0.0f,"A_CV","%",0,100);
    configParam(B_SCL_PARAM,0.f,1.f,0.0f,"B_CV","%",0,100);

    configParam(ZOOM_PARAM,0.1f,10.f,5.f,"ZOOM");
    configParam(GATE_OSC_PARAM,0.f,1.0f,0.0f,"GATE_OSC");
    configInput(VOCT_INPUT,"V/Oct");
    configInput(GATE_INPUT,"Gate");
    configInput(X_INPUT,"X");
    configInput(Y_INPUT,"Y");
    configInput(RX_INPUT,"Scale X");
    configInput(RY_INPUT,"Scale Y");
    configInput(ROT_INPUT,"Rotation");
    configInput(N1_INPUT,"N1");
    configInput(N2_INPUT,"N2");
    configInput(N3_INPUT,"N3");
    configInput(A_INPUT,"A");
    configInput(B_INPUT,"B");
    configInput(PHS_INPUT,"Ext Phase");
    configOutput(LEFT_OUTPUT,"Left");
    configOutput(RIGHT_OUTPUT,"Right");

    configButton(SIMD_PARAM,"use simd");

    for(int k=0;k<4;k++) {
      osc[k].computer=&computer;
      osc[k].phs=TWOPIF*2;
      oscBw[k].computer=&computer;
      oscBw[k].phs=TWOPIF*2;
    }
    for(int k=0;k<16;k++) {
      fosc[k].computer=&fcomputer;
      fosc[k].phs=TWOPIF*2;
      foscBw[k].computer=&fcomputer;
      foscBw[k].phs=TWOPIF*2;
    }
    paramUpdaterDivider.setDivision(32);
  }

  void onReset(const ResetEvent &e) override {
    Module::onReset(e);
    reset=true;
  }

  void onRandomize(const RandomizeEvent &e) override {
    //INFO("onRandomize");
    params[RX_PARAM].setValue((float)rnd.nextDouble());
    params[RY_PARAM].setValue((float)rnd.nextDouble());

    int cdist[20]={1,2,2,2,2,2,2,2,2,3,3,3,3,3,3,3,4,4,4,4};
    int count=cdist[(int)(rnd.nextDouble()*20)];
    for(int k=0;k<4;k++) { // the first 4 params are the terrain parameters
      if(k<count) {
        params[k].setValue(rnd.nextDouble()*Computer<float>::NUM_TERRAINS);
      } else {
        params[k].setValue(-1);
      }
    }
    params[X_PARAM].setValue(((float)rnd.nextTri(12)-0.5f)*40.f);// approx gauss dist
    params[Y_PARAM].setValue(((float)rnd.nextTri(12)-0.5f)*40.f);// approx gauss dist

    // tell the view it should be centered
    center=true;
  }

  void changed() {
    if(state)
      return;
    if(!(lastParam[0]==genParam[0]&&lastParam[1]==genParam[1]&&lastParam[2]==genParam[2]&&lastParam[3]==genParam[3])) {
      state=true;
      lastParam[0]=genParam[0];
      lastParam[1]=genParam[1];
      lastParam[2]=genParam[2];
      lastParam[3]=genParam[3];
    }
  }


  bool isOn(float_4 gate) {
    return gate[0]>0.f||gate[1]>0.f||gate[2]>0.f||gate[3]>0.f;
    //return true;
  }


  void process(const ProcessArgs &args) override {
    if(reset) {
      reset=false;
      for(int k=0;k<4;k++) {
        osc[k].reset();
        oscBw[k].reset();
      }
      for(int k=0;k<16;k++) {
        outputs[LEFT_OUTPUT].setVoltage(0.f,k);
        outputs[RIGHT_OUTPUT].setVoltage(0.f,k);
      }
    }
    if(paramUpdaterDivider.process()) {
      genParam[0]=floorf(params[G1_PARAM].getValue());
      genParam[1]=floorf(params[G2_PARAM].getValue());
      genParam[2]=floorf(params[G3_PARAM].getValue());
      genParam[3]=floorf(params[G4_PARAM].getValue());
      changed();
      m0=floorf(params[M0_PARAM].getValue());
      m1=floorf(params[M1_PARAM].getValue());
      n1=params[N1_PARAM].getValue();
      n2=params[N2_PARAM].getValue();
      n3=params[N3_PARAM].getValue();
      a=params[A_PARAM].getValue();
      b=params[B_PARAM].getValue();
      cx=xavg.process(params[X_PARAM].getValue());
      cy=yavg.process(params[Y_PARAM].getValue());
      rx=params[RX_PARAM].getValue();
      ry=params[RY_PARAM].getValue();
      rot=params[ROT_PARAM].getValue();
    }
    bool extMode=inputs[PHS_INPUT].isConnected();
    int channels=std::max(extMode?inputs[PHS_INPUT].getChannels():inputs[VOCT_INPUT].getChannels(),1);

    for(int c=0;c<channels;c+=4) {
      auto *oscil=&osc[c/4];
      float_4 gate=inputs[GATE_INPUT].getVoltageSimd<float_4>(c);
      float gateOsc=params[GATE_OSC_PARAM].getValue();
      if(isOn(gate)||gateOsc==0.0f) {
        float_4 voct=inputs[VOCT_INPUT].getVoltageSimd<float_4>(c);

        //float_4 n1In=simd::fabs(inputs[N1_INPUT].getVoltageSimd<float_4>(c))*params[N1_SCL_PARAM].getValue()+n1;
        float_4 n1In=inputs[N1_INPUT].getVoltageSimd<float_4>(c)*params[N1_SCL_PARAM].getValue()+n1;
        n1In=simd::clamp(n1In,0.05f,16.f);
        float_4 n1Ins=simd::ifelse(float_4(params[N1_SGN_PARAM].getValue())>0.f,-1.f*n1In,n1In);

        float_4 aIn=inputs[A_INPUT].getVoltageSimd<float_4>(c)*params[A_SCL_PARAM].getValue()+a;
        aIn=simd::clamp(aIn,0.05f,5.f);

        float_4 bIn=inputs[B_INPUT].getVoltageSimd<float_4>(c)*params[B_SCL_PARAM].getValue()+b;
        bIn=simd::clamp(bIn,0.05f,5.f);

        float_4 n2In=inputs[N2_INPUT].getVoltageSimd<float_4>(c)*params[N2_SCL_PARAM].getValue()+n2;
        n2In=simd::clamp(n2In,-5.f,5.f);
        float_4 n3In=inputs[N3_INPUT].getVoltageSimd<float_4>(c)*params[N3_SCL_PARAM].getValue()+n3;
        n3In=simd::clamp(n3In,-5.f,5.f);


        float_4 xIn=inputs[X_INPUT].getVoltageSimd<float_4>(c)*params[X_SCL_PARAM].getValue()+cx;
        float_4 yIn=inputs[Y_INPUT].getVoltageSimd<float_4>(c)*params[Y_SCL_PARAM].getValue()+cy;
        float_4 rxIn=inputs[RX_INPUT].getVoltageSimd<float_4>(c)*params[RX_SCL_PARAM].getValue()+rx;
        float_4 ryIn=inputs[RY_INPUT].getVoltageSimd<float_4>(c)*params[RY_SCL_PARAM].getValue()+ry;
        float_4 rotIn=inputs[ROT_INPUT].getVoltageSimd<float_4>(c)*params[ROT_SCL_PARAM].getValue()+rot;

        oscil->setPitch(voct);
        if(extMode) {
          oscil->updateExtPhase(inputs[PHS_INPUT].getVoltageSimd<float_4>(c)/5.f,false);
        } else {
          oscil->updatePhase(args.sampleTime,false);
        }
        float_4 outL=oscil->process(genParam,4,xIn,yIn,rxIn,ryIn,rotIn,m0,m1,n1Ins,n2In,n3In,aIn,bIn);
        outputs[LEFT_OUTPUT].setVoltageSimd(outL*5.f,c);

        if(outputs[RIGHT_OUTPUT].isConnected()) {
          auto *oscilBw=&oscBw[c/4];
          oscilBw->setPitch(voct);
          if(extMode) {
            oscilBw->updateExtPhase(inputs[PHS_INPUT].getVoltageSimd<float_4>(c)/5.f,true);
          } else {
            oscilBw->updatePhase(args.sampleTime,true);
          }
          float_4 outR=oscilBw->process(genParam,4,xIn,yIn,rxIn,ryIn,rotIn,m0,m1,n1Ins,n2In,n3In,aIn,bIn);
          outputs[RIGHT_OUTPUT].setVoltageSimd(outR*5.f,c);
        } else {
          outputs[RIGHT_OUTPUT].setVoltageSimd(float_4(0.f),c);
        }
      } else {
        outputs[LEFT_OUTPUT].setVoltageSimd(float_4(0.f),c);
        outputs[RIGHT_OUTPUT].setVoltageSimd(float_4(0.f),c);
      }
    }
    outputs[LEFT_OUTPUT].setChannels(channels);
    outputs[RIGHT_OUTPUT].setChannels(channels);

  }

};

struct SuperTerrainDisplay : TransparentWidget {
  GenImage image;
  Computer<float> computer;
  GeneticSuperTerrain *_module;
  Vec _size;
  int imageHandle=-1;
  float posX=0;
  float posY=0;
  float offX=0;
  float offY=0;
  float startOffX=0;
  float startOffY=0;

  float dragX,dragY;
  float startX,startY;
  bool isDragging=false;
  bool isMoving=false;
  float sx,sy;
  float lastZoom;
  float colors[16][3]={{255,0,  0},
                       {0,  255,0},
                       {0,  0,  255},
                       {255,255,0},
                       {255,0,  255},
                       {0,  255,255},
                       {128,0,  0},
                       {0,  128,0},
                       {128,128,128},
                       {255,128,0},
                       {255,0,  128},
                       {0,  128,255},
                       {128,0,  128},
                       {128,255,0},
                       {128,128,255},
                       {128,255,255}};

  SuperTerrainDisplay(GeneticSuperTerrain *module,Vec size) : image(size) {
    image.computer=&computer;
    _module=module;
    _size=size;
    sx=_size.x/2;
    sy=_size.y/2;

    lastZoom=module->params[GeneticSuperTerrain::ZOOM_PARAM].getValue();
    image.updateImage(module->genParam,-offX-lastZoom,-offY-lastZoom,-offX+lastZoom,-offY+lastZoom);
    paramToCoord();
  }

  void centerView() {
    float px=_module->params[GeneticSuperTerrain::X_PARAM].getValue();
    float py=_module->params[GeneticSuperTerrain::Y_PARAM].getValue();
    offX=-px;
    offY=-py;
    image.updateImage(_module->genParam,-offX-lastZoom,-offY-lastZoom,-offX+lastZoom,-offY+lastZoom);
  }

  void coordToParam() {
    float zoom=_module->params[GeneticSuperTerrain::ZOOM_PARAM].getValue();
    float px=zoom*(posX-sx)/sx;
    float py=zoom*(posY-sy)/sy;
    _module->params[GeneticSuperTerrain::X_PARAM].setValue(px);
    _module->params[GeneticSuperTerrain::Y_PARAM].setValue(py);
  }

  void paramToCoord() {
    float zoom=_module->params[GeneticSuperTerrain::ZOOM_PARAM].getValue();
    float px=_module->params[GeneticSuperTerrain::X_PARAM].getValue();
    float py=_module->params[GeneticSuperTerrain::Y_PARAM].getValue();
    posX=(px/zoom)*sx+sx;
    posY=(py/zoom)*sy+sy;
  }

  void onButton(const event::Button &e) override {
    if(!(e.button==GLFW_MOUSE_BUTTON_LEFT&&(e.mods&RACK_MOD_MASK)==0)) {
      return;
    }
    if(e.action==GLFW_PRESS) {
      float zoom=_module->params[GeneticSuperTerrain::ZOOM_PARAM].getValue();
      e.consume(this);
      float realPosX=posX+(offX/zoom)*sx;
      float realPosY=posY+(offY/zoom)*sy;
      if(e.pos.x>=realPosX-5&&e.pos.x<=realPosX+5&&e.pos.y>=realPosY-5&&e.pos.y<=realPosY+5) {
        startX=e.pos.x;
        startY=e.pos.y;
        isDragging=true;
      } else {
        isMoving=true;
        startOffX=offX;
        startOffY=offY;
        dragX=APP->scene->rack->getMousePos().x;
        dragY=APP->scene->rack->getMousePos().y;
        //printf("START %f %f %f %f %f %f %f %f\n",startOffX, startOffY,offX,offY,posX,posY,e.pos.x,e.pos.y);
      }
    } else {
      isDragging=false;
      isMoving=false;
    }
  }

  void onDragStart(const event::DragStart &e) override {
    if(isDragging||isMoving) {
      dragX=APP->scene->rack->getMousePos().x;
      dragY=APP->scene->rack->getMousePos().y;
    }
  }

  void onDragMove(const event::DragMove &e) override {
    float newDragX=APP->scene->rack->getMousePos().x;
    float newDragY=APP->scene->rack->getMousePos().y;
    float zoom=_module->params[GeneticSuperTerrain::ZOOM_PARAM].getValue();
    if(isDragging) {
      posX=startX+(newDragX-dragX)-(offX/zoom)*sx;
      posY=startY+(newDragY-dragY)-(offY/zoom)*sy;
      coordToParam();
    }
    if(isMoving) {
      float mX=(newDragX-dragX);
      float mY=(newDragY-dragY);
      offX=(zoom*mX/sx)+startOffX;
      offY=(zoom*mY/sy)+startOffY;
      //printf("MOVE %f %f %f %f %f %f\n",mX, mY,offX,offY,posX,posY);
      image.updateImage(_module->genParam,-offX-zoom,-offY-zoom,-offX+zoom,-offY+zoom);

    }
  }

  void onDragEnd(const event::DragEnd &e) override {
    isDragging=false;
    isMoving=false;
  }

  void drawCurve(const DrawArgs &args,int channel=-1) {
    float n1=_module->params[GeneticSuperTerrain::N1_PARAM].getValue();
    float n2=_module->params[GeneticSuperTerrain::N2_PARAM].getValue();
    float n3=_module->params[GeneticSuperTerrain::N3_PARAM].getValue();
    float ap=_module->params[GeneticSuperTerrain::A_PARAM].getValue();
    float bp=_module->params[GeneticSuperTerrain::B_PARAM].getValue();
    float m0=_module->params[GeneticSuperTerrain::M0_PARAM].getValue();
    float m1=_module->params[GeneticSuperTerrain::M1_PARAM].getValue();
    float rx=_module->params[GeneticSuperTerrain::RX_PARAM].getValue();
    float ry=_module->params[GeneticSuperTerrain::RY_PARAM].getValue();
    float rot=_module->params[GeneticSuperTerrain::ROT_PARAM].getValue();
    float zoom=_module->params[GeneticSuperTerrain::ZOOM_PARAM].getValue();
    float xc=0;
    float yc=0;
    float cx=0;
    float cy=0;
    if(channel>=0) {
      n1+=_module->inputs[GeneticSuperTerrain::N1_INPUT].getVoltage(channel)*_module->params[GeneticSuperTerrain::N1_SCL_PARAM].getValue();
      n1=simd::clamp(n1,0.05f,16.f);
      n1=_module->params[GeneticSuperTerrain::N1_SGN_PARAM].getValue()>0.f?-n1:n1;

      n2+=_module->inputs[GeneticSuperTerrain::N2_INPUT].getVoltage(channel)*_module->params[GeneticSuperTerrain::N2_SCL_PARAM].getValue();
      n2=simd::clamp(n2,-5.f,5.f);
      n3+=_module->inputs[GeneticSuperTerrain::N3_INPUT].getVoltage(channel)*_module->params[GeneticSuperTerrain::N3_SCL_PARAM].getValue();
      n3=simd::clamp(n3,-5.f,5.f);

      ap+=_module->inputs[GeneticSuperTerrain::A_INPUT].getVoltage(channel)*_module->params[GeneticSuperTerrain::A_SCL_PARAM].getValue();
      ap=simd::clamp(ap,0.05f,5.f);

      bp+=_module->inputs[GeneticSuperTerrain::B_INPUT].getVoltage(channel)*_module->params[GeneticSuperTerrain::B_SCL_PARAM].getValue();
      bp=simd::clamp(bp,0.05f,5.f);

      cx=_module->inputs[GeneticSuperTerrain::X_INPUT].getVoltage(channel)*_module->params[GeneticSuperTerrain::X_SCL_PARAM].getValue()+cx;
      cy=_module->inputs[GeneticSuperTerrain::Y_INPUT].getVoltage(channel)*_module->params[GeneticSuperTerrain::Y_SCL_PARAM].getValue()+cy;
      rx=_module->inputs[GeneticSuperTerrain::RX_INPUT].getVoltage(channel)*_module->params[GeneticSuperTerrain::RX_SCL_PARAM].getValue()+rx;
      ry=_module->inputs[GeneticSuperTerrain::RY_INPUT].getVoltage(channel)*_module->params[GeneticSuperTerrain::RY_SCL_PARAM].getValue()+ry;
      rot=_module->inputs[GeneticSuperTerrain::ROT_INPUT].getVoltage(channel)*_module->params[GeneticSuperTerrain::ROT_SCL_PARAM].getValue()+rot;
    } else {
      n1=_module->params[GeneticSuperTerrain::N1_SGN_PARAM].getValue()>0.f?-n1:n1;
    }
    float delta=0.01;
    float len=4*M_PI;
    float t=0;
    int k=0;
    float oX=(offX/zoom)*sx;
    float oY=(offY/zoom)*sy;
    NVGcolor curveColor=channel==-1?nvgRGB(255,255,255):nvgRGB(colors[channel][0],colors[channel][1],colors[channel][2]);
    nvgStrokeColor(args.vg,curveColor);
    nvgBeginPath(args.vg);
    nvgStrokeWidth(args.vg,std::max(5.f/zoom,1.f));
    while(t<len) {
      computer.superformula(t,cx,cy,rx,ry,m0,m1,n1,n2,n3,ap,bp,xc,yc);
      computer.rotate_point(cx,cy,rot,xc,yc);
      xc=(sx/zoom)*xc;
      yc=(sy/zoom)*yc;
      if(k==0)
        nvgMoveTo(args.vg,xc+oX+posX,yc+oY+posY);
      else
        nvgLineTo(args.vg,xc+oX+posX,yc+oY+posY);

      k++;
      t+=delta;
    }
    nvgStroke(args.vg);
  }

  void drawLayer(const DrawArgs &args,int layer) override {
    if(layer==1) {
      _draw(args);
    }
    Widget::drawLayer(args,layer);
  }

  void _draw(const DrawArgs &args) {
    if(_module->center) {
      centerView();
      _module->center=false;
    }
    nvgScissor(args.vg,0,0,_size.x,_size.y);
    float zoom=_module->params[GeneticSuperTerrain::ZOOM_PARAM].getValue();
    if(_module->state) {
      image.updateImage(_module->genParam,-offX-zoom,-offY-zoom,-offX+zoom,-offY+zoom);
      _module->state=false;
    }
    if(zoom!=lastZoom) {
      image.updateImage(_module->genParam,-offX-zoom,-offY-zoom,-offX+zoom,-offY+zoom);
      lastZoom=zoom;
      paramToCoord();
    }
    if(imageHandle==-1) {
      imageHandle=nvgCreateImageRGBA(args.vg,_size.x,_size.y,0,image.data);
    } else {
      nvgUpdateImage(args.vg,imageHandle,image.data);
    }

    NVGpaint imgPaint=nvgImagePattern(args.vg,0,0,_size.x,_size.y,0,imageHandle,1);
    nvgBeginPath(args.vg);
    nvgRect(args.vg,0,0,_size.x,_size.y);
    nvgFillPaint(args.vg,imgPaint);
    nvgFill(args.vg);

    NVGcolor ballColor=nvgRGB(25,150,252);
    nvgStrokeColor(args.vg,ballColor);

    float oX=(offX/zoom)*sx;
    float oY=(offY/zoom)*sy;

    nvgFillColor(args.vg,ballColor);
    nvgBeginPath(args.vg);
    nvgCircle(args.vg,posX+oX,posY+oY,20/zoom);
    nvgFill(args.vg);
    drawCurve(args);
    for(int k=0;k<16;k++) {
      if(_module->inputs[GeneticSuperTerrain::GATE_INPUT].getVoltage(k)>0) {
        drawCurve(args,k);
      }
    }
  }
};

struct GeneticSuperTerrainWidget : ModuleWidget {
  GeneticSuperTerrainWidget(GeneticSuperTerrain *module) {
    setModule(module);
    setPanel(APP->window->loadSvg(asset::plugin(pluginInstance,"res/GeneticSuperTerrain.svg")));

    addChild(createWidget<ScrewSilver>(Vec(RACK_GRID_WIDTH,0)));
    addChild(createWidget<ScrewSilver>(Vec(box.size.x-2*RACK_GRID_WIDTH,0)));
    addChild(createWidget<ScrewSilver>(Vec(RACK_GRID_WIDTH,RACK_GRID_HEIGHT-RACK_GRID_WIDTH)));
    addChild(createWidget<ScrewSilver>(Vec(box.size.x-2*RACK_GRID_WIDTH,RACK_GRID_HEIGHT-RACK_GRID_WIDTH)));

    if(module!=nullptr) {
      auto display=new SuperTerrainDisplay(module,Vec(360,360));
      display->box.pos=mm2px(Vec(15,MHEIGHT-121.92f-2.5f));
      display->box.size=Vec(360,360);
      addChild(display);
      auto cb=createParam<CenterButton<SuperTerrainDisplay>>(mm2px(Vec(141,MHEIGHT-22.f-3.2f)),module,GeneticSuperTerrain::CENTER_PARAM);
      cb->module=display;
      addChild(cb);
      auto xparam=createParam<XYKnob<SuperTerrainDisplay>>(mm2px(Vec(140,MHEIGHT-78.f-9.f)),module,GeneticSuperTerrain::X_PARAM);
      xparam->td=display;
      addParam(xparam);
      auto yparam=createParam<XYKnob<SuperTerrainDisplay>>(mm2px(Vec(140,MHEIGHT-47.5f-9.f)),module,GeneticSuperTerrain::Y_PARAM);
      yparam->td=display;
      addParam(yparam);
    }
    addParam(createParam<TrimbotWhite9Snap>(mm2px(Vec(3,MHEIGHT-108.f-9.f)),module,GeneticSuperTerrain::G1_PARAM));
    addParam(createParam<TrimbotWhite9Snap>(mm2px(Vec(3,MHEIGHT-93.f-9.f)),module,GeneticSuperTerrain::G2_PARAM));
    addParam(createParam<TrimbotWhite9Snap>(mm2px(Vec(3,MHEIGHT-78.f-9.f)),module,GeneticSuperTerrain::G3_PARAM));
    addParam(createParam<TrimbotWhite9Snap>(mm2px(Vec(3,MHEIGHT-63.f-9.f)),module,GeneticSuperTerrain::G4_PARAM));
    addInput(createInput<SmallPort>(mm2px(Vec(4.350,MHEIGHT-52.f-6.287f)),module,GeneticSuperTerrain::GATE_INPUT));
    addParam(createParam<SmallRoundButton>(mm2px(Vec(11,MHEIGHT-51.f-6.287f)),module,GeneticSuperTerrain::GATE_OSC_PARAM));
    addInput(createInput<SmallPort>(mm2px(Vec(4.350,MHEIGHT-41.5f-6.287f)),module,GeneticSuperTerrain::VOCT_INPUT));
    addInput(createInput<SmallPort>(mm2px(Vec(4.350,MHEIGHT-31.0f-6.287f)),module,GeneticSuperTerrain::PHS_INPUT));
    addOutput(createOutput<SmallPort>(mm2px(Vec(4.350,MHEIGHT-20.5f-6.287f)),module,GeneticSuperTerrain::LEFT_OUTPUT));
    addOutput(createOutput<SmallPort>(mm2px(Vec(4.350,MHEIGHT-10.0f-6.287f)),module,GeneticSuperTerrain::RIGHT_OUTPUT));


    addParam(createParam<TrimbotWhite9>(mm2px(Vec(140,MHEIGHT-7.751f-9.f)),module,GeneticSuperTerrain::ZOOM_PARAM));

    addParam(createParam<TrimbotWhite9Snap>(mm2px(Vec(140,MHEIGHT-106.f-9.f)),module,GeneticSuperTerrain::M0_PARAM));
    addParam(createParam<TrimbotWhite9Snap>(mm2px(Vec(140,MHEIGHT-92.f-9.f)),module,GeneticSuperTerrain::M1_PARAM));

    addInput(createInput<SmallPort>(mm2px(Vec(141.5f,MHEIGHT-69.5f-6.287f)),module,GeneticSuperTerrain::X_INPUT));
    addParam(createParam<TrimbotWhite>(mm2px(Vec(141.5f,MHEIGHT-61.f-6.371f)),module,GeneticSuperTerrain::X_SCL_PARAM));

    addInput(createInput<SmallPort>(mm2px(Vec(141.5f,MHEIGHT-39.f-6.287f)),module,GeneticSuperTerrain::Y_INPUT));
    addParam(createParam<TrimbotWhite>(mm2px(Vec(141.5f,MHEIGHT-30.5f-6.371f)),module,GeneticSuperTerrain::Y_SCL_PARAM));

    addParam(createParam<TrimbotWhite9>(mm2px(Vec(156,MHEIGHT-106.f-9.f)),module,GeneticSuperTerrain::RX_PARAM));
    addInput(createInput<SmallPort>(mm2px(Vec(169.33f,MHEIGHT-107.5f-6.287f)),module,GeneticSuperTerrain::RX_INPUT));
    addParam(createParam<TrimbotWhite>(mm2px(Vec(180.f,MHEIGHT-107.5f-6.371f)),module,GeneticSuperTerrain::RX_SCL_PARAM));

    addParam(createParam<TrimbotWhite9>(mm2px(Vec(156,MHEIGHT-92.f-9.f)),module,GeneticSuperTerrain::RY_PARAM));
    addInput(createInput<SmallPort>(mm2px(Vec(169.33f,MHEIGHT-93.5f-6.287f)),module,GeneticSuperTerrain::RY_INPUT));
    addParam(createParam<TrimbotWhite>(mm2px(Vec(180.f,MHEIGHT-93.5f-6.371f)),module,GeneticSuperTerrain::RY_SCL_PARAM));

    addParam(createParam<TrimbotWhite9>(mm2px(Vec(156,MHEIGHT-78.f-9.f)),module,GeneticSuperTerrain::ROT_PARAM));
    addInput(createInput<SmallPort>(mm2px(Vec(169.33f,MHEIGHT-79.5f-6.287f)),module,GeneticSuperTerrain::ROT_INPUT));
    addParam(createParam<TrimbotWhite>(mm2px(Vec(180.f,MHEIGHT-79.5f-6.371f)),module,GeneticSuperTerrain::ROT_SCL_PARAM));

    addParam(createParam<TrimbotWhite9>(mm2px(Vec(156,MHEIGHT-64.f-9.f)),module,GeneticSuperTerrain::N1_PARAM));
    addParam(createParam<SmallRoundButton>(mm2px(Vec(166.f,MHEIGHT-74.f)),module,GeneticSuperTerrain::N1_SGN_PARAM));
    addInput(createInput<SmallPort>(mm2px(Vec(169.33f,MHEIGHT-65.5f-6.287f)),module,GeneticSuperTerrain::N1_INPUT));
    addParam(createParam<TrimbotWhite>(mm2px(Vec(180.f,MHEIGHT-65.5f-6.371f)),module,GeneticSuperTerrain::N1_SCL_PARAM));

    addParam(createParam<TrimbotWhite9>(mm2px(Vec(156,MHEIGHT-50.f-9.f)),module,GeneticSuperTerrain::N2_PARAM));
    addInput(createInput<SmallPort>(mm2px(Vec(169.33f,MHEIGHT-51.5f-6.287f)),module,GeneticSuperTerrain::N2_INPUT));
    addParam(createParam<TrimbotWhite>(mm2px(Vec(180.f,MHEIGHT-51.5f-6.371f)),module,GeneticSuperTerrain::N2_SCL_PARAM));

    addParam(createParam<TrimbotWhite9>(mm2px(Vec(156,MHEIGHT-36.f-9.f)),module,GeneticSuperTerrain::N3_PARAM));
    addInput(createInput<SmallPort>(mm2px(Vec(169.33f,MHEIGHT-37.5f-6.287f)),module,GeneticSuperTerrain::N3_INPUT));
    addParam(createParam<TrimbotWhite>(mm2px(Vec(180.f,MHEIGHT-37.5f-6.371f)),module,GeneticSuperTerrain::N3_SCL_PARAM));

    addParam(createParam<TrimbotWhite9>(mm2px(Vec(156,MHEIGHT-22.f-9.f)),module,GeneticSuperTerrain::A_PARAM));
    addInput(createInput<SmallPort>(mm2px(Vec(169.33f,MHEIGHT-23.5f-6.287f)),module,GeneticSuperTerrain::A_INPUT));
    addParam(createParam<TrimbotWhite>(mm2px(Vec(180.f,MHEIGHT-23.5f-6.371f)),module,GeneticSuperTerrain::A_SCL_PARAM));

    addParam(createParam<TrimbotWhite9>(mm2px(Vec(156,MHEIGHT-8.f-9.f)),module,GeneticSuperTerrain::B_PARAM));
    addInput(createInput<SmallPort>(mm2px(Vec(169.33f,MHEIGHT-9.5f-6.287f)),module,GeneticSuperTerrain::B_INPUT));
    addParam(createParam<TrimbotWhite>(mm2px(Vec(180.f,MHEIGHT-9.5f-6.371f)),module,GeneticSuperTerrain::B_SCL_PARAM));

    //addParam(createParam<SmallRoundButton>(mm2px(Vec(149.f,MHEIGHT-22.f-3.2f)),module,GeneticSuperTerrain::SIMD_PARAM));


  }
};

Model *modelGeneticSuperTerrain=createModel<GeneticSuperTerrain,GeneticSuperTerrainWidget>("GeneticSuperTerrain");
