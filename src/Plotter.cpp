#include "dcb.h"

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
    bufsize=params[DIM_PARAM].getValue();
    int channels = inputs[X_INPUT].getChannels();
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
};

struct PlotterDisplay : TransparentWidget {
  Plotter *module;

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
      Vec center={box.pos.x+box.size.x/2,box.pos.y+box.size.y/2};
      nvgStrokeWidth(args.vg,2.f);
      if(module) {
        int channels = module->inputs[Plotter::X_INPUT].getChannels();
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
    auto display=new PlotterDisplay();
    display->module=module;
    display->box.pos=mm2px(Vec(7.f,MHEIGHT-121.92f-5.f));
    display->box.size=Vec(360,360);
    addChild(display);
    addInput(createInput<SmallPort>(mm2px(Vec(5,MHEIGHT-104.f)),module,Plotter::X_INPUT));
    addInput(createInput<SmallPort>(mm2px(Vec(5,MHEIGHT-92.f)),module,Plotter::Y_INPUT));
    addParam(createParam<TrimbotWhite>(mm2px(Vec(5,MHEIGHT-80.f)),module,Plotter::SCALE_PARAM));
    addParam(createParam<TrimbotWhite>(mm2px(Vec(5,MHEIGHT-68.f)),module,Plotter::DIM_PARAM));
  }
};


Model *modelPlotter=createModel<Plotter,PlotterWidget>("Plotter");