#include "dcb.h"
#include "Computer.hpp"

template<typename T>
struct SuperLFOSC {
  T phase=0.f;
  T freq=1.021977f;
  Computer<T> *computer;
  float phs=TWOPIF*2;
  float ofs = 0;

  void setPitch(T voct,T frq) {
    freq=1.021977f*simd::pow(2.0f,frq+voct);
  }

  void reset() {
    phase=0;
  }

  void updatePhase(float oneSampleTime,float p=0.f) {
    //T dPhase=freq*oneSampleTime*phs;
    //phase=simd::fmod(phase+dPhase,phs);
    ofs = p;
    T dPhase=freq*oneSampleTime;
    phase=phase+dPhase;
    phase-=simd::floor(phase);
  }
  void superformula(T t,T kx,T ky,T krx,T kry,T y,T z,T n1,T n2,T n3,T a,T b,T &outX,T &outY) {

    T t1=cosl(y*t/4);
    T t2=sinl(z*t/4);
    T r0=fpow(simd::fabs(t1/a),n2)+fpow(simd::fabs(t2/b),n3);
    T r=fpow(r0,-1/n1);
    outX=kx+krx*r*cosl(t);
    outY=ky+kry*r*sinl(t);
  }
  void process(T cx,T cy,T rx,T ry,T rot,T y,T z,T n1,T n2,T n3,T a,T b,T &xc,T &yc) {
    computer->superformula((phase+ofs)*phs,cx,cy,rx,ry,y,z,n1,n2,n3,a,b,xc,yc);
    computer->rotate_point(cx,cy,rot,xc,yc);
  }
};
struct SuperLFOSC2 {
  float phase=0.f;
  float freq=1.021977f;
  float phs=TWOPIF*2;
  float ofs = 0;

  void setPitch(float voct,float frq) {
    freq=1.021977f*powf(2.0f,frq+voct);
  }

  void reset() {
    phase=0;
  }

  void updatePhase(float oneSampleTime,float p=0.f) {
    //T dPhase=freq*oneSampleTime*phs;
    //phase=simd::fmod(phase+dPhase,phs);
    ofs = p;
    float dPhase=freq*oneSampleTime;
    phase=phase+dPhase;
    phase-=simd::floor(phase);
  }
  void superformula(float t,float kx,float ky,float krx,float kry,float y,float z,float n1,float n2,float n3,float a,float b,float& outX,float& outY) {

    float t1=cosf(y*t/4);
    float t2=sinf(z*t/4);
    float r0=powf(fabsf(t1/a),n2)+powf(fabsf(t2/b),n3);
    float r=powf(r0,-1/n1);
    outX=kx+krx*r*cosf(t);
    outY=ky+kry*r*sinf(t);
  }
  void rotate_point(float cx,float cy,float angle,float& x,float& y) {
    float s=sinf(angle);
    float c=cosf(angle);
    x-=cx;
    y-=cy;
    float xnew=x*c-y*s;
    float ynew=x*s+y*c;
    x=xnew+cx;
    y=ynew+cy;
  }
  void process(float cx,float cy,float rx,float ry,float rot,float y,float z,float n1,float n2,float n3,float a,float b,float &xc,float &yc) {
    superformula((phase+ofs)*phs,cx,cy,rx,ry,y,z,n1,n2,n3,a,b,xc,yc);
    rotate_point(cx,cy,rot,xc,yc);
  }
};


struct SuperLFO : Module {
  enum ParamIds {
    FREQ_PARAM,RX_PARAM,RY_PARAM,ROT_PARAM,M0_PARAM,M1_PARAM,N1_PARAM,N1_SGN_PARAM,N2_PARAM,N3_PARAM,A_PARAM,B_PARAM,X_SCL_PARAM,Y_SCL_PARAM,RX_SCL_PARAM,RY_SCL_PARAM,ROT_SCL_PARAM,N1_SCL_PARAM,N2_SCL_PARAM,N3_SCL_PARAM,A_SCL_PARAM,B_SCL_PARAM,PARAMS_LEN
  };
  enum InputIds {
    VOCT_INPUT,PHS_INPUT,RST_INPUT,RX_INPUT,RY_INPUT,ROT_INPUT,N1_INPUT,N2_INPUT,N3_INPUT,A_INPUT,B_INPUT,INPUTS_LEN
  };
  enum OutputIds {
    X_OUTPUT,Y_OUTPUT,OUTPUTS_LEN
  };
  enum LightIds {
    LIGHTS_LEN
  };
  Computer<float> computer;
  //SuperLFOSC<float> osc[16];
  SuperLFOSC2 osc[16];
  dsp::SchmittTrigger rstTrigger;

  SuperLFO() {
    config(PARAMS_LEN,INPUTS_LEN,OUTPUTS_LEN,LIGHTS_LEN);
    configParam(M0_PARAM,0.f,16.f,4.f,"M0");
    getParamQuantity(M0_PARAM)->snapEnabled=true;
    configParam(M1_PARAM,0.f,16.f,4.f,"M1");
    getParamQuantity(M1_PARAM)->snapEnabled=true;
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
    configParam(FREQ_PARAM,-8.f,8.f,0.f,"Frequency"," Hz",2,1.021977f);
    configInput(VOCT_INPUT,"V/Oct");
    configInput(RX_INPUT,"Scale X");
    configInput(RY_INPUT,"Scale Y");
    configInput(ROT_INPUT,"Rotation");
    configInput(N1_INPUT,"N1");
    configInput(N2_INPUT,"N2");
    configInput(N3_INPUT,"N3");
    configInput(A_INPUT,"A");
    configInput(B_INPUT,"B");
    configOutput(X_OUTPUT,"X");
    configOutput(Y_OUTPUT,"Y");
    /*
    for(int k=0;k<16;k++) {
      osc[k].computer=&computer;
    }
     */
  }

  void process(const ProcessArgs &args) override {
    float m0=floorf(params[M0_PARAM].getValue());
    float m1=floorf(params[M1_PARAM].getValue());
    float n1=params[N1_PARAM].getValue();
    float n2=params[N2_PARAM].getValue();
    float n3=params[N3_PARAM].getValue();
    float a=params[A_PARAM].getValue();
    float b=params[B_PARAM].getValue();
    float rx=params[RX_PARAM].getValue();
    float ry=params[RY_PARAM].getValue();
    float rot=params[ROT_PARAM].getValue();
    float freq=params[FREQ_PARAM].getValue();
    int channels=std::max(inputs[VOCT_INPUT].getChannels(),1);

    if(rstTrigger.process(inputs[RST_INPUT].getVoltage())) {
      for(auto &k:osc) {
        k.reset();
      }
    }

    for(int c=0;c<channels;c++) {
      auto *oscil=&osc[c];

      float n1In=inputs[N1_INPUT].getVoltage(c)*params[N1_SCL_PARAM].getValue()+n1;
      n1In=clamp(n1In,0.05f,16.f);
      float n1Ins=params[N1_SGN_PARAM].getValue()>0.f?-1.f*n1In:n1In;

      float aIn=inputs[A_INPUT].getVoltage(c)*params[A_SCL_PARAM].getValue()+a;
      aIn=clamp(aIn,0.05f,5.f);

      float bIn=inputs[B_INPUT].getVoltage(c)*params[B_SCL_PARAM].getValue()+b;
      bIn=clamp(bIn,0.05f,5.f);

      float n2In=inputs[N2_INPUT].getVoltage(c)*params[N2_SCL_PARAM].getValue()+n2;
      n2In=clamp(n2In,-5.f,5.f);
      float n3In=inputs[N3_INPUT].getVoltage(c)*params[N3_SCL_PARAM].getValue()+n3;
      n3In=clamp(n3In,-5.f,5.f);
      float rxIn=inputs[RX_INPUT].getVoltage(c)*params[RX_SCL_PARAM].getValue()+rx;
      float ryIn=inputs[RY_INPUT].getVoltage(c)*params[RY_SCL_PARAM].getValue()+ry;
      float rotIn=inputs[ROT_INPUT].getVoltage(c)*params[ROT_SCL_PARAM].getValue()+rot;

      float voct=inputs[VOCT_INPUT].getVoltage(c);
      float phs=inputs[PHS_INPUT].getVoltage(c)/10.f;
      oscil->setPitch(voct,freq);
      oscil->updatePhase(args.sampleTime,phs);
      float xout,yout;
      oscil->process(0.f,0.f,rxIn,ryIn,rotIn,m0,m1,n1Ins,n2In,n3In,aIn,bIn,xout,yout);
      outputs[X_OUTPUT].setVoltage(clamp(xout*5.f,-10.f,10.f),c);
      outputs[Y_OUTPUT].setVoltage(clamp(yout*5.f,-10.f,10.f),c);
    }
    outputs[X_OUTPUT].setChannels(channels);
    outputs[Y_OUTPUT].setChannels(channels);
  }
};


struct SuperLFOWidget : ModuleWidget {
  SuperLFOWidget(SuperLFO *module) {
    setModule(module);
    setPanel(createPanel(asset::plugin(pluginInstance,"res/SuperLFO.svg")));

    addChild(createWidget<ScrewSilver>(Vec(RACK_GRID_WIDTH,0)));
    addChild(createWidget<ScrewSilver>(Vec(box.size.x-2*RACK_GRID_WIDTH,0)));
    addChild(createWidget<ScrewSilver>(Vec(RACK_GRID_WIDTH,RACK_GRID_HEIGHT-RACK_GRID_WIDTH)));
    addChild(createWidget<ScrewSilver>(Vec(box.size.x-2*RACK_GRID_WIDTH,RACK_GRID_HEIGHT-RACK_GRID_WIDTH)));

    addParam(createParam<TrimbotWhite9>(mm2px(Vec(7,MHEIGHT-106.f-9.f)),module,SuperLFO::M0_PARAM));
    addParam(createParam<TrimbotWhite9>(mm2px(Vec(7,MHEIGHT-92.f-9.f)),module,SuperLFO::M1_PARAM));
    addInput(createInput<SmallPort>(mm2px(Vec(8.6f,MHEIGHT-79.5f-6.287f)),module,SuperLFO::PHS_INPUT));
    addInput(createInput<SmallPort>(mm2px(Vec(8.6f,MHEIGHT-65.5f-6.287f)),module,SuperLFO::RST_INPUT));
    addParam(createParam<TrimbotWhite9>(mm2px(Vec(7,MHEIGHT-50.f-9.f)),module,SuperLFO::FREQ_PARAM));
    addInput(createInput<SmallPort>(mm2px(Vec(8.6f,MHEIGHT-37.5f-6.287f)),module,SuperLFO::VOCT_INPUT));
    addOutput(createOutput<SmallPort>(mm2px(Vec(8.6f,MHEIGHT-22.75f-6.287f)),module,SuperLFO::X_OUTPUT));
    addOutput(createOutput<SmallPort>(mm2px(Vec(8.6f,MHEIGHT-10.75f-6.287f)),module,SuperLFO::Y_OUTPUT));

    float rx=23.5f;
    addParam(createParam<TrimbotWhite9>(mm2px(Vec(rx,MHEIGHT-106.f-9.f)),module,SuperLFO::RX_PARAM));
    addInput(createInput<SmallPort>(mm2px(Vec(rx+13.33f,MHEIGHT-107.5f-6.287f)),module,SuperLFO::RX_INPUT));
    addParam(createParam<TrimbotWhite>(mm2px(Vec(rx+24.f,MHEIGHT-107.5f-6.371f)),module,SuperLFO::RX_SCL_PARAM));

    addParam(createParam<TrimbotWhite9>(mm2px(Vec(rx,MHEIGHT-92.f-9.f)),module,SuperLFO::RY_PARAM));
    addInput(createInput<SmallPort>(mm2px(Vec(rx+13.33f,MHEIGHT-93.5f-6.287f)),module,SuperLFO::RY_INPUT));
    addParam(createParam<TrimbotWhite>(mm2px(Vec(rx+24.f,MHEIGHT-93.5f-6.371f)),module,SuperLFO::RY_SCL_PARAM));

    addParam(createParam<TrimbotWhite9>(mm2px(Vec(rx,MHEIGHT-78.f-9.f)),module,SuperLFO::ROT_PARAM));
    addInput(createInput<SmallPort>(mm2px(Vec(rx+13.33f,MHEIGHT-79.5f-6.287f)),module,SuperLFO::ROT_INPUT));
    addParam(createParam<TrimbotWhite>(mm2px(Vec(rx+24.f,MHEIGHT-79.5f-6.371f)),module,SuperLFO::ROT_SCL_PARAM));

    addParam(createParam<TrimbotWhite9>(mm2px(Vec(rx,MHEIGHT-64.f-9.f)),module,SuperLFO::N1_PARAM));
    addParam(createParam<SmallRoundButton>(mm2px(Vec(rx+12.33f,MHEIGHT-73.f)),module,SuperLFO::N1_SGN_PARAM));
    addInput(createInput<SmallPort>(mm2px(Vec(rx+13.33f,MHEIGHT-65.5f-6.287f)),module,SuperLFO::N1_INPUT));
    addParam(createParam<TrimbotWhite>(mm2px(Vec(rx+24.f,MHEIGHT-65.5f-6.371f)),module,SuperLFO::N1_SCL_PARAM));

    addParam(createParam<TrimbotWhite9>(mm2px(Vec(rx,MHEIGHT-50.f-9.f)),module,SuperLFO::N2_PARAM));
    addInput(createInput<SmallPort>(mm2px(Vec(rx+13.33f,MHEIGHT-51.5f-6.287f)),module,SuperLFO::N2_INPUT));
    addParam(createParam<TrimbotWhite>(mm2px(Vec(rx+24.f,MHEIGHT-51.5f-6.371f)),module,SuperLFO::N2_SCL_PARAM));

    addParam(createParam<TrimbotWhite9>(mm2px(Vec(rx,MHEIGHT-36.f-9.f)),module,SuperLFO::N3_PARAM));
    addInput(createInput<SmallPort>(mm2px(Vec(rx+13.33f,MHEIGHT-37.5f-6.287f)),module,SuperLFO::N3_INPUT));
    addParam(createParam<TrimbotWhite>(mm2px(Vec(rx+24.f,MHEIGHT-37.5f-6.371f)),module,SuperLFO::N3_SCL_PARAM));

    addParam(createParam<TrimbotWhite9>(mm2px(Vec(rx,MHEIGHT-22.f-9.f)),module,SuperLFO::A_PARAM));
    addInput(createInput<SmallPort>(mm2px(Vec(rx+13.33f,MHEIGHT-23.5f-6.287f)),module,SuperLFO::A_INPUT));
    addParam(createParam<TrimbotWhite>(mm2px(Vec(rx+24.f,MHEIGHT-23.5f-6.371f)),module,SuperLFO::A_SCL_PARAM));

    addParam(createParam<TrimbotWhite9>(mm2px(Vec(rx,MHEIGHT-8.f-9.f)),module,SuperLFO::B_PARAM));
    addInput(createInput<SmallPort>(mm2px(Vec(rx+13.33f,MHEIGHT-9.5f-6.287f)),module,SuperLFO::B_INPUT));
    addParam(createParam<TrimbotWhite>(mm2px(Vec(rx+24.f,MHEIGHT-9.5f-6.371f)),module,SuperLFO::B_SCL_PARAM));

  }
};


Model *modelSuperLFO=createModel<SuperLFO,SuperLFOWidget>("SuperLFO");