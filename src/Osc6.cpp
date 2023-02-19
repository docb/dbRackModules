#include "dcb.h"
#include "filter.hpp"

struct Osc6Osc {
  uint32_t pos=0;
  Cheby1_32_BandFilter<float> bandFilter;
  uint32_t adjustPitch(uint32_t t,uint32_t bits,float sr,float freq) {
    int mod=1<<bits;
    double kp=freq*((double)mod/sr);
    double t0=double(t)*kp;
    return (uint32_t)t0;
  }

  float process(float freq,float sr,uint32_t bits,uint32_t x,uint32_t xa,uint32_t xb,uint32_t xc,uint32_t xd,uint32_t y,uint32_t ya,uint32_t yb,uint32_t yc,uint32_t yd,uint32_t ym,uint32_t yadd) {
    uint32_t t=adjustPitch(pos,bits,sr,freq);
    uint32_t val=((x&(t*xc>>xa))|t*xd>>xb)*(yadd+((y&(t*yc>>ya))|t*yd>>yb)*ym);
    uint32_t m=1<<bits;
    uint32_t mVal=val&(m-1);
    float fltVal=(float)mVal/(float)m-0.5f;
    pos=pos+1;
    return fltVal;
  }

  float processOVS(float freq,float sr,uint32_t bits,uint32_t x,uint32_t xa,uint32_t xb,uint32_t xc,uint32_t xd,uint32_t y,uint32_t ya,uint32_t yb,uint32_t yc,uint32_t yd,uint32_t ym,uint32_t yadd) {
    float out;
    for(int k=0;k<16;k++) {
      uint32_t t=adjustPitch(pos,bits,sr,freq/16.f);
      uint32_t val=((x&(t*xc>>xa))|t*xd>>xb)*(((y&(t*yc>>ya))|t*yd>>yb)*ym+yadd);
      uint32_t m=1<<bits;
      uint32_t mVal=val&(m-1);
      out=(float)mVal/(float)m-0.5f;
      out=bandFilter.process(out);
      pos=pos+1;
    }
    return out;
  }

  void reset(float ofs=0.f) {
    pos=0;
  }

};


struct Osc6 : Module {
  enum ParamId {
    BIT_PARAM,XA_PARAM,XB_PARAM,XC_PARAM,XD_PARAM,YA_PARAM,YB_PARAM,YC_PARAM,YD_PARAM,Y_MULT_PARAM,Y_ADD_PARAM,RST_PARAM,FREQ_PARAM,
    X_PARAM,Y_PARAM=X_PARAM+16,OCT_PARAM=Y_PARAM+16,PARAMS_LEN
  };
  enum InputId {
    VOCT_INPUT,X_INPUT,Y_INPUT,RST_INPUT,XA_INPUT,XB_INPUT,XC_INPUT,XD_INPUT,YA_INPUT,YB_INPUT,YC_INPUT,YD_INPUT,Y_MULT_INPUT,Y_ADD_INPUT,INPUTS_LEN
  };
  enum OutputId {
    CV_OUTPUT,OUTPUTS_LEN
  };
  enum LightId {
    LIGHTS_LEN
  };

  Osc6Osc osc[16];
  DCBlocker<float> dcb[16];
  dsp::SchmittTrigger rstTrigger;
  dsp::SchmittTrigger manualRstTrigger;
  bool oversample=true;
  bool blockDC=true;
  dsp::ClockDivider paramDivider;
  Osc6() {
    config(PARAMS_LEN,INPUTS_LEN,OUTPUTS_LEN,LIGHTS_LEN);
    configButton(RST_PARAM,"Rst");
    configParam(XA_PARAM,0,31,0,"XA");
    getParamQuantity(XA_PARAM)->snapEnabled=true;
    configParam(XB_PARAM,0,31,0,"XB");
    getParamQuantity(XB_PARAM)->snapEnabled=true;
    configParam(XC_PARAM,0,31,1,"XC");
    getParamQuantity(XC_PARAM)->snapEnabled=true;
    configParam(XD_PARAM,0,31,0,"XD");
    getParamQuantity(XD_PARAM)->snapEnabled=true;
    configParam(YA_PARAM,0,31,0,"YA");
    getParamQuantity(YA_PARAM)->snapEnabled=true;
    configParam(YB_PARAM,0,31,0,"YB");
    getParamQuantity(YB_PARAM)->snapEnabled=true;
    configParam(YC_PARAM,0,31,1,"YC");
    getParamQuantity(YC_PARAM)->snapEnabled=true;
    configParam(YD_PARAM,0,31,0,"YD");
    getParamQuantity(YD_PARAM)->snapEnabled=true;
    configParam(Y_MULT_PARAM,0,31,0,"YM");
    getParamQuantity(Y_MULT_PARAM)->snapEnabled=true;
    configParam(Y_ADD_PARAM,0,31,1,"YP");
    getParamQuantity(Y_ADD_PARAM)->snapEnabled=true;
    configParam(BIT_PARAM,2,16,8,"Bits");
    getParamQuantity(BIT_PARAM)->snapEnabled=true;
    configParam(OCT_PARAM,-8,8,0,"Oct");
    getParamQuantity(OCT_PARAM)->snapEnabled=true;
    configInput(VOCT_INPUT,"V/Oct");
    configInput(RST_INPUT,"Rst");
    configInput(X_INPUT,"X");
    configInput(Y_INPUT,"Y");
    configInput(XA_INPUT,"XA");
    configInput(XB_INPUT,"XB");
    configInput(XC_INPUT,"XC");
    configInput(XD_INPUT,"XD");
    configInput(YA_INPUT,"YA");
    configInput(YB_INPUT,"YB");
    configInput(YC_INPUT,"YC");
    configInput(YD_INPUT,"YD");
    configInput(Y_ADD_INPUT,"YP");
    configInput(Y_MULT_INPUT,"YM");
    configOutput(CV_OUTPUT,"CV");
    configParam(FREQ_PARAM,-8.f,4.f,0.f,"Frequency"," Hz",2,dsp::FREQ_C4);
    for(int k=0;k<16;k++) {
      std::string nr=std::to_string(k+1);
      configParam(X_PARAM+k,0,1,1,"X"+nr);
      configParam(Y_PARAM+k,0,1,1,"Y"+nr);
    }
    paramDivider.setDivision(32);
  }

  void setParamFromInput(ParamId param,InputId input) {
    if(inputs[input].isConnected()) {
      getParamQuantity(param)->setValue(inputs[input].getVoltage()*3.2f);
    }
  }

  void process(const ProcessArgs &args) override {
    if(paramDivider.process()) {
      setParamFromInput(XA_PARAM,XA_INPUT);
      setParamFromInput(XB_PARAM,XB_INPUT);
      setParamFromInput(XC_PARAM,XC_INPUT);
      setParamFromInput(XD_PARAM,XD_INPUT);
      setParamFromInput(YA_PARAM,YA_INPUT);
      setParamFromInput(YB_PARAM,YB_INPUT);
      setParamFromInput(YC_PARAM,YC_INPUT);
      setParamFromInput(YD_PARAM,YD_INPUT);
      setParamFromInput(Y_ADD_PARAM,Y_ADD_INPUT);
      setParamFromInput(Y_MULT_PARAM,Y_MULT_INPUT);
    }
    if(rstTrigger.process(inputs[RST_INPUT].getVoltage())|manualRstTrigger.process(params[RST_PARAM].getValue())) {
      for(int k=0;k<16;k++) {
        osc[k].reset();
      }
    }
    int channels=std::max(inputs[VOCT_INPUT].getChannels(),1);
    uint32_t x=0;
    for(int k=0;k<16;k++) {
      x|=(params[X_PARAM+k].getValue()>0.f)<<k;
    }
    if(inputs[X_INPUT].isConnected()) {
      for(int c=0;c<16;c++) {
        int i=inputs[X_INPUT].getVoltage(c)>0;
        x|=i<<c;
      }
    }
    uint32_t y=0;
    for(int k=0;k<16;k++) {
      y|=(params[Y_PARAM+k].getValue()>0.f)<<k;
    }
    if(inputs[Y_INPUT].isConnected()) {
      for(int c=0;c<16;c++) {
        int i=inputs[Y_INPUT].getVoltage(c)>0;
        y|=i<<c;
      }
    }
    uint32_t xa=params[XA_PARAM].getValue();
    uint32_t xb=params[XB_PARAM].getValue();
    uint32_t xc=params[XC_PARAM].getValue();
    uint32_t xd=params[XD_PARAM].getValue();
    uint32_t ya=params[YA_PARAM].getValue();
    uint32_t yb=params[YB_PARAM].getValue();
    uint32_t yc=params[YC_PARAM].getValue();
    uint32_t yd=params[YD_PARAM].getValue();
    uint32_t ym=params[Y_MULT_PARAM].getValue();
    uint32_t yadd=params[Y_ADD_PARAM].getValue();

    uint32_t bits=params[BIT_PARAM].getValue();
    float oct=params[OCT_PARAM].getValue();

    for(int c=0;c<channels;c++) {
      float voct=inputs[VOCT_INPUT].getVoltage(c);
      float pitch = params[FREQ_PARAM].getValue()+voct;
      float freq = dsp::FREQ_C4 * dsp::approxExp2_taylor5(pitch+oct + 30.f) / std::pow(2.f, 30.f);
      float out=oversample?osc[c].processOVS(freq,args.sampleRate,bits,x,xa,xb,xc,xd,y,ya,yb,yc,yd,ym,yadd):
                osc[c].process(freq,args.sampleRate,bits,x,xa,xb,xc,xd,y,ya,yb,yc,yd,ym,yadd);
      outputs[CV_OUTPUT].setVoltage((blockDC?dcb[c].process(out):out)*10.f,c);
    }
    outputs[CV_OUTPUT].setChannels(channels);
  }

  json_t *dataToJson() override {
    json_t *data=json_object();
    json_object_set_new(data,"oversample",json_boolean(oversample));
    json_object_set_new(data,"blockDC",json_boolean(blockDC));
    return data;
  }

  void dataFromJson(json_t *rootJ) override {
    json_t *jOverSample = json_object_get(rootJ,"oversample");
    if(jOverSample!=nullptr) oversample = json_boolean_value(jOverSample);
    json_t *jDcBlock = json_object_get(rootJ,"blockDC");
    if(jDcBlock!=nullptr) blockDC = json_boolean_value(jDcBlock);
  }

};


struct DBSlider : app::SvgSlider {
  DBSlider() {
    setBackgroundSvg(Svg::load(asset::plugin(pluginInstance,"res/DBSlider.svg")));
    setHandleSvg(Svg::load(asset::plugin(pluginInstance,"res/DBSliderHandle.svg")));
    setHandlePosCentered(
      math::Vec(19.84260/2, 92 - 11.74218/2),
      math::Vec(19.84260/2, 0.0 + 11.74218/2)
    );
  }
};

struct Osc6Widget : ModuleWidget {
	Osc6Widget(Osc6* module) {
		setModule(module);
		setPanel(createPanel(asset::plugin(pluginInstance, "res/Osc6.svg")));

    float x=4;
    float y=8;
    float d=11.5;
    float dx=0.5;
    for(int k=0;k<16;k++) {
      auto button=createParam<SmallButtonWithLabel>(mm2px(Vec(x+(k/8)*8,(k%8)*4+y)),module,Osc6::X_PARAM+k);
      button->setLabel(std::to_string(k+1));
      addParam(button);
    }
    addInput(createInput<SmallPort>(mm2px(Vec(x+5,y+34)),module,Osc6::X_INPUT));
    x+=20;
    addParam(createParam<DBSlider>(mm2px(Vec(x,y)),module,Osc6::XC_PARAM));
    addInput(createInput<SmallPort>(mm2px(Vec(x,y+34)),module,Osc6::XC_INPUT));
    x+=d;
    addParam(createParam<DBSlider>(mm2px(Vec(x,y)),module,Osc6::XA_PARAM));
    addInput(createInput<SmallPort>(mm2px(Vec(x+dx,y+34)),module,Osc6::XA_INPUT));
    x+=d;
    addParam(createParam<DBSlider>(mm2px(Vec(x,y)),module,Osc6::XD_PARAM));
    addInput(createInput<SmallPort>(mm2px(Vec(x+dx,y+34)),module,Osc6::XD_INPUT));
    x+=d;
    addParam(createParam<DBSlider>(mm2px(Vec(x,y)),module,Osc6::XB_PARAM));
    addInput(createInput<SmallPort>(mm2px(Vec(x+dx,y+34)),module,Osc6::XB_INPUT));
    x+=d;
    addParam(createParam<DBSlider>(mm2px(Vec(x,y)),module,Osc6::Y_ADD_PARAM));
    addInput(createInput<SmallPort>(mm2px(Vec(x+dx,y+34)),module,Osc6::Y_ADD_INPUT));
    y=68;
    x=4;
    for(int k=0;k<16;k++) {
      auto button=createParam<SmallButtonWithLabel>(mm2px(Vec(x+(k/8)*8,(k%8)*4+y)),module,Osc6::Y_PARAM+k);
      button->setLabel(std::to_string(k+1));
      addParam(button);
    }
    addInput(createInput<SmallPort>(mm2px(Vec(x+5,y+34)),module,Osc6::Y_INPUT));
    x+=20;
    addParam(createParam<DBSlider>(mm2px(Vec(x,y)),module,Osc6::YC_PARAM));
    addInput(createInput<SmallPort>(mm2px(Vec(x,y+34)),module,Osc6::YC_INPUT));
    x+=d;
    addParam(createParam<DBSlider>(mm2px(Vec(x,y)),module,Osc6::YA_PARAM));
    addInput(createInput<SmallPort>(mm2px(Vec(x+dx,y+34)),module,Osc6::YA_INPUT));
    x+=d;
    addParam(createParam<DBSlider>(mm2px(Vec(x,y)),module,Osc6::YD_PARAM));
    addInput(createInput<SmallPort>(mm2px(Vec(x+dx,y+34)),module,Osc6::YD_INPUT));
    x+=d;
    addParam(createParam<DBSlider>(mm2px(Vec(x,y)),module,Osc6::YB_PARAM));
    addInput(createInput<SmallPort>(mm2px(Vec(x+dx,y+34)),module,Osc6::YB_INPUT));
    x+=d;
    addParam(createParam<DBSlider>(mm2px(Vec(x,y)),module,Osc6::Y_MULT_PARAM));
    addInput(createInput<SmallPort>(mm2px(Vec(x+dx,y+34)),module,Osc6::Y_MULT_INPUT));

    y=116;
    x=4;
    addParam(createParam<TrimbotWhite>(mm2px(Vec(x,y)),module,Osc6::BIT_PARAM));
    x+=10;
    addParam(createParam<TrimbotWhite>(mm2px(Vec(x,y)),module,Osc6::FREQ_PARAM));
    x=24;
    addParam(createParam<TrimbotWhite>(mm2px(Vec(x,y)),module,Osc6::OCT_PARAM));
    x+=d;
    addInput(createInput<SmallPort>(mm2px(Vec(x,y)),module,Osc6::VOCT_INPUT));
    x+=d;
    addParam(createParam<MLEDM>(mm2px(Vec(x,y)),module,Osc6::RST_PARAM));
    x+=d;
    addInput(createInput<SmallPort>(mm2px(Vec(x,y)),module,Osc6::RST_INPUT));
    x+=d;
    addOutput(createOutput<SmallPort>(mm2px(Vec(x,y)),module,Osc6::CV_OUTPUT));
	}

  void appendContextMenu(Menu* menu) override {
    Osc6 *module=dynamic_cast<Osc6 *>(this->module);
    assert(module);

    menu->addChild(new MenuSeparator);

    menu->addChild(createBoolPtrMenuItem("Oversample","",&module->oversample));
    menu->addChild(createBoolPtrMenuItem("block DC","",&module->blockDC));

  }

};


Model* modelOsc6 = createModel<Osc6, Osc6Widget>("Osc6");