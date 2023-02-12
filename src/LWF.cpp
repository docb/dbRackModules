#include "dcb.h"

#include <cmath>
#include <limits>

/**
 * Ported from https://github.com/eres-j/VCVRack-plugin-JE to Rack v2
 * Changes:
 * - Supports Polyphony, CV Attenuators, and modulation for the resistors
 * - Implemented with simd as far as possible
 * - Used Fukushima Lambert W function instead of utl (more CPU friendly)
 */

using simd::float_4;
/**
 * Adapted from https://github.com/DarkoVeberic/LambertW.git
 * Copyright (C) 2011 Darko Veberic, darko.veberic@ijs.si
 *
 */
struct LW {
  // Fukushima method
  const double q[21]={-1,+1,-0.333333333333333333,+0.152777777777777778,-0.0796296296296296296,+0.0445023148148148148,-0.0259847148736037625,+0.0156356325323339212,-0.00961689202429943171,+0.00601454325295611786,-0.00381129803489199923,+0.00244087799114398267,-0.00157693034468678425,+0.00102626332050760715,-0.000672061631156136204,+0.000442473061814620910,-0.000292677224729627445,+0.000194387276054539318,-0.000129574266852748819,+0.0000866503580520812717,
                    -0.0000581136075044138168};

  double LambertWSeries(const double p) {
    const double ap=abs(p);
    if(ap<0.01159)
      return -1+p*(1+p*(q[2]+p*(q[3]+p*(q[4]+p*(q[5]+p*q[6])))));
    else if(ap<0.0766)
      return -1+p*(1+p*(q[2]+p*(q[3]+p*(q[4]+p*(q[5]+p*(q[6]+p*(q[7]+p*(q[8]+p*(q[9]+p*q[10])))))))));
    else
      return -1+p*(1+p*(q[2]+p*(q[3]+p*(q[4]+p*(q[5]+p*(q[6]+p*(q[7]+p*(q[8]+p*(q[9]+p*(q[10]+p*(q[11]+p*(q[12]+p*(q[13]+p*(q[14]+p*(q[15]+p*(q[16]+p*(q[17]+p*(q[18]+p*(q[19]+p*q[20])))))))))))))))))));
  }

  inline double LambertW0ZeroSeries(const double z) {
    return z*(1-z*(1-z*(1.5-z*(2.6666666666666666667-z*(5.2083333333333333333-z*(10.8-z*(23.343055555555555556-z*(52.012698412698412698-z*(118.62522321428571429-z*(275.57319223985890653-z*(649.78717234347442681-z*(1551.1605194805194805-z*(3741.4497029592385495-z*(9104.5002411580189358-z*(22324.308512706601434-z*(55103.621972903835338-z*136808.86090394293563))))))))))))))));
  }

  inline double FinalResult(const double w,const double y) {
    const double f0=w-y;
    const double f1=1+y;
    const double f00=f0*f0;
    const double f11=f1*f1;
    const double f0y=f0*y;
    return w-4*f0*(6*f1*(f11+f0y)+f00*y)/(f11*(24*f11+36*f0y)+f00*(6*y*y+8*f1*y+f0y));
  }

  double lambertW0(const double z) {
    static double e[66];
    static double g[65];
    static double a[12];
    static double b[12];

    if(!e[0]) {
      const double e1=1/M_E;
      double ej=1;
      e[0]=M_E;
      e[1]=1;
      g[0]=0;
      for(int j=1,jj=2;jj<66;++jj) {
        ej*=M_E;
        e[jj]=e[j]*e1;
        g[j]=j*ej;
        j=jj;
      }
      a[0]=sqrt(e1);
      b[0]=0.5;
      for(int j=0,jj=1;jj<12;++jj) {
        a[jj]=sqrt(a[j]);
        b[jj]=b[j]*0.5;
        j=jj;
      }
    }
    if(abs(z)<0.05)
      return LambertW0ZeroSeries(z);
    if(z<-0.35) {
      const double p2=2*(M_E*z+1);
      if(p2>0)
        return LambertWSeries(sqrt(p2));
      if(p2==0)
        return -1;
      //cerr<<"(lambertw0) Argument out of range. z="<<z<<endl;
      return std::numeric_limits<double>::quiet_NaN();
    }
    int n;
    for(n=0;n<=2;++n)
      if(g[n]>z)
        goto line1;
    n=2;
    for(int j=1;j<=5;++j) {
      n*=2;
      if(g[n]>z)
        goto line2;
    }
    //cerr<<"(lambertw0) Argument too large. z="<<z<<endl;
    return std::numeric_limits<double>::quiet_NaN();
    line2:
    {
      int nh=n/2;
      for(int j=1;j<=5;++j) {
        nh/=2;
        if(nh<=0)
          break;
        if(g[n-nh]>z)
          n-=nh;
      }
    }
    line1:
    --n;
    int jmax=8;
    if(z<=-0.36)
      jmax=12;
    else if(z<=-0.3)
      jmax=11;
    else if(n<=0)
      jmax=10;
    else if(n<=1)
      jmax=9;
    double y=z*e[n+1];
    double w=n;
    for(int j=0;j<jmax;++j) {
      const double wj=w+b[j];
      const double yj=y*a[j];
      if(wj<yj) {
        w=wj;
        y=yj;
      }
    }
    return FinalResult(w,y);
  }

};

struct LWF : Module {
  enum ParamIds {
    INPUT_GAIN_PARAM,DC_OFFSET_PARAM,OUTPUT_GAIN_PARAM,RESISTOR_PARAM,LOAD_RESISTOR_PARAM,INPUT_GAIN_CV_PARAM,DC_OFFSET_CV_PARAM,OUTPUT_GAIN_CV_PARAM,RESISTOR_CV_PARAM,LOAD_RESISTOR_CV_PARAM,NUM_PARAMS
  };

  enum InputIds {
    INPUT_INPUT,INPUT_GAIN_INPUT,DC_OFFSET_INPUT,RESISTOR_INPUT,LOAD_RESISTOR_INPUT,OUTPUT_GAIN_INPUT,NUM_INPUTS
  };

  enum OutputIds {
    OUTPUT_OUTPUT,NUM_OUTPUTS
  };
  float maxExponent;
  float maxValue;
  const float thermalVoltage=0.026f;
  const float saturationCurrent=10e-17f;
  DCBlocker<float_4> dcb[4];
  LW fukushimaLW;

  LWF() {
    config(NUM_PARAMS,NUM_INPUTS,NUM_OUTPUTS,0);
    configParam(INPUT_GAIN_PARAM,0.f,1.f,0.1f,"Input gain");
    configParam(DC_OFFSET_PARAM,-5.f,5.f,0.f,"Input offset");
    configParam(OUTPUT_GAIN_PARAM,0.f,1.f,1.f,"Output gain");
    configParam(RESISTOR_PARAM,10000.f,100000.f,15000.f,"Resistor (ohm)");
    configParam(LOAD_RESISTOR_PARAM,1000.f,10000.f,7500.f,"Load resistor (ohm)");
    configParam(INPUT_GAIN_CV_PARAM,0.f,1.f,0.f,"Input gain CV");
    configParam(DC_OFFSET_CV_PARAM,0.f,1.f,0.f,"Offset CV");
    configParam(OUTPUT_GAIN_CV_PARAM,0.f,1.f,0.f,"Output gain CV");
    configParam(RESISTOR_CV_PARAM,0.f,1.f,0.f,"Resistor CV");
    configParam(LOAD_RESISTOR_CV_PARAM,0.f,1.f,0.f,"Load resistor CV");
    configInput(INPUT_INPUT,"IN");
    configInput(INPUT_GAIN_INPUT,"Input Gain");
    configInput(DC_OFFSET_INPUT,"Offset");
    configInput(OUTPUT_GAIN_INPUT,"Output Gain");
    configInput(RESISTOR_INPUT,"Resistor");
    configInput(LOAD_RESISTOR_INPUT,"Load resistor");
    configOutput(OUTPUT_OUTPUT,"OUT");
    configBypass(INPUT_INPUT,OUTPUT_OUTPUT);

    maxExponent=std::nextafter(std::log(std::numeric_limits<float>::max()),0.f);
    maxValue=std::exp(maxExponent);
  }

  float_4 expMax(float_4 exponent) {
    return simd::ifelse(exponent<maxExponent,simd::exp(exponent),maxValue);
  }

  float_4 lw(float_4 in,int cc) {
    float_4 ret;
    for(int k=0;k<cc;k++) {
      ret[k]=float(fukushimaLW.lambertW0(in[k]));
    }
    return ret;
  }

  void process(const ProcessArgs &args) override {
    float inGainParam=params[INPUT_GAIN_PARAM].getValue();
    float offsetParam=params[DC_OFFSET_PARAM].getValue();
    float resistorParam=params[RESISTOR_PARAM].getValue();
    float loadResistorParam=params[LOAD_RESISTOR_PARAM].getValue();
    float outGainParam=params[OUTPUT_GAIN_PARAM].getValue();
    int channels=inputs[INPUT_INPUT].getChannels();
    for(int c=0;c<channels;c+=4) {
      float_4 in=inputs[INPUT_INPUT].getVoltageSimd<float_4>(c);
      float_4 inGain=simd::clamp(inGainParam+inputs[INPUT_GAIN_INPUT].getPolyVoltageSimd<float_4>(c)*params[INPUT_GAIN_CV_PARAM].getValue()*0.1f,0.f,1.f);
      float_4 offset=simd::clamp(offsetParam+inputs[DC_OFFSET_INPUT].getPolyVoltageSimd<float_4>(c)*params[DC_OFFSET_CV_PARAM].getValue(),-5.f,5.f);
      float_4 resistor=simd::clamp(resistorParam+inputs[RESISTOR_INPUT].getPolyVoltageSimd<float_4>(c)*params[RESISTOR_CV_PARAM].getValue()*5000.f,10000.f,100000.f);
      float_4 loadResistor=simd::clamp(loadResistorParam+inputs[LOAD_RESISTOR_INPUT].getPolyVoltageSimd<float_4>(c)*params[LOAD_RESISTOR_CV_PARAM].getValue()*500.f,1000.f,10000.f);
      float_4 outGain=simd::clamp(outGainParam+inputs[OUTPUT_GAIN_INPUT].getPolyVoltageSimd<float_4>(c)*params[OUTPUT_GAIN_CV_PARAM].getValue()*0.1f,0.f,1.f);
      float_4 alpha=loadResistor*2/resistor;
      float_4 beta=(resistor+loadResistor*2)/(thermalVoltage*resistor);
      float_4 delta=(loadResistor*saturationCurrent)/thermalVoltage;
      float_4 input=in*inGain+offset;
      float_4 theta=simd::ifelse(input>=0.f,1.f,-1.f);
      int ccs=std::min(4,channels-c);
      float_4 n=theta*thermalVoltage*lw(delta*expMax(theta*beta*input),ccs)-alpha*input;
      float_4 out=simd::ifelse(n>1.5f,1.f,simd::ifelse(n<-1.5f,-1.f,n-(4.f/27.f)*n*n*n));
      outputs[OUTPUT_OUTPUT].setVoltageSimd(dcb[c/4].process(out*outGain*5.f),c);
    }
    outputs[OUTPUT_OUTPUT].setChannels(channels);
  }
};


struct LWFWidget : ModuleWidget {
  LWFWidget(LWF *module) {
    setModule(module);
    setPanel(createPanel(asset::plugin(pluginInstance,"res/LWF.svg")));
    float x=1.9;
    float x2=11.9;
    float y=11;
    float y2=9;
    addParam(createParam<TrimbotWhite9>(mm2px(Vec(x,y)),module,LWF::INPUT_GAIN_PARAM));
    addParam(createParam<TrimbotWhite>(mm2px(Vec(x2,y2)),module,LWF::INPUT_GAIN_CV_PARAM));
    addInput(createInput<SmallPort>(mm2px(Vec(x2,y2+7)),module,LWF::INPUT_GAIN_INPUT));
    y+=22;
    y2+=22;
    addParam(createParam<TrimbotWhite9>(mm2px(Vec(x,y)),module,LWF::DC_OFFSET_PARAM));
    addParam(createParam<TrimbotWhite>(mm2px(Vec(x2,y2)),module,LWF::DC_OFFSET_CV_PARAM));
    addInput(createInput<SmallPort>(mm2px(Vec(x2,y2+7)),module,LWF::DC_OFFSET_INPUT));
    y+=22;
    y2+=22;
    addParam(createParam<TrimbotWhite9>(mm2px(Vec(x,y)),module,LWF::RESISTOR_PARAM));
    addParam(createParam<TrimbotWhite>(mm2px(Vec(x2,y2)),module,LWF::RESISTOR_CV_PARAM));
    addInput(createInput<SmallPort>(mm2px(Vec(x2,y2+7)),module,LWF::RESISTOR_INPUT));
    y+=22;
    y2+=22;
    addParam(createParam<TrimbotWhite9>(mm2px(Vec(x,y)),module,LWF::LOAD_RESISTOR_PARAM));
    addParam(createParam<TrimbotWhite>(mm2px(Vec(x2,y2)),module,LWF::LOAD_RESISTOR_CV_PARAM));
    addInput(createInput<SmallPort>(mm2px(Vec(x2,y2+7)),module,LWF::LOAD_RESISTOR_INPUT));
    y+=22;
    y2+=22;
    addParam(createParam<TrimbotWhite9>(mm2px(Vec(x,y)),module,LWF::OUTPUT_GAIN_PARAM));
    addParam(createParam<TrimbotWhite>(mm2px(Vec(x2,y2)),module,LWF::OUTPUT_GAIN_CV_PARAM));
    addInput(createInput<SmallPort>(mm2px(Vec(x2,y2+7)),module,LWF::OUTPUT_GAIN_INPUT));
    y=116;
    addInput(createInput<SmallPort>(mm2px(Vec(x,y)),module,LWF::INPUT_INPUT));
    addOutput(createOutput<SmallPort>(mm2px(Vec(x2,y)),module,LWF::OUTPUT_OUTPUT));
  }
};

Model *modelLWF=createModel<LWF,LWFWidget>("LWF");