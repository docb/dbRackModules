#include "dcb.h"
#include "clipper.hpp"
using simd::float_4;
template<typename T>
struct FilterOut {
  T LP=0.f;
  T BP=0.f;
  T HP=0.f;
  T BR=0.f;
};

template<typename T>
struct SVFilter {
  T s1=0.f;
  T s2=0.f;
  Clipper<T> clipper;
  /*
  T df1(T drive,T in) {
    T n=in*drive;
    return simd::ifelse(n>1.5f,1.f,simd::ifelse(n<-1.5f,-1.f,n-(4.f/27.f)*n*n*n));
  }

  T df2(T drive,T in) {
    T n=in*drive;
    return simd::ifelse(n>3.f,1.f,simd::ifelse(n<-3.f,-1.f,n*(27.0f+n*n)/(27.0f+9.0f*n*n)));
  }

  T df3(T drive,T in) {
    T n=in*drive;
    return simd::ifelse(n>2.4674f,1.f,simd::ifelse(n<-2.4674f,-1.f,simd::sin(n*0.63661977f)));
  }

  T df(T drive,T in,int mode) {
    switch(mode) {
      case 1: return df2(drive,in);
      case 2: return df3(drive,in);
      default: return df1(drive,in);
    }
  }
   */
  FilterOut<T> process(T in,T freq,T res,T drive,float piosr,unsigned mode=0) {
    FilterOut<T> y={};
    T w=simd::tan(freq*piosr);
    T R=1.f/res;
    T fac=1.f/(1.f+w*R+w*w);
    y.HP=(in-(R+w)*s1-s2)*fac;
    T u=w*clipper.process(drive,y.HP,mode);
    y.BP=u+s1;
    s1=y.BP+u;
    u=w*clipper.process(drive,y.BP,mode);
    y.LP=u+s2;
    s2=y.LP+u;
    y.BR=y.HP+y.LP;
    return y;
  }

  void reset() {
    s1=0.f;
    s2=0.f;
  }
};

struct USVF : Module {
  enum ParamId {
    FREQ_PARAM,FREQ_CV_PARAM,R_PARAM,R_CV_PARAM,D_PARAM,D_CV_PARAM,PARAMS_LEN
  };
  enum InputId {
    CV_INPUT,FREQ_INPUT,R_INPUT,D_INPUT,INPUTS_LEN
  };
  enum OutputId {
    LP_OUTPUT,BP_OUTPUT,HP_OUTPUT,OUTPUTS_LEN
  };
  enum LightId {
    LIGHTS_LEN
  };
  SVFilter<float_4> filter[4];
  unsigned mode=0;
  DCBlocker<float_4> dcbH[4];
  DCBlocker<float_4> dcbB[4];
  DCBlocker<float_4> dcbL[4];
  USVF() {
    config(PARAMS_LEN,INPUTS_LEN,OUTPUTS_LEN,LIGHTS_LEN);
    configParam(FREQ_PARAM,8.f,14.5f,12.f,"Frequency"," Hz",2,1);
    configInput(FREQ_INPUT,"Freq");
    configParam(FREQ_CV_PARAM,0,1,0,"Freq CV","%",0,100);
    configParam(R_PARAM,0.5,20,0.5,"Q");
    configInput(R_INPUT,"Q");
    configParam(R_CV_PARAM,0,1,0,"Q CV");
    configParam(D_PARAM,0,1,0.2,"Drive");
    configInput(D_INPUT,"Drive");
    configParam(D_CV_PARAM,0,1,0,"Drive CV");
    configOutput(LP_OUTPUT,"Low Pass");
    configOutput(HP_OUTPUT,"High Pass");
    configOutput(BP_OUTPUT,"Band Pass");
    configInput(CV_INPUT,"In");
    configBypass(CV_INPUT,LP_OUTPUT);
  }

  void onReset(const ResetEvent &e) override {
    for(auto &f:filter) {
      f.reset();
    }
  }

  void process(const ProcessArgs &args) override {
    const float piosr=M_PI/args.sampleRate;
    const float r=params[R_PARAM].getValue();
    const float rCvParam=params[R_CV_PARAM].getValue();
    const float drive=params[D_PARAM].getValue();
    const float driveCvParam=params[D_CV_PARAM].getValue();
    const float freqParam=params[FREQ_PARAM].getValue();
    const float freqCvParam=params[FREQ_CV_PARAM].getValue();
    int channels=inputs[CV_INPUT].getChannels();
    if(outputs[HP_OUTPUT].isConnected()||outputs[BP_OUTPUT].isConnected()||outputs[LP_OUTPUT].isConnected()) {
      for(int c=0;c<channels;c+=4) {
        const float_4 pitch=freqParam+inputs[FREQ_INPUT].getPolyVoltageSimd<float_4>(c)*freqCvParam;
        const float_4 cutoff=simd::clamp(simd::pow(2.f,pitch),2.f,args.sampleRate*0.4f);
        const float_4 R=simd::clamp(r+inputs[R_INPUT].getPolyVoltageSimd<float_4>(c)*rCvParam*0.1f,0.5f,20.f);
        const float_4 D=simd::clamp(drive+inputs[D_INPUT].getPolyVoltageSimd<float_4>(c)*driveCvParam*0.1f,0.0f,1.f);
        const float_4 in=inputs[CV_INPUT].getVoltageSimd<float_4>(c);
        const FilterOut<float_4> out=filter[c/4].process(in,cutoff,R,D,piosr,mode);
        if(outputs[HP_OUTPUT].isConnected()) outputs[HP_OUTPUT].setVoltageSimd(dcbH[c/4].process(out.HP),c);
        if(outputs[BP_OUTPUT].isConnected()) outputs[BP_OUTPUT].setVoltageSimd(dcbB[c/4].process(out.BP),c);
        if(outputs[LP_OUTPUT].isConnected()) outputs[LP_OUTPUT].setVoltageSimd(dcbL[c/4].process(out.LP),c);
      }
    }
    outputs[HP_OUTPUT].setChannels(channels);
    outputs[BP_OUTPUT].setChannels(channels);
    outputs[LP_OUTPUT].setChannels(channels);
  }

  json_t *dataToJson() override {
    json_t *data=json_object();
    json_object_set_new(data,"mode",json_integer(mode));
    return data;
  }

  void dataFromJson(json_t *rootJ) override {
    json_t *jMode = json_object_get(rootJ,"mode");
    if(jMode!=nullptr) mode = json_integer_value(jMode);
  }

};


struct USVFWidget : ModuleWidget {
  std::vector<std::string> labels={"Tanh","Hard Clip","Sin","Overdrive","Exp","Sqr","Abs"};
  USVFWidget(USVF *module) {
    setModule(module);
    setPanel(createPanel(asset::plugin(pluginInstance,"res/USVF.svg")));

    float y=9;
    float x=1.9;
    addParam(createParam<TrimbotWhite>(mm2px(Vec(x,y)),module,USVF::FREQ_PARAM));
    y+=7;
    addInput(createInput<SmallPort>(mm2px(Vec(x,y)),module,USVF::FREQ_INPUT));
    y+=7;
    addParam(createParam<TrimbotWhite>(mm2px(Vec(x,y)),module,USVF::FREQ_CV_PARAM));
    y=34;
    addParam(createParam<TrimbotWhite>(mm2px(Vec(x,y)),module,USVF::R_PARAM));
    y+=7;
    addInput(createInput<SmallPort>(mm2px(Vec(x,y)),module,USVF::R_INPUT));
    y+=7;
    addParam(createParam<TrimbotWhite>(mm2px(Vec(x,y)),module,USVF::R_CV_PARAM));
    y=59;
    addParam(createParam<TrimbotWhite>(mm2px(Vec(x,y)),module,USVF::D_PARAM));
    y+=7;
    addInput(createInput<SmallPort>(mm2px(Vec(x,y)),module,USVF::D_INPUT));
    y+=7;
    addParam(createParam<TrimbotWhite>(mm2px(Vec(x,y)),module,USVF::D_CV_PARAM));

    y=83;
    addInput(createInput<SmallPort>(mm2px(Vec(x,y)),module,USVF::CV_INPUT));
    y+=11;
    addOutput(createOutput<SmallPort>(mm2px(Vec(x,y)),module,USVF::BP_OUTPUT));
    y+=11;
    addOutput(createOutput<SmallPort>(mm2px(Vec(x,y)),module,USVF::HP_OUTPUT));
    y+=11;
    addOutput(createOutput<SmallPort>(mm2px(Vec(x,y)),module,USVF::LP_OUTPUT));
  }
  void appendContextMenu(Menu *menu) override {
    USVF *module=dynamic_cast<USVF *>(this->module);
    assert(module);
    menu->addChild(new MenuSeparator);
    auto modeSelect=new LabelIntSelectItem(&module->mode,labels);
    modeSelect->text="Nonlinear mode";
    modeSelect->rightText=labels[module->mode]+"  "+RIGHT_ARROW;
    menu->addChild(modeSelect);
  }
};


Model *modelUSVF=createModel<USVF,USVFWidget>("USVF");