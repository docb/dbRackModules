#include "dcb.h"
#include "filter.hpp"

using simd::float_4;
using simd::int32_4;
#define NUM_OSC 3

struct Q3 : Module {
  enum ParamId {
    ENUMS(OCT_PARAM, NUM_OSC),
    ENUMS(TUNE_PARAM, NUM_OSC),
    ENUMS(PW_PARAM, NUM_OSC),
    ENUMS(LEVEL_PARAM, NUM_OSC),
    TT_LEVEL_PARAM,
    ENUMS(LIN_PARAM, NUM_OSC),
    ENUMS(FM_A_PARAM, NUM_OSC),
    ENUMS(FM_B_PARAM, NUM_OSC),
    ENUMS(FM_C_PARAM, NUM_OSC),
    ENUMS(FM_TT_PARAM, NUM_OSC),
    ENUMS(RND_PARAM, NUM_OSC),
    ENGINE_PARAM,
    PARAMS_LEN
  };

  enum InputId {
    ENUMS(FM_MOD_INPUT, NUM_OSC),
    ENUMS(V_OCT_INPUT, NUM_OSC),
    ENUMS(LEVEL_INPUT, NUM_OSC),
    TT_LEVEL_INPUT,
    ENUMS(PWM_INPUT, NUM_OSC),
    TT_INPUT, RST_INPUT, INPUTS_LEN
  };

  enum OutputId {
    CV_OUTPUT, A_OUTPUT, OUTPUTS_LEN=A_OUTPUT+4
  };

  enum LightId {
    LIGHTS_LEN
  };

  constexpr static float inv16=1.f/16.f;
  constexpr static float inv2_30=1.f/1073741824.f;

  unsigned acc[3][16]={};
  unsigned max[3][16]={};
  float_4 phs[3][4]={};
  float_4 osc[4][4]={};
  DCBlocker<float_4> dcBlockerSimd[3][4];
  DCBlocker<float_4> outDcBlockerSimd[4];
  uint16_t gatePattern[3]={};
  int gateCount[3]={};
  bool oscB[4][16]={};
  DCBlocker<float> dcBlocker[3][16];
  DCBlocker<float> outDcBlocker[16];
  bool lpFM=true;
  std::string lbl[4]={"A", "B", "C"};
  Cheby1_32_BandFilter<float_4> filter[4];
  bool b[8]={false, true, true, false, true, false, false, true};
  RND rnd;
  float selfLP[4][16]={};
  dsp::SchmittTrigger rstTrigger;

  Q3() {
    config(PARAMS_LEN, INPUTS_LEN, OUTPUTS_LEN, LIGHTS_LEN);
    for(int k=0; k<3; k++) {
      configParam(OCT_PARAM+k, -6, 4, 0, "Octaves "+lbl[k]);
      getParamQuantity(OCT_PARAM+k)->snapEnabled=true;
      configParam(TUNE_PARAM+k, -7, 7, 0, "Tune "+lbl[k], " Semitone");
      configParam(PW_PARAM+k, 0, 1, 0.5, "Pwm "+lbl[k]);
      configParam(LEVEL_PARAM+k, -1, 1, 0., "Level "+lbl[k]);
      configParam(LIN_PARAM+k, 0, 1, 0, "Linear FM "+lbl[k]);
      configParam(FM_A_PARAM+k, -4, 4, 0, "FM_A -> "+lbl[k]);
      configParam(FM_B_PARAM+k, -4, 4, 0, "FM_B -> "+lbl[k]);
      configParam(FM_C_PARAM+k, -4, 4, 0, "FM_C -> "+lbl[k]);
      configParam(FM_TT_PARAM+k, -4, 4, 0, "FM_TT -> "+lbl[k]);
      configParam(RND_PARAM+k, 0, 1, 0, "RND Seed "+lbl[k]);
      configInput(FM_MOD_INPUT+k, "FM "+lbl[k]);
      configInput(V_OCT_INPUT+k, "V/Oct "+lbl[k]);
      configInput(LEVEL_INPUT+k, "Level "+lbl[k]);
      configInput(PWM_INPUT+k, "PWM "+lbl[k]);
      configOutput(A_OUTPUT+k, "Gate "+lbl[k]);
    }
    configInput(RST_INPUT, "Rst");
    configInput(TT_INPUT, "TT");
    configInput(TT_LEVEL_INPUT, "TT Level");
    configParam(TT_LEVEL_PARAM, -1, 1, 0, "TT Level");
    configOutput(CV_OUTPUT, "Mix");
    configOutput(A_OUTPUT+3, "Gate TT");
    configSwitch(ENGINE_PARAM, 0, 3, 1, "Engine", {
                 "DivideN (Low CPU)", "Accurate (Normal CPU)", "HIFI (High CPU)", "Aliasing (Low CPU)"
                 });
  }

  void onReset() override {
    Module::onReset();
    for(int k=0; k<3; k++) {
      getParamQuantity(OCT_PARAM+k)->setImmediateValue(static_cast<float>(k)-2.f);
    }
  }

  float_4 getMixSIMD(int chn) {
    float_4 mix=0.f;
    for(int k=0; k<4; k++)
      mix+=(inputs[LEVEL_INPUT+k].getVoltageSimd<float_4>(chn)*0.1f+params[LEVEL_PARAM+k].getValue())*(osc[k][chn/4]-
        0.5f);
    return mix;
  }

  void refreshGatePattern(int k) {
    float r=params[RND_PARAM+k].getValue();
    auto seedInput=(uint64_t)(floorf(r*static_cast<float>(ULONG_MAX)));
    INFO("seed=%ld r=%f", seedInput, r);
    float pw_val=clamp(params[PW_PARAM+k].getValue()+inputs[PWM_INPUT+k].getVoltage());
    int gates=clamp(static_cast<int>(std::floor(4.f+12.f*pw_val)), 4, 16);
    gateCount[k]=gates;
    rnd.reset(seedInput);
    uint16_t pat=0;
    for(int i=0; i<gates; i++) {
      if(rnd.nextCoin(0.5f)) {
        pat|=(1<<i);
      }
    }
    gatePattern[k]=pat;
  }

  void process(const ProcessArgs& args) override {
    if(rstTrigger.process(inputs[RST_INPUT].getVoltage())) {
      for(int k=0; k<3; k++) {
        for(int j=0; j<16; j++) {
          acc[k][j]=0;
        }
        for(int j=0; j<4; j++) {
          phs[k][j]=0.f;
        }
      }
    }
    if(inputs[TT_INPUT].isConnected()) {
      int channels=std::min(inputs[TT_INPUT].getChannels(), 8);
      for(int k=0; k<channels; k++) {
        b[k]=inputs[TT_INPUT].getVoltage(k)>1.f;
      }
    }
    int engine=static_cast<int>(params[ENGINE_PARAM].getValue());
    switch(engine) {
    case 0: default: processDivN(args);
      break;
    case 1: processAccurate(args);
      break;
    case 2: processHIFI(args);
      break;
    case 3: processAlias(args);
      break;
    }
  }

  static float scaleExpGeneric(float x, float xMax, float yMax) {
    float absX=std::abs(x);
    float scale=yMax/(dsp::approxExp2_taylor5(xMax+30.f)*(1.f/1073741824.f)-1.0f);
    float val=scale*(dsp::approxExp2_taylor5(absX+30.f)*(1.f/1073741824.f)-1.0f);
    return std::copysign(val, x);
  }

  int32_4 computeTT(int g) {
    auto b0=int32_4(osc[0][g]);
    auto b1=int32_4(osc[1][g]);
    auto b2=int32_4(osc[2][g]);
    int32_4 idx=b0|(b1<<1)|(b2<<2);

    int32_4 tt_mask=0;
    for(int i=0; i<8; i++) {
      if(b[i]) {
        tt_mask|=(idx==i);
      }
    }
    return tt_mask;
  }

  //===================== Sample-Rate FM & TT (16x Oversampled) =====================
  void processAccurate(const ProcessArgs& args) {
    float R_1x=1.f-(2.f*M_PI*10.f/args.sampleRate);
    float oversampledRate=args.sampleRate*16.f;
    float R_16x=1.f-(2.f*M_PI*10.f/oversampledRate);

    float linFmFactor=(dsp::FREQ_C4*args.sampleTime)*inv16;
    float maxNyquistInc=(args.sampleRate*0.5f*args.sampleTime)*inv16;

    int channels=std::max({
      inputs[V_OCT_INPUT].getChannels(),
      inputs[V_OCT_INPUT+1].getChannels(),
      inputs[V_OCT_INPUT+2].getChannels(),
      1
      });

    for(int c=0; c<channels; c+=4) {
      int g=c/4;
      float_4 fms[3];
      float_4 pw[3];
      bool randomWave[3];
      float_4 level[4];

      for(int k=0; k<4; k++) {
        level[k]=inputs[LEVEL_INPUT+k].getVoltageSimd<float_4>(c)*0.1f+params[LEVEL_PARAM+k].getValue();
      }

      auto tt_mask=computeTT(g);
      osc[3][g]=simd::ifelse(tt_mask, 1.f, 0.f);

      // Compute Phase Increments (fms)
      for(int k=0; k<3; k++) {
        bool linear=params[LIN_PARAM+k].getValue()>0.f;
        float gain=linear?4.f:2.f;
        float pch=params[OCT_PARAM+k].getValue()+params[TUNE_PARAM+k].getValue()/12.f;

        float_4 fm=0.f;
        for(int j=0; j<4; j++) {
          float fmVal=params[FM_A_PARAM+j*3+k].getValue()+inputs[FM_MOD_INPUT+k].getVoltage(j);
          if(fmVal!=0.f) {
            float scaled=scaleExpGeneric(fmVal/(linear?1.f:2.f), -gain, gain);
            fm+=(osc[j][g]-0.5f)*scaled;
          }
        }

        fm=dcBlockerSimd[k][g].process(fm, R_1x);

        float_4 pitch=pch+inputs[V_OCT_INPUT+k].getPolyVoltageSimd<simd::float_4>(c);
        float_4 baseFreq=dsp::FREQ_C4*dsp::approxExp2_taylor5(pitch+30.f)*inv2_30;

        if(linear) {
          fms[k]=(baseFreq*args.sampleTime)*inv16+float_4(linFmFactor)*fm;
        } else {
          fms[k]=(baseFreq*dsp::approxExp2_taylor5(fm+30.f)*inv2_30*args.sampleTime)*inv16;
        }
        fms[k]=simd::fmin(fms[k], float_4(maxNyquistInc));

        pw[k]=clamp(params[PW_PARAM+k].getValue()+inputs[PWM_INPUT+k].getVoltageSimd<float_4>(c)*0.1f);
        randomWave[k]=params[RND_PARAM+k].getValue()>0.f;
      }

      // 16x Oversampling Loop
      float_4 mix=0.f;
      for(int step=0; step<16; step++) {
        for(int k=0; k<3; k++) {
          phs[k][g]+=fms[k];
          phs[k][g]-=simd::floor(phs[k][g]);

          if(randomWave[k]) {
            int32_4 gate_idx=int32_4(phs[k][g]*static_cast<float>(gateCount[k]));
            uint16_t pat=gatePattern[k];
            float_4 gate_out;
            gate_out[0]=((pat>>gate_idx[0])&1)?1.f:0.f;
            gate_out[1]=((pat>>gate_idx[1])&1)?1.f:0.f;
            gate_out[2]=((pat>>gate_idx[2])&1)?1.f:0.f;
            gate_out[3]=((pat>>gate_idx[3])&1)?1.f:0.f;
            osc[k][g]=gate_out;
          } else {
            osc[k][g]=simd::ifelse(phs[k][g]<pw[k], 1.f, 0.f);
          }
        }

        mix=0.f;
        for(int k=0; k<4; k++) {
          mix+=level[k]*(osc[k][g]-0.5f);
        }

        mix=filter[g].process(mix);
      }

      mix=outDcBlockerSimd[g].process(mix, R_16x);
      outputs[CV_OUTPUT].setVoltageSimd(mix*5.f, c);
      for(int k=0; k<4; k++) {
        outputs[A_OUTPUT+k].setVoltageSimd(osc[k][g]*10.f, c);
      }
    }
    for(int k=0; k<OUTPUTS_LEN; k++) {
      outputs[k].setChannels(channels);
    }
  }

  //===================== accurate with oversampling and sub sample fm =====================
  void processHIFI(const ProcessArgs& args) {
    float oversampledRate=args.sampleRate*16.f;
    float R=1.f-(2.f*M_PI*10.f/oversampledRate);
    int channels=std::max({
      inputs[V_OCT_INPUT].getChannels(),
      inputs[V_OCT_INPUT+1].getChannels(),
      inputs[V_OCT_INPUT+2].getChannels(),
      1
      });

    for(int c=0; c<channels; c+=4) {
      int g=c/4;
      float_4 mix=0.f;
      float_4 baseFreq[3];
      float fmParam[12];
      bool linear[3];

      for(int k=0; k<3; k++) {
        linear[k]=params[LIN_PARAM+k].getValue()>0.f;
        float gain=linear[k]?4.f:2.f;
        float pch=params[OCT_PARAM+k].getValue()+params[TUNE_PARAM+k].getValue()/12.f;
        for(int j=0; j<4; j++) {
          float fm=params[FM_A_PARAM+j*3+k].getValue()+inputs[FM_MOD_INPUT+k].getVoltage(j);
          if(fm!=0) {
            fmParam[k*4+j]=scaleExpGeneric(fm/(linear[k]?1.f:2.f), -gain, gain);
          } else
            fmParam[k*4+j]=0;
        }

        float_4 pitch=pch+inputs[V_OCT_INPUT+k].getPolyVoltageSimd<simd::float_4>(c);
        baseFreq[k]=dsp::FREQ_C4*dsp::approxExp2_taylor5(pitch+30.f)*inv2_30;
      }

      for(int step=0; step<16; step++) {
        for(int k=0; k<3; k++) {
          bool randomWave=params[RND_PARAM+k].getValue()>0;
          float_4 pw=clamp(params[PW_PARAM+k].getValue()+inputs[PWM_INPUT+k].getVoltageSimd<float_4>(c)*float_4(0.1f));
          float_4 fm=0;
          for(int j=0; j<4; j++) {
            if(fmParam[k*4+j]!=0.f)
              fm+=(osc[j][g]-0.5f)*float_4(fmParam[k*4+j]);
          }
          fm=dcBlockerSimd[k][g].process(fm, R);
          float_4 freq;
          if(linear[k]) {
            freq=baseFreq[k]+dsp::FREQ_C4*fm;
          } else {
            freq=baseFreq[k]*dsp::approxExp2_taylor5(fm+30.f)*inv2_30;
          }

          float_4 fms=(simd::fmin(freq, args.sampleRate*0.5f)*args.sampleTime)*(1.f/16.f);
          phs[k][g]+=fms;
          phs[k][g]-=simd::floor(phs[k][g]);

          if(randomWave) {
            auto gate_idx=int32_4(phs[k][g]*static_cast<float>(gateCount[k]));
            int32_4 gate_mask=0;
            uint16_t pat=gatePattern[k];
            for(int i=0; i<gateCount[k]; i++) {
              if((pat>>i)&1) {
                gate_mask|=(gate_idx==i);
              }
            }
            osc[k][g]=simd::ifelse(gate_mask, 1.f, 0.f);
          } else {
            osc[k][g]=simd::ifelse(phs[k][g]<pw, 1.f, 0.f);
          }
        }

        auto tt_mask=computeTT(g);
        osc[3][g]=simd::ifelse(tt_mask, 1.f, 0.f);

        mix=getMixSIMD(c);
        mix=filter[g].process(mix);
      }
      mix=outDcBlockerSimd[g].process(mix, R);
      outputs[CV_OUTPUT].setVoltageSimd(mix*5.f, c);
      for(int k=0; k<4; k++) {
        outputs[A_OUTPUT+k].setVoltageSimd(osc[k][g]*10.f, g);
      }
    }
    for(int k=0; k<OUTPUTS_LEN; k++) {
      outputs[k].setChannels(channels);
    }
  }

  //====================== Clock divider ==========================

  float getFreq(int idx, int c, float R, float sampleRate) {
    float pch=params[OCT_PARAM+idx].getValue()+params[TUNE_PARAM+idx].getValue()/12;
    bool linear=params[LIN_PARAM+idx].getValue()>0;
    float pitch=pch+inputs[V_OCT_INPUT+idx].getVoltage(c);
    float freq;
    float fm=0;
    for(int j=0; j<4; j++) {
      float fmParam=params[FM_A_PARAM+j*3+idx].getValue()+inputs[FM_MOD_INPUT+idx].getVoltage(j);
      if(linear) {
        fmParam=scaleExpGeneric(fmParam, -4, 4);
      } else {
        fmParam=scaleExpGeneric(fmParam/2, -2, 2);
      }
      if(idx==j&&lpFM) {
        selfLP[j][c]+=0.1f*(static_cast<float>(oscB[j][c])-selfLP[j][c]);
        fm+=selfLP[j][c]*fmParam;
      } else {
        fm+=(static_cast<float>(oscB[j][c])-0.5f)*fmParam;
      }
    }
    fm=dcBlocker[idx][c].process(fm, R);
    if(linear) {
      freq=dsp::FREQ_C4*dsp::approxExp2_taylor5(pitch+30.f)*inv2_30;
      freq+=dsp::FREQ_C4*fm;
    } else {
      pitch+=fm;
      freq=dsp::FREQ_C4*dsp::approxExp2_taylor5(pitch+30.f)*inv2_30;
    }
    return std::fmin(freq, sampleRate/2);
  }

  int getIndex(int chn) {
    return int(oscB[0][chn])
      |int(oscB[1][chn])<<1
      |int(oscB[2][chn])<<2;
  }

  void processTT(int chn) {
    oscB[3][chn]=b[getIndex(chn)];
  }

  float getMix(int chn) {
    float ret=0;
    for(int k=0; k<4; k++)
      ret+=(inputs[LEVEL_INPUT+k].getVoltage(chn)*0.1f+params[LEVEL_PARAM+k].getValue())*(static_cast<float>(oscB[k][
        chn])-0.5f);
    return ret;
  }

  void processDivN(const ProcessArgs& args) {
    float R=1.f-(2.f*M_PI*10.f/args.sampleRate);
    int channels=std::max(std::max(inputs[V_OCT_INPUT].getChannels(), 1),
                          std::max(inputs[V_OCT_INPUT+1].getChannels(), inputs[V_OCT_INPUT+2].getChannels()));
    for(int c=0; c<channels; c++) {
      for(int k=0; k<3; k++) {
        bool randomWave=params[RND_PARAM+k].getValue()>0;
        float pw=clamp(params[PW_PARAM+k].getValue()+inputs[PWM_INPUT+k].getVoltage(c)*0.1f);
        max[k][c]=static_cast<unsigned>(args.sampleRate/getFreq(k, c, R, args.sampleRate));
        acc[k][c]++;
        if(acc[k][c]>max[k][c]) acc[k][c]=0;
        if(randomWave) {
          float phs_val=static_cast<float>(acc[k][c])/static_cast<float>(std::max(1u, max[k][c]));
          int count=gateCount[k];
          int gate_idx=clamp(static_cast<int>(phs_val*static_cast<float>(count)), 0, count-1);
          oscB[k][c]=((gatePattern[k]>>gate_idx)&1)!=0;
        } else {
          oscB[k][c]=acc[k][c]<static_cast<unsigned>(std::floor(static_cast<float>(max[k][c])*pw));
        }
      }
      processTT(c);
      float mix=outDcBlocker[c].process(getMix(c)*5.f, R);
      outputs[CV_OUTPUT].setVoltage(mix, c);
      for(int k=0; k<4; k++) {
        outputs[A_OUTPUT+k].setVoltage(oscB[k][c]?10.f:0.f, c);
      }
    }
    for(int k=0; k<OUTPUTS_LEN; k++) {
      outputs[k].setChannels(channels);
    }
  }

  //===================== PROCESS 3: Native 1x Rate (Aliasing Engine) =====================
  void processAlias(const ProcessArgs& args) {
    float R=1.f-(2.f*M_PI*10.f/args.sampleRate);
    float linFmFactor=dsp::FREQ_C4*args.sampleTime;
    float maxNyquistInc=args.sampleRate*0.5f*args.sampleTime;

    int channels=std::max({
      inputs[V_OCT_INPUT].getChannels(),
      inputs[V_OCT_INPUT+1].getChannels(),
      inputs[V_OCT_INPUT+2].getChannels(),
      1
      });

    for(int c=0; c<channels; c+=4) {
      int g=c/4;
      float_4 mix=0.f;

      float_4 basePhaseInc[3];
      float fmParam[12];
      bool linear[3];
      bool hasFM[3];
      bool randomWave[3];
      float_4 pw[3];
      float_4 level[4];

      for(int k=0; k<4; k++) {
        level[k]=inputs[LEVEL_INPUT+k].getVoltageSimd<float_4>(c)*0.1f+params[LEVEL_PARAM+k].getValue();
      }

      for(int k=0; k<3; k++) {
        linear[k]=params[LIN_PARAM+k].getValue()>0.f;
        float gain=linear[k]?4.f:2.f;
        float pch=params[OCT_PARAM+k].getValue()+params[TUNE_PARAM+k].getValue()/12.f;

        hasFM[k]=false;
        for(int j=0; j<4; j++) {
          float fm=params[FM_A_PARAM+j*3+k].getValue()+inputs[FM_MOD_INPUT+k].getVoltage(j);
          if(fm!=0.f) {
            fmParam[k*4+j]=scaleExpGeneric(fm/(linear[k]?1.f:2.f), -gain, gain);
            hasFM[k]=true;
          } else {
            fmParam[k*4+j]=0.f;
          }
        }

        float_4 pitch=pch+inputs[V_OCT_INPUT+k].getPolyVoltageSimd<float_4>(c);
        float_4 baseFreq=dsp::FREQ_C4*dsp::approxExp2_taylor5(pitch+30.f)*inv2_30;
        basePhaseInc[k]=baseFreq*args.sampleTime;

        pw[k]=clamp(params[PW_PARAM+k].getValue()+inputs[PWM_INPUT+k].getVoltageSimd<float_4>(c)*0.1f);
        randomWave[k]=params[RND_PARAM+k].getValue()>0.f;
      }

      for(int k=0; k<3; k++) {
        float_4 fms;
        if(hasFM[k]) {
          float_4 fm=0.f;
          for(int j=0; j<4; j++) {
            if(fmParam[k*4+j]!=0.f)
              fm+=(osc[j][g]-0.5f)*fmParam[k*4+j];
          }
          fm=dcBlockerSimd[k][g].process(fm, R);

          if(linear[k]) {
            fms=basePhaseInc[k]+float_4(linFmFactor)*fm;
          } else {
            fms=basePhaseInc[k]*dsp::approxExp2_taylor5(fm+30.f)*inv2_30;
          }
          fms=simd::fmin(fms, float_4(maxNyquistInc));
        } else {
          fms=simd::fmin(basePhaseInc[k], float_4(maxNyquistInc));
        }

        phs[k][g]+=fms;
        phs[k][g]-=simd::floor(phs[k][g]);

        if(randomWave[k]) {
          auto gate_idx=simd::int32_4(phs[k][g]*static_cast<float>(gateCount[k]));
          uint16_t pat=gatePattern[k];
          float_4 gate_out;
          gate_out[0]=((pat>>gate_idx[0])&1)?1.f:0.f;
          gate_out[1]=((pat>>gate_idx[1])&1)?1.f:0.f;
          gate_out[2]=((pat>>gate_idx[2])&1)?1.f:0.f;
          gate_out[3]=((pat>>gate_idx[3])&1)?1.f:0.f;
          osc[k][g]=gate_out;
        } else {
          osc[k][g]=simd::ifelse(phs[k][g]<pw[k], 1.f, 0.f);
        }
      }

      auto tt_mask=computeTT(g);
      osc[3][g]=simd::ifelse(tt_mask, 1.f, 0.f);

      mix=0.f;
      for(int k=0; k<4; k++) {
        mix+=level[k]*(osc[k][g]-0.5f);
      }

      mix=outDcBlockerSimd[g].process(mix, R);
      outputs[CV_OUTPUT].setVoltageSimd(mix*5.f, c);
      for(int k=0; k<4; k++) {
        outputs[A_OUTPUT+k].setVoltageSimd(osc[k][g]*10.f, c);
      }
    }

    for(int k=0; k<OUTPUTS_LEN; k++) {
      outputs[k].setChannels(channels);
    }
  }

  json_t* dataToJson() override {
    json_t* data=json_object();
    json_t* onList=json_array();
    for(int j=0; j<8; j++) {
      json_array_append_new(onList,json_boolean(b[j]));
    }
    json_object_set_new(data, "b", onList);
    json_object_set_new(data, "accurate",json_boolean(lpFM));
    return data;
  }

  void dataFromJson(json_t* rootJ) override {
    json_t* data=json_object_get(rootJ, "b");
    if(!data)
      return;

    for(int j=0; j<8; j++) {
      json_t* on=json_array_get(data, j);
      b[j]=json_boolean_value(on);
    }
    json_t* jAccurate=json_object_get(rootJ, "accurate");
    if(jAccurate) {
      lpFM=json_boolean_value(jAccurate);
    }
  }

  bool getGateStatus(int key) {
    return b[key];
  }

  void updateKey(int key) {
    b[key]=!(b[key]);
  }

  int maxChannels=8;

  void onRandomize(const RandomizeEvent& e) override {
    onRandomize();
  }

  void onRandomize() override {
    for(int k=FM_A_PARAM; k<=FM_TT_PARAM_LAST; k++) {
      auto pq=getParamQuantity(k);
      if(rnd.nextCoin()) {
        pq->setValue(pq->getMinValue()+static_cast<float>(rnd.nextDouble())*(pq->getMaxValue()-pq->getMinValue()));
      } else {
        pq->setValue(0.f);
      }
    }
    for(int k=0; k<=LIN_PARAM_LAST; k++) {
      auto pq=getParamQuantity(k);
      pq->setValue(pq->getMinValue()+static_cast<float>(rnd.nextDouble())*(pq->getMaxValue()-pq->getMinValue()));
    }
    for(int k=RND_PARAM; k<=RND_PARAM_LAST; k++) {
      auto pq=getParamQuantity(k);
      pq->setValue(pq->getMinValue()+static_cast<float>(rnd.nextDouble())*(pq->getMaxValue()-pq->getMinValue()));
    }
    for(int k=0; k<8; k++) {
      b[k]=rnd.nextCoin();
    }
  }
};

template <typename M>
struct GateButton : OpaqueWidget {
  M* module;
  int key;
  Tooltip* tooltip=nullptr;
  std::string label;
  NVGcolor onColor=nvgRGB(118, 169, 118);
  NVGcolor offColor=nvgRGB(55, 80, 55);
  NVGcolor onColorInactive=nvgRGB(128, 128, 128);
  NVGcolor offColorInactive=nvgRGB(43, 43, 43);
  NVGcolor border=nvgRGB(196, 201, 104);
  NVGcolor borderInactive=nvgRGB(150, 150, 150);
  std::basic_string<char> fontPath;

  GateButton(M* _module, int _key, Vec pos, Vec size) : module(_module), key(_key) {
    fontPath=asset::plugin(pluginInstance, "res/FreeMonoBold.ttf");
    box.size=size;
    box.pos=pos;
    int g=0;
    label=string::f("chn %d", key+g);
  }

  void onButton(const event::Button& e) override {
    if(!(e.button==GLFW_MOUSE_BUTTON_LEFT&&(e.mods&RACK_MOD_MASK)==0)) {
      return;
    }
    if(e.action==GLFW_PRESS) {
      if(module) module->updateKey(key);
    }
  }

  void createTooltip() {
    if(!settings::tooltips)
      return;
    tooltip=new Tooltip;
    tooltip->text=string::f("chn %d", key);
    APP->scene->addChild(tooltip);
  }

  void destroyTooltip() {
    if(!tooltip)
      return;
    APP->scene->removeChild(tooltip);
    delete tooltip;
    tooltip=nullptr;
  }

  void onEnter(const EnterEvent& e) override {
    createTooltip();
  }

  void onLeave(const LeaveEvent& e) override {
    destroyTooltip();
  }

  void drawLayer(const DrawArgs& args, int layer) override {
    if(layer==1) {
      _draw(args);
    }
    Widget::drawLayer(args, layer);
  }

  void _draw(const DrawArgs& args) {
    std::shared_ptr<Font> font=APP->window->loadFont(fontPath);
    NVGcolor color=offColor;
    NVGcolor borderColor=border;
    int g=1;
    if(module) {
      g=0;
      if(key>=module->maxChannels) {
        borderColor=borderInactive;
        if(module->getGateStatus(key)) {
          color=onColorInactive;
        } else {
          color=offColorInactive;
        }
      } else {
        borderColor=border;
        if(module->getGateStatus(key)) {
          color=onColor;
        } else {
          color=offColor;
        }
      }
    }
    nvgBeginPath(args.vg);
    nvgRoundedRect(args.vg, 1, 1, box.size.x-2, box.size.y-2, 2);
    nvgFillColor(args.vg, color);
    nvgStrokeColor(args.vg, borderColor);
    nvgFill(args.vg);
    nvgStroke(args.vg);
    nvgFontSize(args.vg, box.size.y-2);
    nvgFontFaceId(args.vg, font->handle);
    NVGcolor textColor=nvgRGB(0xff, 0xff, 0xff);
    nvgTextAlign(args.vg, NVG_ALIGN_CENTER|NVG_ALIGN_MIDDLE);
    nvgFillColor(args.vg, textColor);
    nvgText(args.vg, box.size.x/2.f, box.size.y/2.f, std::to_string(key+g).c_str(),NULL);
  }
};

template <typename M>
struct GateDisplay : OpaqueWidget {
  M* module;
  Vec buttonSize;

  GateDisplay(M* m, Vec _pos) : module(m) {
    box.pos=_pos;
    box.size=Vec(20, 10*8);
    for(int k=0; k<8; k++) {
      addChild(new GateButton<M>(module, k, Vec(0, k*10), Vec(20, 10)));
    }
  }
};

template <typename T>
struct PwmKnob : TrimbotWhite {
  T* module=nullptr;
  int k=0;
  PwmKnob() : TrimbotWhite() {}

  void onChange(const ChangeEvent& e) override {
    SvgKnob::onChange(e);
    if(module) {
      module->refreshGatePattern(k);
    }
  }
};

template <typename T>
struct RndKnob : TrimbotWhite {
  T* module=nullptr;
  int k=0;
  bool contextMenu=false;

  RndKnob() : TrimbotWhite() {}

  void onButton(const ButtonEvent& e) override {
    if(e.action==GLFW_PRESS&&e.button==GLFW_MOUSE_BUTTON_RIGHT&&(e.mods&RACK_MOD_MASK)==0) {
      contextMenu=true;
    } else {
      contextMenu=false;
    }
    Knob::onButton(e);
  }

  void onChange(const ChangeEvent& e) override {
    SvgKnob::onChange(e);
    if(module&&contextMenu) {
      module->refreshGatePattern(k);
    }
    contextMenu=false;
  }

  void onDragEnd(const DragEndEvent& e) override {
    SvgKnob::onDragEnd(e);
    if(e.button==GLFW_MOUSE_BUTTON_LEFT) {
      if(module) {
        module->refreshGatePattern(k);
      }
    }
  }
};

struct Q3Widget : ModuleWidget {
  Q3Widget(Q3* module) {
    setModule(module);
    setPanel(createPanel(asset::plugin(pluginInstance, "res/Q3.svg")));
    addChild(new GateDisplay<Q3>(module, mm2px(Vec(49.5, 12))));
    auto selectParam=createParam<SelectParamH>(mm2px(Vec(18, 4)), module, Q3::ENGINE_PARAM);
    selectParam->box.size=Vec(80, 10);
    selectParam->init({"DN", "OV", "FI", "AL"});
    addParam(selectParam);
    float x=18;
    for(int k=0; k<3; k++) {
      float y=12;
      addParam(createParam<TrimbotWhite>(mm2px(Vec(x, y)), module, Q3::OCT_PARAM+k));
      y+=8;
      addParam(createParam<TrimbotWhite>(mm2px(Vec(x, y)), module, Q3::TUNE_PARAM+k));
      y+=8;
      auto pwmParam=createParam<PwmKnob<Q3>>(mm2px(Vec(x, y)), module, Q3::PW_PARAM+k);
      pwmParam->module=module;
      pwmParam->k=k;
      addParam(pwmParam);
      y+=8;
      addParam(createParam<TrimbotWhite>(mm2px(Vec(x, y)), module, Q3::LEVEL_PARAM+k));
      y+=8;
      auto param=createParam<SmallButtonWithLabel>(mm2px(Vec(x, y+1.5f)), module, Q3::LIN_PARAM+k);
      param->label="Lin";
      addParam(param);
      y+=8;
      addParam(createParam<TrimbotWhite>(mm2px(Vec(x, y)), module, Q3::FM_A_PARAM+k));
      y+=8;
      addParam(createParam<TrimbotWhite>(mm2px(Vec(x, y)), module, Q3::FM_B_PARAM+k));
      y+=8;
      addParam(createParam<TrimbotWhite>(mm2px(Vec(x, y)), module, Q3::FM_C_PARAM+k));
      y+=8;
      addParam(createParam<TrimbotWhite>(mm2px(Vec(x, y)), module, Q3::FM_TT_PARAM+k));
      y+=8;
      auto rndParam=createParam<RndKnob<Q3>>(mm2px(Vec(x, y)), module, Q3::RND_PARAM+k);
      rndParam->module=module;
      rndParam->k=k;
      addParam(rndParam);
      y+=8;
      addInput(createInput<SmallPort>(mm2px(Vec(x, y)), module, Q3::FM_MOD_INPUT+k));
      y+=8;
      addInput(createInput<SmallPort>(mm2px(Vec(x, y)), module, Q3::V_OCT_INPUT+k));
      y+=8;
      addInput(createInput<SmallPort>(mm2px(Vec(x, y)), module, Q3::LEVEL_INPUT+k));
      y+=8;
      addInput(createInput<SmallPort>(mm2px(Vec(x, y)), module, Q3::PWM_INPUT+k));
      x+=10;
    }

    addParam(createParam<TrimbotWhite>(mm2px(Vec(50, 52)), module, Q3::TT_LEVEL_PARAM));
    addInput(createInput<SmallPort>(mm2px(Vec(50, 60)), module, Q3::TT_LEVEL_INPUT));
    addInput(createInput<SmallPort>(mm2px(Vec(50, 41)), module, Q3::TT_INPUT));
    addOutput(createOutput<SmallPort>(mm2px(Vec(50, 116)), module, Q3::CV_OUTPUT));
    for(int k=Q3::A_OUTPUT; k<Q3::OUTPUTS_LEN; k++) {
      addOutput(createOutput<SmallPort>(mm2px(Vec(50, 72+11*(k-1))), module, k));
    }
    addInput(createInput<HiddenPort>(mm2px(Vec(5, 116)), module, Q3::RST_INPUT));
  }

  void appendContextMenu(Menu* menu) override {
    Q3* module=dynamic_cast<Q3*>(this->module);
    assert(module);
    //menu->addChild(new MenuSeparator);
    //menu->addChild(createBoolPtrMenuItem("Lowpass FM (divN mode only)","",&module->lpFM));
  }
};

Model* modelQ3=createModel<Q3, Q3Widget>("Q3");
