#include "dcb.h"
/*
 * ported from csound clfilt Copyright (C) 2002 Erik Spjut
 */
using simd::float_4;
#define CL_LIM 8

template<typename T>
struct ChebyFilter2 {
  T xn[CL_LIM]={};
  T yn[CL_LIM]={};
  T xnm1[CL_LIM]={};
  T xnm2[CL_LIM]={};
  T ynm1[CL_LIM]={};
  T ynm2[CL_LIM]={};
  float alpha[CL_LIM]={};
  float beta[CL_LIM]={};
  float b0[CL_LIM]={};
  float b1[CL_LIM]={};
  float b2[CL_LIM]={};
  T a0[CL_LIM]={};
  T a1[CL_LIM]={};
  T a2[CL_LIM]={};
  int nsec=4;

  void init(bool highPass=false) {
    float eps=sqrtf(powf(10.0f,0.1f)-1.0f);
    float aleph=0.5f/nsec*logf(1.0f/eps+sqrtf(1.0f/eps/eps+1.0f));
    for(int m=0;m<=nsec-1;m++) {
      float bethe=M_PI*((m+0.5f)/(2.0f*nsec)+0.5f);
      alpha[m]=(sinhf(aleph)*cosf(bethe));
      beta[m]=(coshf(aleph)*sinf(bethe));
      if(m==0) { /* H0 and pole magnitudes */
        b0[m]=((alpha[m])*(alpha[m])+(beta[m])*(beta[m]))*1.0f/sqrtf(1.0f+eps*eps);
        b1[m]=(highPass?-1.f:1.f)*((alpha[m])*(alpha[m])+(beta[m])*(beta[m]))*2.0f/sqrtf(1.0f+eps*eps);
        b2[m]=((alpha[m])*(alpha[m])+(beta[m])*(beta[m]))*1.0f/sqrtf(1.0f+eps*eps);
      } else { /* pole magnitudes */
        b0[m]=((alpha[m])*(alpha[m])+(beta[m])*(beta[m]))*1.f;
        b1[m]=(highPass?-1.f:1.f)*((alpha[m])*(alpha[m])+(beta[m])*(beta[m]))*2.f;
        b2[m]=((alpha[m])*(alpha[m])+(beta[m])*(beta[m]))*1.f;
      }
    }
  }

  T process(T in,T freq,float piosr,bool hiPass=false) {
    T tanfpi=simd::tan(piosr*freq);
    T tanfpi2=tanfpi*tanfpi;
    T cotfpi=1.f/tanfpi;
    T cotfpi2=cotfpi*cotfpi;
    if(hiPass) {
      for(int m=0;m<=nsec-1;m++) {
        a0[m]=(alpha[m])*(alpha[m])+(beta[m])*(beta[m])+tanfpi*(tanfpi-2.0f*(alpha[m]));
        a1[m]=2.f*(tanfpi2-((alpha[m])*(alpha[m])+(beta[m])*(beta[m])));
        a2[m]=(alpha[m])*(alpha[m])+(beta[m])*(beta[m])+tanfpi*(tanfpi+2.f*(alpha[m]));
      }
    } else {
      for(int m=0;m<=nsec-1;m++) {
        a0[m]=(alpha[m])*(alpha[m])+(beta[m])*(beta[m])+cotfpi*(cotfpi-2.0f*(alpha[m]));
        a1[m]=2.f*((alpha[m])*(alpha[m])+(beta[m])*(beta[m])-cotfpi2);
        a2[m]=(alpha[m])*(alpha[m])+(beta[m])*(beta[m])+cotfpi*(cotfpi+2.f*(alpha[m]));
      }
    }
    xn[0]=in;
    for(int m=0;m<=nsec-1;m++) {
      yn[m]=(b0[m]*xn[m]+b1[m]*xnm1[m]+b2[m]*xnm2[m]-a1[m]*ynm1[m]-a2[m]*ynm2[m])/a0[m];
      xnm2[m]=xnm1[m];
      xnm1[m]=xn[m];
      ynm2[m]=ynm1[m];
      ynm1[m]=yn[m];
      xn[m+1]=yn[m];
    }
    T out=yn[nsec-1];
    return out;
  }
};

template<typename T>
struct ChebyFilter {
  // We only keep the required state histories
  T xnm1[CL_LIM] = {};
  T xnm2[CL_LIM] = {};
  T ynm1[CL_LIM] = {};
  T ynm2[CL_LIM] = {};

  // Precomputed Constants
  float b0[CL_LIM] = {};
  float b1[CL_LIM] = {};
  float b2[CL_LIM] = {};
  float mag2[CL_LIM] = {};   // Replaces alpha^2 + beta^2
  float alpha2[CL_LIM] = {}; // Replaces 2 * alpha

  int nsec = 4;

  void init(bool highPass = false) {
    float eps = sqrtf(powf(10.0f, 0.1f) - 1.0f);
    float aleph = 0.5f / nsec * logf(1.0f / eps + sqrtf(1.0f / eps / eps + 1.0f));

    for(int m = 0; m < nsec; m++) {
      float bethe = M_PI * ((m + 0.5f) / (2.0f * nsec) + 0.5f);
      float a = sinhf(aleph) * cosf(bethe);
      float b = coshf(aleph) * sinf(bethe);

      // Precompute the math that never changes!
      mag2[m] = (a * a) + (b * b);
      alpha2[m] = 2.0f * a;

      float denom = (m == 0) ? sqrtf(1.0f + eps * eps) : 1.0f;
      float mult = (m == 0 && highPass) ? -2.0f : (m == 0 ? 2.0f : (highPass ? -2.0f : 2.0f));

      b0[m] = mag2[m] / denom;
      b1[m] = (mag2[m] * mult) / denom;
      b2[m] = mag2[m] / denom;
    }
  }

  // Use a template parameter to eliminate the highpass branch at compile time!
  template<bool hiPass = false>
  T process(T in, T freq, float piosr) {
    T out = in; // 'out' cascades through the sections, replacing the xn and yn arrays

    // We compute tan once outside the loop
    T tanfpi = simd::tan(piosr * freq);

    if(hiPass) {
      T tanfpi2 = tanfpi * tanfpi;

      for(int m = 0; m < nsec; m++) {
        // Compute 'a' coefficients locally in registers
        T a0 = mag2[m] + tanfpi * (tanfpi - alpha2[m]);
        T a1 = 2.f * (tanfpi2 - mag2[m]);
        T a2 = mag2[m] + tanfpi * (tanfpi + alpha2[m]);

        // Invert a0 to use multiplication instead of division
        T inv_a0 = 1.f / a0;

        // Compute the difference equation
        T yn_val = (b0[m] * out + b1[m] * xnm1[m] + b2[m] * xnm2[m] - a1 * ynm1[m] - a2 * ynm2[m]) * inv_a0;

        // Shift states
        xnm2[m] = xnm1[m];
        xnm1[m] = out;
        ynm2[m] = ynm1[m];
        ynm1[m] = yn_val;

        out = yn_val; // Cascade to the next section
      }
    } else {
      T cotfpi = 1.f / tanfpi;
      T cotfpi2 = cotfpi * cotfpi;

      for(int m = 0; m < nsec; m++) {
        T a0 = mag2[m] + cotfpi * (cotfpi - alpha2[m]);
        T a1 = 2.f * (mag2[m] - cotfpi2);
        T a2 = mag2[m] + cotfpi * (cotfpi + alpha2[m]);

        T inv_a0 = 1.f / a0;

        T yn_val = (b0[m] * out + b1[m] * xnm1[m] + b2[m] * xnm2[m] - a1 * ynm1[m] - a2 * ynm2[m]) * inv_a0;

        xnm2[m] = xnm1[m];
        xnm1[m] = out;
        ynm2[m] = ynm1[m];
        ynm1[m] = yn_val;

        out = yn_val;
      }
    }

    return out;
  }
};




struct CHBY : Module {
  enum ParamId {
    FREQ_PARAM,FREQ_CV_PARAM,PARAMS_LEN
  };
  enum InputId {
    CV_INPUT,FREQ_INPUT,INPUTS_LEN
  };
  enum OutputId {
    LP_OUTPUT,HP_OUTPUT,OUTPUTS_LEN
  };
  enum LightId {
    LIGHTS_LEN
  };
  ChebyFilter<float_4> lpFilter[4];
  ChebyFilter<float_4> hpFilter[4];

  CHBY() {
    config(PARAMS_LEN,INPUTS_LEN,OUTPUTS_LEN,LIGHTS_LEN);
    configParam(FREQ_PARAM,3.f,14.f,14.f,"Frequency"," Hz",2,1);
    configOutput(LP_OUTPUT,"LowPass");
    configOutput(HP_OUTPUT,"HiPass");
    configInput(FREQ_INPUT,"Freq");
    configParam(FREQ_CV_PARAM,0,1,0,"Freq CV","%",0,100);

    for(int k=0;k<4;k++) {
      lpFilter[k].init();
      hpFilter[k].init(true);
    }
    configBypass(CV_INPUT,LP_OUTPUT);
    configBypass(CV_INPUT,HP_OUTPUT);
  }

  void process(const ProcessArgs &args) override {
    float piosr=M_PI/APP->engine->getSampleRate();
    float freqParam=params[FREQ_PARAM].getValue();
    float freqCvParam=params[FREQ_CV_PARAM].getValue();
    int channels=inputs[CV_INPUT].getChannels();
    bool lpConnected=outputs[LP_OUTPUT].isConnected();
    bool hpConnected=outputs[HP_OUTPUT].isConnected();
    for(int c=0;c<channels;c+=4) {
      float_4 pitch=freqParam+inputs[FREQ_INPUT].getPolyVoltageSimd<float_4>(c)*freqCvParam;
      float_4 cutoff=simd::clamp(simd::pow(2.f,pitch),2.f,args.sampleRate*0.33f);
      float_4 in=inputs[CV_INPUT].getVoltageSimd<float_4>(c);
      if(hpConnected) {
        float_4 hpOut=hpFilter[c/4].process<true>(in,cutoff,piosr);
        outputs[HP_OUTPUT].setVoltageSimd(hpOut,c);
      }
      if(lpConnected) {
        float_4 out=lpFilter[c/4].process(in,cutoff,piosr);
        outputs[LP_OUTPUT].setVoltageSimd(out,c);
      }
    }
    if(lpConnected)
      outputs[LP_OUTPUT].setChannels(channels);
    if(hpConnected)
      outputs[HP_OUTPUT].setChannels(channels);
  }
};


struct CHBYWidget : ModuleWidget {
  CHBYWidget(CHBY *module) {
    setModule(module);
    setPanel(createPanel(asset::plugin(pluginInstance,"res/CHBY.svg")));

    float y=9;
    float x=1.9;
    addParam(createParam<TrimbotWhite>(mm2px(Vec(x,y)),module,CHBY::FREQ_PARAM));
    y+=7;
    addInput(createInput<SmallPort>(mm2px(Vec(x,y)),module,CHBY::FREQ_INPUT));
    y+=7;
    addParam(createParam<TrimbotWhite>(mm2px(Vec(x,y)),module,CHBY::FREQ_CV_PARAM));

    y=92;
    addInput(createInput<SmallPort>(mm2px(Vec(x,y)),module,CHBY::CV_INPUT));
    y+=12;
    addOutput(createOutput<SmallPort>(mm2px(Vec(x,y)),module,CHBY::HP_OUTPUT));
    y+=12;
    addOutput(createOutput<SmallPort>(mm2px(Vec(x,y)),module,CHBY::LP_OUTPUT));
  }
};


Model *modelCHBY=createModel<CHBY,CHBYWidget>("CHBY");