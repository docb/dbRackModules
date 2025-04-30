#include "dcb.h"

using simd::float_4;
using simd::int32_4;

template<typename T>
struct SinPOsc {
  T phs=0.f;
  float g1=TWOPIF-5.f;
  float g2=M_PI-3;

  T sin2pi_pade_05_5_4(T x) {
    x-=0.5f;
    T x2=x*x;
    T x3=x2*x;
    T x4=x2*x2;
    T x5=x2*x3;
    return (T(-6.283185307)*x+T(33.19863968)*x3-T(32.44191367)*x5)/
           (1+T(1.296008659)*x2+T(0.7028072946)*x4);
  }

  T process5P() {
    T po=phs*4.f;
    T mask=phs<0.5f;
    T x=simd::ifelse(mask,po,po-2.f)-1.f;
    T x2=x*x;
    T y=0.5f*(x*(M_PI-x2*(g1-x2*g2)));
    return simd::ifelse(mask,y,-y);
  }

  void updatePhs(float sampleTime,T freq) {
    phs+=simd::fmin(freq*sampleTime,0.5f);
    phs-=simd::floor(phs);
  }

  T process(float sampleTime,T freq) {
    updatePhs(sampleTime,freq);
    //return sin2pi_pade_05_5_4(phs);
    return process5P();
  }

  void reset(T trigger) {
    phs=simd::ifelse(trigger,0.f,phs);
  }
};

#define MAXPARTIALS 16

template<typename T>
struct ParaSinOscBank {
  SinPOsc<T> partials[MAXPARTIALS];

  float powf_fast_ub(float a,float b) {
    union {
      float d;
      int x;
    } u={a};
    u.x=(int)(b*(u.x-1064631197)+1065353217);
    return u.d;
  }

  float_4 fastPow(float a,float_4 b) {
    float r[4]={};
    for(int k=0;k<4;k++) {
      r[k]=powf_fast_ub(a,b[k]);
    }
    return {r[0],r[1],r[2],r[3]};
  }

  float fastPow(float a,float b) {
    return powf_fast_ub(a,b);
  }

  T process(float sampleRate,T baseFreq,int numPartials,T decay,T oddEven,int offset,T stretch) {
    float sampleTime=1.f/sampleRate;
    T ret=0.f;
    for(int k=0;k<numPartials;k++) {
      T dmp=1.f/fastPow(k+1,decay);
      if(k%2==0) {
        dmp*=simd::ifelse(oddEven>0.f,1.0f-oddEven,1.f);
      } else {
        dmp*=simd::ifelse(oddEven<0.f,1.0f+oddEven,1.f);
      }
      T freq=baseFreq*float((k+1)+offset);
      if(k>0) {
        freq*=simd::ifelse(stretch!=0.f,fastPow(float(k),stretch),1.f);
      }
      ret+=simd::ifelse(freq*3<sampleRate,partials[k].process(sampleTime,freq),0.f)*dmp;
    }
    return ret;
  }
};

struct Osc8 : Module {
  enum ParamId {
    FM_PARAM,
    FM_2_PARAM,
    NUM_PARTIALS_PARAM,
    NUM_PARTIALS_2_PARAM,
    DECAY_PARAM,
    DECAY_2_PARAM,
    ODD_EVEN_PARAM,
    ODD_EVEN_2_PARAM,
    OFFSET_PARAM,
    OFFSET_2_PARAM,
    STRETCH_PARAM,
    STRETCH_2_PARAM,
    RATIO_PARAM,
    RATIO_2_PARAM,
    FINE_PARAM,
    FINE_2_PARAM,
    FM_CV_PARAM,
    FM_CV_2_PARAM,
    DECAY_CV_PARAM,
    DECAY_CV_2_PARAM,
    ODD_EVEN_CV_PARAM,
    ODD_EVEN_CV_2_PARAM,
    STRETCH_CV_PARAM,
    STRETCH_CV_2_PARAM,
    FREQ_PARAM,
    PARAMS_LEN
  };
  enum InputId {
    FM_INPUT,
    FM_2_INPUT,
    DECAY_INPUT,
    DECAY_2_INPUT,
    ODD_EVEN_INPUT,
    ODD_EVEN_2_INPUT,
    STRETCH_INPUT,
    STRETCH_2_INPUT,
    RATIO_INPUT,
    RATIO_2_INPUT,
    FINE_INPUT,
    FINE_2_INPUT,
    NUM_INPUT,
    NUM_2_INPUT,
    OFFSET_INPUT,
    OFFSET_2_INPUT,
    VOCT_INPUT,
    INPUTS_LEN
  };
  enum OutputId {
    OUTPUT,OUTPUT_2,OUTPUTS_LEN
  };
  enum LightId {
    LIGHTS_LEN
  };

  ParaSinOscBank<float_4> oscBank1[4];
  ParaSinOscBank<float_4> oscBank2[4];
  float_4 out2[4]={};
  DCBlocker<float_4> dcb[4];
  DCBlocker<float_4> dcb2[4];
  dsp::ClockDivider divider;

  Osc8() {
    config(PARAMS_LEN,INPUTS_LEN,OUTPUTS_LEN,LIGHTS_LEN);
    configInput(VOCT_INPUT,"V/Oct");
    for(int k=0;k<2;k++) {
      std::string strNr=std::to_string(k+1);

      configParam(NUM_PARTIALS_PARAM+k,1,MAXPARTIALS,6,"Num Partials "+strNr);
      getParamQuantity(NUM_PARTIALS_PARAM+k)->snapEnabled=true;
      configInput(NUM_INPUT+k,"Num Partials "+strNr);

      configParam(OFFSET_PARAM+k,0,24,0,"First Partial "+strNr);
      getParamQuantity(OFFSET_PARAM+k)->snapEnabled=true;
      configInput(OFFSET_INPUT+k,"First Partial "+strNr);

      configParam(ODD_EVEN_PARAM+k,-1,1,0,"Odd/Even "+strNr);
      configParam(ODD_EVEN_CV_PARAM+k,0,1,0,"Odd/Even CV "+strNr,"%",0,100);
      configInput(ODD_EVEN_INPUT+k,"Odd/Even "+strNr);

      configParam(DECAY_PARAM+k,0.1,3,1,"Decay "+strNr);
      configParam(DECAY_CV_PARAM+k,0,1,0,"Decay CV "+strNr,"%",0,100);
      configInput(DECAY_INPUT+k,"Decay "+strNr);

      configParam(STRETCH_PARAM+k,-1.25,1.25,0,"Stretch "+strNr);
      configInput(STRETCH_INPUT+k,"Stretch "+strNr);
      configParam(STRETCH_CV_PARAM+k,0,1,0,"Stretch CV "+strNr,"%",0,100);

      configParam(FM_CV_PARAM+k,0,1,0,"FM CV "+strNr,"%",0,100);
      configParam(FM_PARAM+k,0,3,0,"FM Amount "+strNr,"%",0,100);
      configInput(FM_INPUT+k,"FM CV "+strNr);

      configParam(RATIO_PARAM+k,-32,32,1,"Ratio "+strNr);
      getParamQuantity(RATIO_PARAM+k)->snapEnabled=true;
      configInput(RATIO_INPUT+k,"Ratio "+strNr);

      configParam(FINE_PARAM+k,-1,1,0,"Fine "+strNr);
      configInput(FINE_INPUT+k,"Fine "+strNr);

      configOutput(OUTPUT+k,"Out "+strNr);
    }
    divider.setDivision(32);
  }

  float getFreq(int k) {
    float ratio=params[RATIO_PARAM+k].getValue();
    float fine=params[FINE_PARAM+k].getValue();
    if(ratio<1) {
      fine=clamp(fine,0.f,1.f);
      ratio+=fine;
      ratio-=2;
      ratio=1.f/fabsf(ratio);
    } else {
      ratio+=fine;
    }
    return ratio;
  }

  void process(const ProcessArgs &args) override {
    if(divider.process()) {
      for(int k=0;k<2;k++) {
        if(inputs[RATIO_INPUT+k].isConnected()) {
          setImmediateValue(getParamQuantity(RATIO_PARAM+k),inputs[RATIO_INPUT+k].getVoltage()*3.2f);
        }
        if(inputs[FINE_INPUT+k].isConnected()) {
          setImmediateValue(getParamQuantity(FINE_PARAM+k),inputs[FINE_INPUT+k].getVoltage()*0.1);
        }
        if(inputs[NUM_INPUT+k].isConnected()) {
          setImmediateValue(getParamQuantity(NUM_PARTIALS_PARAM+k),
                            inputs[NUM_INPUT+k].getVoltage()*1.6f);
        }
        if(inputs[OFFSET_INPUT+k].isConnected()) {
          setImmediateValue(getParamQuantity(OFFSET_PARAM+k),inputs[OFFSET_INPUT+k].getVoltage()*1.6f);
        }
      }
    }
    float freq1=getFreq(0);
    float freq2=getFreq(1);


    float fmParam1=params[FM_PARAM].getValue();
    float fmParamCV1=params[FM_CV_PARAM].getValue();
    float fmParam2=params[FM_2_PARAM].getValue();
    float fmParamCV2=params[FM_CV_2_PARAM].getValue();

    //bool linear=params[LIN_PARAM].getValue()>0;
    int numPartials1=params[NUM_PARTIALS_PARAM].getValue();
    int offset1=params[OFFSET_PARAM].getValue();
    float decay1=params[DECAY_PARAM].getValue();
    float decayCV1=params[DECAY_CV_PARAM].getValue();
    float stretch1=params[STRETCH_PARAM].getValue();
    float stretchCV1=params[STRETCH_CV_PARAM].getValue();
    float oddEven1=params[ODD_EVEN_PARAM].getValue();
    float oddEvenCV1=params[ODD_EVEN_CV_PARAM].getValue();

    int numPartials2=params[NUM_PARTIALS_2_PARAM].getValue();
    int offset2=params[OFFSET_2_PARAM].getValue();
    float decay2=params[DECAY_2_PARAM].getValue();
    float decayCV2=params[DECAY_CV_2_PARAM].getValue();
    float stretch2=params[STRETCH_2_PARAM].getValue();
    float stretchCV2=params[STRETCH_CV_2_PARAM].getValue();
    float oddEven2=params[ODD_EVEN_2_PARAM].getValue();
    float oddEvenCV2=params[ODD_EVEN_CV_2_PARAM].getValue();

    int channels=std::max(inputs[VOCT_INPUT].getChannels(),1);
    for(int c=0;c<channels;c+=4) {
      auto *oscil1=&oscBank1[c/4];
      auto *oscil2=&oscBank2[c/4];
      float_4 pitch1=simd::log2(freq1)+inputs[VOCT_INPUT].getVoltageSimd<float_4>(c);
      float_4 freq1;
      float_4 fmAmount1=fmParam1+inputs[FM_INPUT].getPolyVoltageSimd<float_4>(c)*fmParamCV1;
      freq1=dsp::FREQ_C4*dsp::approxExp2_taylor5(pitch1+30.f)/std::pow(2.f,30.f);
      freq1+=dsp::FREQ_C4*out2[c/4]*fmAmount1;
      freq1=simd::fmin(freq1,args.sampleRate/2);
      float_4 decay4=simd::clamp(decay1+inputs[DECAY_INPUT].getPolyVoltageSimd<float_4>(c)*decayCV1,0.1f,
                                 3.f);
      float_4 oddEven4=simd::clamp(
        oddEven1+inputs[ODD_EVEN_INPUT].getPolyVoltageSimd<float_4>(c)*oddEvenCV1*0.1,-1.f,1.f);
      float_4 stretch4=simd::clamp(
        stretch1+inputs[STRETCH_INPUT].getPolyVoltageSimd<float_4>(c)*stretchCV1*0.1f,-1.25f,1.25f);

      float_4 out=oscil1->process(args.sampleRate,freq1,numPartials1,1.f/decay4,oddEven4,offset1,
                                  stretch4);
      out=dcb[c/4].process(out*3.f);
      outputs[OUTPUT].setVoltageSimd(out,c);

      float_4 pitch2=simd::log2(freq2)+inputs[VOCT_INPUT].getVoltageSimd<float_4>(c);
      float_4 freq2;
      float_4 fmAmount2=fmParam2+inputs[FM_2_INPUT].getPolyVoltageSimd<float_4>(c)*fmParamCV2;
      freq2=dsp::FREQ_C4*dsp::approxExp2_taylor5(pitch2+30.f)/std::pow(2.f,30.f);
      freq2+=dsp::FREQ_C4*out*fmAmount2;
      freq2=simd::fmin(freq2,args.sampleRate/2);
      float_4 decay42=simd::clamp(decay2+inputs[DECAY_2_INPUT].getPolyVoltageSimd<float_4>(c)*decayCV2,
                                  0.1f,3.f);
      float_4 oddEven42=simd::clamp(
        oddEven2+inputs[ODD_EVEN_2_INPUT].getPolyVoltageSimd<float_4>(c)*oddEvenCV2*0.1,-1.f,1.f);
      float_4 stretch42=simd::clamp(
        stretch2+inputs[STRETCH_2_INPUT].getPolyVoltageSimd<float_4>(c)*stretchCV2*0.1f,-1.25f,
        1.25f);

      out2[c/4]=oscil2->process(args.sampleRate,freq2,numPartials2,1.f/decay42,oddEven42,offset2,
                                stretch42);
      out2[c/4]=dcb2[c/4].process(out2[c/4]*3.f);
      outputs[OUTPUT_2].setVoltageSimd(out2[c/4],c);


    }
    outputs[OUTPUT].setChannels(channels);
    outputs[OUTPUT_2].setChannels(channels);
  }
};


struct Osc8Widget : ModuleWidget {
  Osc8Widget(Osc8 *module) {
    setModule(module);
    setPanel(createPanel(asset::plugin(pluginInstance,"res/Osc8.svg")));

    float x=6;
    float x2=16;
    float ofs=23;
    float x4=39;
    for(int k=0;k<2;k++) {
      float y=14;
      addParam(createParam<TrimbotWhite>(mm2px(Vec(x+k*ofs,y)),module,Osc8::RATIO_PARAM+k));
      addInput(createInput<SmallPort>(mm2px(Vec(x+k*ofs,y+8)),module,Osc8::RATIO_INPUT+k));
      addParam(createParam<TrimbotWhite>(mm2px(Vec(x2+k*ofs,y)),module,Osc8::FINE_PARAM+k));
      addInput(createInput<SmallPort>(mm2px(Vec(x2+k*ofs,y+8)),module,Osc8::FINE_INPUT+k));
      y+=22;
      addParam(createParam<TrimbotWhite>(mm2px(Vec(x+k*ofs,y)),module,Osc8::NUM_PARTIALS_PARAM+k));
      addInput(createInput<SmallPort>(mm2px(Vec(x+k*ofs,y+8)),module,Osc8::NUM_INPUT+k));
      addParam(createParam<TrimbotWhite>(mm2px(Vec(x2+k*ofs,y)),module,Osc8::OFFSET_PARAM+k));
      addInput(createInput<SmallPort>(mm2px(Vec(x2+k*ofs,y+8)),module,Osc8::OFFSET_INPUT+k));

      y=58;
      addParam(createParam<TrimbotWhite>(mm2px(Vec(x+k*ofs,y)),module,Osc8::DECAY_PARAM+k));
      addInput(createInput<SmallPort>(mm2px(Vec(x+k*ofs,y+8)),module,Osc8::DECAY_INPUT+k));
      addParam(createParam<TrimbotWhite>(mm2px(Vec(x+k*ofs,y+16)),module,Osc8::DECAY_CV_PARAM+k));

      addParam(createParam<TrimbotWhite>(mm2px(Vec(x2+k*ofs,y)),module,Osc8::ODD_EVEN_PARAM+k));
      addInput(createInput<SmallPort>(mm2px(Vec(x2+k*ofs,y+8)),module,Osc8::ODD_EVEN_INPUT+k));
      addParam(createParam<TrimbotWhite>(mm2px(Vec(x2+k*ofs,y+16)),module,Osc8::ODD_EVEN_CV_PARAM+k));
      y=88;

      addParam(createParam<TrimbotWhite>(mm2px(Vec(x+k*ofs,y)),module,Osc8::STRETCH_PARAM+k));
      addInput(createInput<SmallPort>(mm2px(Vec(x+k*ofs,y+8)),module,Osc8::STRETCH_INPUT+k));
      addParam(createParam<TrimbotWhite>(mm2px(Vec(x+k*ofs,y+16)),module,Osc8::STRETCH_CV_PARAM+k));

      addParam(createParam<TrimbotWhite>(mm2px(Vec(x2+k*ofs,y)),module,Osc8::FM_PARAM+k));
      addInput(createInput<SmallPort>(mm2px(Vec(x2+k*ofs,y+8)),module,Osc8::FM_INPUT+k));
      addParam(createParam<TrimbotWhite>(mm2px(Vec(x2+k*ofs,y+16)),module,Osc8::FM_CV_PARAM+k));

    }

    addOutput(createOutput<SmallPort>(mm2px(Vec(x,116)),module,Osc8::OUTPUT));
    addOutput(createOutput<SmallPort>(mm2px(Vec(x4,116)),module,Osc8::OUTPUT_2));
    addInput(createInput<SmallPort>(mm2px(Vec(22.5,116)),module,Osc8::VOCT_INPUT));
  }
};


Model *modelOsc8=createModel<Osc8,Osc8Widget>("Osc8");