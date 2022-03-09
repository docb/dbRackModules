#include "dcb.h"
#include "Computer.hpp"
#define MAXBUFSIZE 256

struct Plotter : Module {
  enum ParamId {
    SCALE_PARAM,DIM_PARAM,PARAMS_LEN
  };
  enum InputId {
    X_INPUT,Y_INPUT,INPUTS_LEN
  };
  enum OutputId {
    OUTPUTS_LEN
  };
  enum LightId {
    LIGHTS_LEN
  };
  float frameRate=60.f;
  int samplesPerFrame;
  int sampleCount=0;
  float bufX[16][MAXBUFSIZE]={};
  float bufY[16][MAXBUFSIZE]={};
  int bufIdx=0;
  int bufsize=MAXBUFSIZE;
  Module *superLFO = nullptr;

  Plotter() {
    config(PARAMS_LEN,INPUTS_LEN,OUTPUTS_LEN,LIGHTS_LEN);
    configParam(SCALE_PARAM,0.5f,4.f,1.f,"Scale");
    configParam(DIM_PARAM,2.f,255.f,255.f,"Dimmer");
    configInput(X_INPUT,"X");
    configInput(Y_INPUT,"Y");
  }

  void onAdd(const AddEvent &e) override {
    samplesPerFrame=(1.f/frameRate)*APP->engine->getSampleRate();
  }

  void onSampleRateChange(const SampleRateChangeEvent &e) override {
    samplesPerFrame=(1.f/frameRate)*APP->engine->getSampleRate();
  }

  void process(const ProcessArgs &args) override {
    if(leftExpander.module && leftExpander.module->model->slug == "SuperLFO") {
        superLFO = leftExpander.module;
    } else {
      superLFO =nullptr;
      bufsize=params[DIM_PARAM].getValue();
      int channels=inputs[X_INPUT].getChannels();
      if(sampleCount>=samplesPerFrame) {
        sampleCount=0;
        if(bufIdx>=bufsize)
          bufIdx=0;
        for(int c=0;c<channels;c++) {
          bufX[c][bufIdx]=inputs[X_INPUT].getVoltage(c)*10.f*params[SCALE_PARAM].getValue();
          bufY[c][bufIdx]=inputs[Y_INPUT].getPolyVoltage(c)*10.f*params[SCALE_PARAM].getValue();
        }
        bufIdx++;
      }
      sampleCount++;
    }
  }
};


struct PlotterDisplay : TransparentWidget {
  Plotter *module;
  Vec center;
  PlotterDisplay(Vec pos, Vec size) : TransparentWidget() {
    box.size = size;
    box.pos = pos;
    center={box.pos.x+box.size.x/2,box.pos.y+box.size.y/2};
  }
  Computer<float> computer;
  enum SuperLFOParamIds {
    FREQ_PARAM,RX_PARAM,RY_PARAM,ROT_PARAM,M0_PARAM,M1_PARAM,N1_PARAM,N1_SGN_PARAM,N2_PARAM,N3_PARAM,A_PARAM,B_PARAM,X_SCL_PARAM,Y_SCL_PARAM,RX_SCL_PARAM,RY_SCL_PARAM,ROT_SCL_PARAM,N1_SCL_PARAM,N2_SCL_PARAM,N3_SCL_PARAM,A_SCL_PARAM,B_SCL_PARAM,PARAMS_LEN
  };

  void drawCurve(const DrawArgs &args) {
    float n1=module->superLFO->params[N1_PARAM].getValue();
    float n2=module->superLFO->params[N2_PARAM].getValue();
    float n3=module->superLFO->params[N3_PARAM].getValue();
    float ap=module->superLFO->params[A_PARAM].getValue();
    float bp=module->superLFO->params[B_PARAM].getValue();
    float m0=module->superLFO->params[M0_PARAM].getValue();
    float m1=module->superLFO->params[M1_PARAM].getValue();
    float rx=module->superLFO->params[RX_PARAM].getValue();
    float ry=module->superLFO->params[RY_PARAM].getValue();
    float rot=module->superLFO->params[ROT_PARAM].getValue();

    float xc=0;
    float yc=0;
    float cx=0;
    float cy=0;

    n1=module->superLFO->params[N1_SGN_PARAM].getValue()>0.f?-n1:n1;
    float delta=0.01;
    float len=4*M_PI;
    float t=0;
    int k=0;
    //float oX=(offX/zoom)*sx;
    //float oY=(offY/zoom)*sy;
    NVGcolor curveColor=nvgRGB(88,255,88);
    nvgStrokeColor(args.vg,curveColor);
    nvgBeginPath(args.vg);
    //nvgStrokeWidth(args.vg,std::max(5.f/zoom,1.f));
    nvgStrokeWidth(args.vg,2.f);
    while(t<len) {
      computer.superformula(t,cx,cy,rx,ry,m0,m1,n1,n2,n3,ap,bp,xc,yc);
      computer.rotate_point(cx,cy,rot,xc,yc);
      xc=module->params[Plotter::SCALE_PARAM].getValue()*xc*50+center.x;
      yc=module->params[Plotter::SCALE_PARAM].getValue()*yc*50+center.y;
      if(k==0)
        nvgMoveTo(args.vg,xc,yc);
      else
        nvgLineTo(args.vg,xc,yc);

      k++;
      t+=delta;
    }
    nvgStroke(args.vg);
  }

  void drawLine(const DrawArgs &args,float x0,float y0,float x1,float y1,NVGcolor color) {
    nvgStrokeColor(args.vg,color);
    nvgBeginPath(args.vg);
    nvgMoveTo(args.vg,x0,y0);
    nvgLineTo(args.vg,x1,y1);
    nvgStroke(args.vg);
  }

  void drawLayer(const DrawArgs &args,int layer) override {
    if(layer==1) {

      nvgScissor(args.vg,box.pos.x,box.pos.y,box.size.x,box.size.y);

      nvgStrokeWidth(args.vg,2.f);
      if(module) {
        if(module->superLFO) {
          drawCurve(args);
        } else {
          int channels=module->inputs[Plotter::X_INPUT].getChannels();
          for(int c=0;c<channels;c++) {
            for(int k=1;k<module->bufsize-1;k++) {
              int j=(module->bufIdx+k);
              float r=float(k)/float(module->bufsize);
              int alpha=int(r*r*r*r*255.f);
              drawLine(args,center.x+module->bufX[c][j%module->bufsize],center.y-module->bufY[c][j%module->bufsize],center.x+module->bufX[c][(j+1)%module->bufsize],center.y-module->bufY[c][(j+1)%module->bufsize],nvgRGBA(0x22,0xcc,0x22,alpha));
            }
          }
        }
      }
    }
    Widget::drawLayer(args,layer);
  }
};

struct PlotterWidget : ModuleWidget {
  PlotterWidget(Plotter *module) {
    setModule(module);
    setPanel(createPanel(asset::plugin(pluginInstance,"res/Plotter.svg")));

    addChild(createWidget<ScrewSilver>(Vec(RACK_GRID_WIDTH,0)));
    addChild(createWidget<ScrewSilver>(Vec(box.size.x-2*RACK_GRID_WIDTH,0)));
    addChild(createWidget<ScrewSilver>(Vec(RACK_GRID_WIDTH,RACK_GRID_HEIGHT-RACK_GRID_WIDTH)));
    addChild(createWidget<ScrewSilver>(Vec(box.size.x-2*RACK_GRID_WIDTH,RACK_GRID_HEIGHT-RACK_GRID_WIDTH)));
    auto display=new PlotterDisplay(mm2px(Vec(7.f,MHEIGHT-121.92f-5.f)),Vec(360,360));
    display->module=module;
    //display->box.pos=mm2px(Vec(7.f,MHEIGHT-121.92f-5.f));
    //display->box.size=Vec(360,360);
    addChild(display);
    addInput(createInput<SmallPort>(mm2px(Vec(5,MHEIGHT-104.f)),module,Plotter::X_INPUT));
    addInput(createInput<SmallPort>(mm2px(Vec(5,MHEIGHT-92.f)),module,Plotter::Y_INPUT));
    addParam(createParam<TrimbotWhite>(mm2px(Vec(5,MHEIGHT-80.f)),module,Plotter::SCALE_PARAM));
    addParam(createParam<TrimbotWhite>(mm2px(Vec(5,MHEIGHT-68.f)),module,Plotter::DIM_PARAM));
  }
};


Model *modelPlotter=createModel<Plotter,PlotterWidget>("Plotter");