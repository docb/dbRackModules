#include "dcb.h"
/**
 * This module uses the NEWS algorithm from the QWelk plugin
 * https://github.com/raincheque/qwelk
 * which is licenced under the following statement:
 *
 *
Copyright 2017 Parsa 'raincheque' Jamshidi

Permission is hereby granted, free of charge, to any person obtaining a copy of this
software and associated documentation files (the "Software"), to deal in the Software without restriction,
including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:
The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.
THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED,
INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM,
DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,
ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 */

#define GWIDTH          4
#define GHEIGHT         8
#define GSIZE           (GWIDTH*GHEIGHT)
#define GMID            (GWIDTH/2+(GHEIGHT/2)*GWIDTH)
#define LIGHT_SIZE      6
#define DIR_BIT_SIZE    8

uint8_t minb(uint8_t a,uint8_t b) {
  return a<b?a:b;
}

uint8_t maxb(uint8_t a,uint8_t b) {
  return a>b?a:b;
}

int clampi(int v,int l,int h) {
  return (v<l)?l:((v>h)?h:v);
}

float slew(float v,float i,float sa,float min,float max,float shape) {
  float ret=v;

  if(i>v) {
    float s=max*powf(min/max,sa);
    ret+=s*crossfade(1.0,(1/10.0)*(i-v),shape)/APP->engine->getSampleRate();
    if(ret>i)
      ret=i;
  } else if(i<v) {
    float s=max*powf(min/max,sa);
    ret-=s*crossfade(1.0,(1/10.0)*(v-i),shape)/APP->engine->getSampleRate();
    if(ret<i)
      ret=i;
  }

  return ret;
}

struct Swen : Module {
  enum ParamIds {
    PARAM_MODE,PARAM_GATEMODE,PARAM_ROUND,PARAM_CLAMP,PARAM_INTENSITY,PARAM_WRAP,PARAM_SMOOTH,PARAM_UNI_BI,PARAM_ORIGIN,NUM_PARAMS
  };
  enum InputIds {
    IN_NEWS,IN_INTENSITY,IN_WRAP,IN_HOLD,IN_ORIGIN,NUM_INPUTS
  };
  enum OutputIds {
    OUT_CELL,NUM_OUTPUTS=OUT_CELL+GSIZE
  };
  enum LightIds {
    LIGHT_GRID,NUM_LIGHTS=LIGHT_GRID+GSIZE
  };

  float sample;
  dsp::SchmittTrigger trig_hold;
  uint8_t grid[GSIZE]{};
  float buffer[GSIZE]{};

  Swen() {
    config(NUM_PARAMS,NUM_INPUTS,NUM_OUTPUTS,NUM_LIGHTS);
    configParam(PARAM_ORIGIN,0.0,GSIZE,GMID,"Origin");
    getParamQuantity(PARAM_ORIGIN)->snapEnabled=true;
    configParam(PARAM_INTENSITY,1,256.0,1.0,"Scale");
    configParam(PARAM_WRAP,-31.0,32.0,0.0,"Wrap");
    configButton(PARAM_UNI_BI,"UNI");
    getParamQuantity(PARAM_UNI_BI)->setValue(1);
    configButton(PARAM_MODE,"Mode");
    getParamQuantity(PARAM_MODE)->setValue(1);
    configButton(PARAM_GATEMODE,"GateMode");
    getParamQuantity(PARAM_GATEMODE)->setValue(1);
    configButton(PARAM_ROUND,"Round");
    getParamQuantity(PARAM_ROUND)->setValue(1);
    configButton(PARAM_CLAMP,"Clamp");
    getParamQuantity(PARAM_CLAMP)->setValue(1);
    configParam(PARAM_SMOOTH,0.0,1.0,0.0,"Smooth");
    for(int y=0;y<GHEIGHT;++y) {
      for(int x=0;x<GWIDTH;++x) {
        int i=x+y*GWIDTH;
        std::string nr=std::to_string(i);
        configOutput(OUT_CELL+i,nr);
      }
    }
    configInput(IN_HOLD,"Clock");
    configInput(IN_NEWS,"In");
    configInput(IN_INTENSITY,"Scale");
    configInput(IN_ORIGIN,"Origin");
    configInput(IN_WRAP,"Wrap");

  }

  void process(const ProcessArgs &args) {
    bool mode=params[PARAM_MODE].getValue()>0.0;
    bool gatemode=params[PARAM_GATEMODE].getValue()>0.0;
    bool round=params[PARAM_ROUND].getValue()==0.0;
    bool clamp=params[PARAM_CLAMP].getValue()==0.0;
    bool bi=params[PARAM_UNI_BI].getValue()==0.0;
    uint8_t intensity=(uint8_t)(floor(params[PARAM_INTENSITY].getValue()));
    int wrap=floor(params[PARAM_WRAP].getValue());
    int origin=floor(params[PARAM_ORIGIN].getValue());
    float smooth=params[PARAM_SMOOTH].getValue();

    float in_origin=inputs[IN_ORIGIN].getVoltage()/10.0;
    float in_intensity=(inputs[IN_INTENSITY].getVoltage()/10.0)*255.0;
    float in_wrap=(inputs[IN_WRAP].getVoltage()/5.0)*31.0;


    intensity=minb(intensity+(uint8_t)in_intensity,255.0);
    wrap=::clampi(wrap+(int)in_wrap,-31,31);

    // are we doing s&h?
    if(trig_hold.process(inputs[IN_HOLD].getVoltage()))
      sample=inputs[IN_NEWS].getVoltage();

    // read the news, or if s&h is active just the held sample
    float news=(inputs[IN_HOLD].isConnected())?sample:inputs[IN_NEWS].getVoltage();

    // if round switch is down, round off the  input signal to an integer
    if(round)
      news=ceil(news);

    // wrap the bits around, e.g. wrap = 2, 1001 ->  0110 / wrap = -3,  1001 -> 0011
    uint32_t bits=*(reinterpret_cast<uint32_t *>(&news));
    if(wrap>0)
      bits=(bits<<wrap)|(bits>>(32-wrap));
    else if(wrap<0) {
      wrap=-wrap;
      bits=(bits>>wrap)|(bits<<(32-wrap));
    }

    news=*((float *)&bits);

    // extract the key out the bits which represent the input signal
    uint32_t key=*(reinterpret_cast<uint32_t *>(&news));

    // reset grid
    for(int i=0;i<GSIZE;++i)
      grid[i]=0;

    // determine origin
    origin=std::min(origin+(int)floor(in_origin*GSIZE),GSIZE);
    int cy=origin/GWIDTH,cx=origin%GWIDTH;

    // extract N-E-W-S steps
    int nort=(key>>24)&0xFF,east=(key>>16)&0xFF,sout=(key>>8)&0xFF,west=(key)&0xFF;

    // begin plotting, or 'the walk'
    int w=0,ic=(mode?1:DIR_BIT_SIZE),cond=0;
    while(w++<ic) {
      cond=mode?(nort):(((nort>>w)&1)==1);
      while(cond-->0) {
        cy=(cy-1)>=0?cy-1:GHEIGHT-1;
        set(cx,cy,gatemode);
      }
      cond=(mode?(east):(((east>>w)&1)==1));
      while(cond-->0) {
        cx=(cx+1)<GWIDTH?cx+1:0;
        set(cx,cy,gatemode);
      }
      cond=(mode?(sout):(((sout>>w)&1)==1));
      while(cond-->0) {
        cy=(cy+1)<GHEIGHT?cy+1:0;
        set(cx,cy,gatemode);
      }
      cond=(mode?(west):(((west>>w)&1)==1));
      while(cond-->0) {
        cx=(cx-1)>=0?cx-1:GWIDTH-1;
        set(cx,cy,gatemode);
      }
    }

    // output
    for(int i=0;i<GSIZE;++i) {
      uint8_t r=grid[i]*intensity;
      if(clamp&&((int)grid[i]*(int)intensity)>0xFF)
        r=0xFF;

      float v=gatemode?(grid[i]?1:0):((uint8_t)r/255.0);

      buffer[i]=slew(buffer[i],v,smooth,0.1,100000.0,0.5);

      outputs[OUT_CELL+i].setVoltage(10.0*buffer[i]-(bi?5.0:0.0));
      lights[LIGHT_GRID+i].setBrightness(buffer[i]*0.9);
    }
  }

  inline void set(int i,bool gatemode) {
    if(gatemode)
      grid[i]^=1;
    else
      grid[i]+=1;
  }

  inline void set(int x,int y,bool gatemode) {
    set(x+y*GWIDTH,gatemode);
  }
};


//TODO: move to common
template<typename _BASE>
struct CellLight : _BASE {
  CellLight() {
    this->box.size=mm2px(Vec(LIGHT_SIZE,LIGHT_SIZE));
  }
};

struct SwenWidget : ModuleWidget {
  SwenWidget(Swen *module) {
    setModule(module);

    setPanel(APP->window->loadSvg(asset::plugin(pluginInstance,"res/Swen.svg")));

    addParam(createParam<TrimbotWhite>(mm2px(Vec(3,11)),module,Swen::PARAM_ORIGIN));
    addInput(createInput<SmallPort>(mm2px(Vec(3,19)),module,Swen::IN_ORIGIN));
    addParam(createParam<TrimbotWhite>(mm2px(Vec(15,11)),module,Swen::PARAM_INTENSITY));
    addInput(createInput<SmallPort>(mm2px(Vec(15,19)),module,Swen::IN_INTENSITY));
    addParam(createParam<TrimbotWhite>(mm2px(Vec(27,11)),module,Swen::PARAM_WRAP));
    addInput(createInput<SmallPort>(mm2px(Vec(27,19)),module,Swen::IN_WRAP));

    addInput(createInput<SmallPort>(mm2px(Vec(7,32)),module,Swen::IN_HOLD));
    addInput(createInput<SmallPort>(mm2px(Vec(7,44)),module,Swen::IN_NEWS));
    addParam(createParam<TrimbotWhite>(mm2px(Vec(7,56)),module,Swen::PARAM_SMOOTH));

    float x=22;
    float y=35;
    float dy=6;
    auto button = createParam<SmallButtonWithLabel>(mm2px(Vec(x,y)),module,Swen::PARAM_UNI_BI);
    button->setLabel("Uni");
    addParam(button);
    y+=dy;
    button=createParam<SmallButtonWithLabel>(mm2px(Vec(x,y)),module,Swen::PARAM_MODE);
    button->setLabel("Mode");
    addParam(button);
    y+=dy;
    button=createParam<SmallButtonWithLabel>(mm2px(Vec(x,y)),module,Swen::PARAM_GATEMODE);
    button->setLabel("GM");
    addParam(button);
    y+=dy;
    button=createParam<SmallButtonWithLabel>(mm2px(Vec(x,y)),module,Swen::PARAM_ROUND);
    button->setLabel("R");
    addParam(button);
    y+=dy;
    button=createParam<SmallButtonWithLabel>(mm2px(Vec(x,y)),module,Swen::PARAM_CLAMP);
    button->setLabel("C");
    addParam(button);

    for(int y=0;y<GHEIGHT;++y)
      for(int x=0;x<GWIDTH;++x) {
        int i=x+y*GWIDTH;
        addChild(createLight<CellLight<GreenLight>>(mm2px(Vec(3+x*8,66+y*7)),module,Swen::LIGHT_GRID+i));
        addOutput(createOutput<SmallPort>(mm2px(Vec(3+x*8,66+y*7)),module,Swen::OUT_CELL+i));
      }
  }
};

Model *modelSwen=createModel<Swen,SwenWidget>("Swen");
