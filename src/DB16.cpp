#include <cfloat>

#include "dcb.h"
using simd::float_4;

template <typename T>
struct Biquad {
  T z1=0, z2=0;
  float b0=1.0f, b1=0.0f, b2=0.0f, a1=0.0f, a2=0.0f;

  void setCoefs(float _b0, float _b1, float _b2, float _a1, float _a2) {
    b0=_b0;
    b1=_b1;
    b2=_b2;
    a1=_a1;
    a2=_a2;
  }

  T process(T in) {
    T out=(in*b0)+z1;
    z1=(in*b1)-(out*a1)+z2;
    z2=(in*b2)-(out*a2);
    return out;
  }
};

template <typename T>
struct LR8 {
  Biquad<T> stage1;
  Biquad<T> stage2;
  Biquad<T> stage3;
  Biquad<T> stage4;

  void setCoefs(float b0_1, float b1_1, float b2_1, float a1_1, float a2_1,
                float b0_2, float b1_2, float b2_2, float a1_2, float a2_2) {

    stage1.setCoefs(b0_1, b1_1, b2_1, a1_1, a2_1); // Q1
    stage2.setCoefs(b0_2, b1_2, b2_2, a1_2, a2_2); // Q2

    stage3.setCoefs(b0_1, b1_1, b2_1, a1_1, a2_1); // Q1
    stage4.setCoefs(b0_2, b1_2, b2_2, a1_2, a2_2); // Q2
  }

  T process(T in) {
    return stage4.process(stage3.process(stage2.process(stage1.process(in))));
  }
};

struct DB16 : Module {
  enum ParamId {
    GAIN, PARAMS_LEN=GAIN+16
  };

  enum InputId {
    L_INPUT, INPUTS_LEN
  };

  enum OutputId {
    L16_OUTPUT, L8_OUTPUT=L16_OUTPUT+16, L4_OUTPUT=L8_OUTPUT+8,
    L2_OUTPUT_1=L4_OUTPUT+4, L2_OUTPUT_2, L_OUTPUT, OUTPUTS_LEN
  };

  enum LightId {
    LIGHTS_LEN
  };

  const float crossoverFreqs[15] = {
    50.f, 75.f, 110.f, 160.f, 235.f, 345.f, 500.f, 730.f,
    1070.f, 1560.f, 2280.f, 3330.f, 4870.f, 7120.f, 10000.f
  };

  const std::string labels16[16] = {
    "Sub (<50)", "50-75 Hz", "75-110 Hz", "110-160 Hz",
    "160-235 Hz", "235-345 Hz", "345-500 Hz", "500-730 Hz",
    "730-1K Hz", "1K-1.5K Hz", "1.5K-2.2K Hz", "2.2K-3.3K Hz",
    "3.3K-4.8K Hz", "4.8K-7.1K Hz", "7.1K-10K Hz", "Air (>10K)"
  };
  const std::string labels8[8]={
    "0-75 HZ", "75-160 HZ", "160-345 HZ", "345-730 HZ",
    "730-1.5K HZ", "1.5K-3.3K HZ", "3.3K-7.1K HZ", ">7.1K HZ"
    };
  const std::string labels4[4]={
    "0-160 HZ", "160-730 HZ", "730-3.3K HZ", ">3.3K HZ"
    };
  LR8<float_4> lowpassFiltersL[4][15];
  LR8<float_4> highpassFiltersL[4][15];

  DB16() {
    config(PARAMS_LEN, INPUTS_LEN, OUTPUTS_LEN, LIGHTS_LEN);
    configInput(L_INPUT, "Audio");
    for(int k=0; k<16; k++) {
      configParam(k, 0, 2, 1, "Gain "+labels16[k]);
      configOutput(L16_OUTPUT+k, labels16[k]);
    }
    for(int k=0; k<8; k++) {
      configOutput(L8_OUTPUT+k, labels8[k]);
    }
    for(int k=0; k<4; k++) {
      configOutput(L4_OUTPUT+k, labels4[k]);
    }
    configOutput(L2_OUTPUT_1, "0-730 HZ");
    configOutput(L2_OUTPUT_2, "730K-24K HZ");
    configOutput(L_OUTPUT, "Sum");

    updateAllCrossovers(APP->engine->getSampleRate());
  }

  void updateAllCrossovers(float sampleRate) {
    for(int i=0; i<15; ++i) {
      for(int c=0; c<4; ++c) {
        setCrossoverFreq8(
          lowpassFiltersL[c][i],
          highpassFiltersL[c][i],
          crossoverFreqs[i],
          sampleRate
        );
      }
    }
  }


  static void setCrossoverFreq8(LR8<float_4>& lpFilter, LR8<float_4>& hpFilter, float freq, float sampleRate) {
    float omega = 2.0f * M_PI * freq / sampleRate;
    float sin_omega = std::sin(omega);
    float cos_omega = std::cos(omega);

    // The two required Q-factors for a 4th-order Butterworth
    float q1 = 0.54119610f;
    float q2 = 1.30656296f;

    // --- STAGE 1 (Using Q1) ---
    float alpha1 = sin_omega / (2.0f * q1);
    float a0_1 = 1.0f + alpha1;
    float inv_a0_1 = 1.0f / a0_1;

    float a1_1 = (-2.0f * cos_omega) * inv_a0_1;
    float a2_1 = (1.0f - alpha1) * inv_a0_1;

    float lp_b1_1 = (1.0f - cos_omega) * inv_a0_1;
    float lp_b0_1 = lp_b1_1 * 0.5f;
    float hp_b1_1 = -(1.0f + cos_omega) * inv_a0_1;
    float hp_b0_1 = -hp_b1_1 * 0.5f;

    // --- STAGE 2 (Using Q2) ---
    float alpha2 = sin_omega / (2.0f * q2);
    float a0_2 = 1.0f + alpha2;
    float inv_a0_2 = 1.0f / a0_2;

    float a1_2 = (-2.0f * cos_omega) * inv_a0_2;
    float a2_2 = (1.0f - alpha2) * inv_a0_2;

    float lp_b1_2 = (1.0f - cos_omega) * inv_a0_2;
    float lp_b0_2 = lp_b1_2 * 0.5f;
    float hp_b1_2 = -(1.0f + cos_omega) * inv_a0_2;
    float hp_b0_2 = -hp_b1_2 * 0.5f;

    // --- ASSIGN TO STRUCTS ---
    lpFilter.setCoefs(
        lp_b0_1, lp_b1_1, lp_b0_1, a1_1, a2_1, // Q1 Coefs
        lp_b0_2, lp_b1_2, lp_b0_2, a1_2, a2_2  // Q2 Coefs
    );

    hpFilter.setCoefs(
        hp_b0_1, hp_b1_1, hp_b0_1, a1_1, a2_1, // Q1 Coefs
        hp_b0_2, hp_b1_2, hp_b0_2, a1_2, a2_2  // Q2 Coefs
    );
  }

  void process(const ProcessArgs& args) override {
    if(inputs[L_INPUT].isConnected()) {
      int channelsL=inputs[L_INPUT].getChannels();
      for(int c=0; c<channelsL; c+=4) {
        float_4 inL=inputs[L_INPUT].getVoltageSimd<float_4>(c);

        float_4 S1KLOW=lowpassFiltersL[c/4][7].process(inL);
        float_4 S1KHIGH=highpassFiltersL[c/4][7].process(inL);

        float_4 v4[4]={};
        v4[0]=lowpassFiltersL[c/4][3].process(S1KLOW);
        v4[1]=highpassFiltersL[c/4][3].process(S1KLOW);

        v4[2]=lowpassFiltersL[c/4][11].process(S1KHIGH);
        v4[3]=highpassFiltersL[c/4][11].process(S1KHIGH);

        float_4 v8[8]={};
        for(int k=0; k<4; k++) {
          v8[2*k]=lowpassFiltersL[c/4][1+k*4].process(v4[k]);
          v8[2*k+1]=highpassFiltersL[c/4][1+k*4].process(v4[k]);
        }

        float_4 v[16]={};
        for(int k=0; k<8; k++) {
          v[2*k]=lowpassFiltersL[c/4][k*2].process(v8[k])*params[k*2].getValue();
          outputs[L16_OUTPUT+2*k].setVoltageSimd(v[2*k], c);
          v[2*k+1]=highpassFiltersL[c/4][k*2].process(v8[k])*params[k*2+1].getValue();
          outputs[L16_OUTPUT+2*k+1].setVoltageSimd(v[2*k+1], c);
        }

        for(int k=0; k<8; k++) {
          v8[k]=v[2*k]+v[2*k+1];
          outputs[L8_OUTPUT+k].setVoltageSimd(v8[k], c);
        }
        for(int k=0; k<4; k++) {
          v4[k]=v8[2*k]+v8[2*k+1];
          outputs[L4_OUTPUT+k].setVoltageSimd(v4[k], c);
        }
        float_4 S1K=v4[0]+v4[1];
        outputs[L2_OUTPUT_1].setVoltageSimd(S1K, c);
        float_4 S1KH=v4[2]+v4[3];
        outputs[L2_OUTPUT_2].setVoltageSimd(S1KH, c);
        outputs[L_OUTPUT].setVoltageSimd(S1K+S1KH, c);
      }
      for(int k=0;k<OUTPUTS_LEN;k++) {
        outputs[k].setChannels(channelsL);
      }
    }
  }
};

struct DB16Widget : ModuleWidget {
  DB16Widget(DB16* module) {
    setModule(module);
    setPanel(createPanel(asset::plugin(pluginInstance, "res/DB16.svg")));
    float x=10.5;
    float y=2.5;
    addInput(createInput<SmallPort>(mm2px(Vec(18.5, 116)), module, DB16::L_INPUT));
    for(int k=0; k<16; k++) {
      addParam(createParam<TrimbotWhite>(mm2px(Vec(x, y)), module, k));
      addOutput(createOutput<SmallPort>(mm2px(Vec(x+8, y)), module, DB16::L16_OUTPUT+k));
      y+=7;
    }
    x=25;
    y=5.5;
    for(int k=0; k<8; k++) {
      addOutput(createOutput<SmallPort>(mm2px(Vec(x, y)), module, DB16::L8_OUTPUT+k));
      y+=14;
    }
    x=30;
    y=12;
    for(int k=0; k<4; k++) {
      addOutput(createOutput<SmallPort>(mm2px(Vec(x, y)), module, DB16::L4_OUTPUT+k));
      y+=28;
    }
    x=33;
    addOutput(createOutput<SmallPort>(mm2px(Vec(x, 27)), module, DB16::L2_OUTPUT_1));
    addOutput(createOutput<SmallPort>(mm2px(Vec(x, 83)), module, DB16::L2_OUTPUT_2));
    addOutput(createOutput<SmallPort>(mm2px(Vec(x, 116)), module, DB16::L_OUTPUT));
  }
};

Model* modelDB16=createModel<DB16, DB16Widget>("DB16");
