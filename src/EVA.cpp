#include "dcb.h"

// reused code from VCV Fundamentals ADSR

using simd::float_4;

struct EVA : Module {
  enum ParamId {
    ATTACK_PARAM,DECAY_PARAM,SUSTAIN_PARAM,RELEASE_PARAM,GAIN_PARAM,PARAMS_LEN
  };
  enum InputId {
    CV_INPUT,GATE_INPUT,RETRIG_INPUT,GAIN_INPUT,INPUTS_LEN
  };
  enum OutputId {
    CV_OUTPUT,ENV_OUTPUT,OUTPUTS_LEN
  };
  enum LightId {
    LIGHTS_LEN
  };
  static constexpr float MIN_TIME=1e-3f;
  static constexpr float MAX_TIME=10.f;
  static constexpr float LAMBDA_BASE=MAX_TIME/MIN_TIME;
  float_4 attacking[4]={};
  float_4 env[4]={};
  dsp::TSchmittTrigger <float_4> trigger[4];
  dsp::ClockDivider cvDivider;
  float_4 attackLambda[4]={};
  float_4 decayLambda[4]={};
  float_4 releaseLambda[4]={};
  float_4 sustain[4]={};

  EVA() {
    config(PARAMS_LEN,INPUTS_LEN,OUTPUTS_LEN,LIGHTS_LEN);
    configParam(ATTACK_PARAM,0.f,1.f,0.5f,"Attack"," ms",LAMBDA_BASE,MIN_TIME*1000);
    configParam(DECAY_PARAM,0.f,1.f,0.5f,"Decay"," ms",LAMBDA_BASE,MIN_TIME*1000);
    configParam(SUSTAIN_PARAM,0.f,1.f,0.5f,"Sustain","%",0,100);
    configParam(RELEASE_PARAM,0.f,1.f,0.5f,"Release"," ms",LAMBDA_BASE,MIN_TIME*1000);
    configParam(GAIN_PARAM,0.f,2.f,1.f,"Gain");
    configInput(GATE_INPUT,"Gate");
    configInput(RETRIG_INPUT,"Retrigger");
    configInput(CV_INPUT,"CV");
    configInput(GAIN_INPUT,"Gain");
    configOutput(CV_OUTPUT,"CV");
    configOutput(ENV_OUTPUT,"Env");
    cvDivider.setDivision(32);
  }

  void process(const ProcessArgs &args) override {
    int inChannels=std::max(1,inputs[CV_INPUT].getChannels());
    int gateChannels=inputs[GATE_INPUT].getChannels();
    int channels = std::max(gateChannels,inChannels);
    // Compute lambdas
    if(cvDivider.process()) {
      float attackParam=params[ATTACK_PARAM].getValue();
      float decayParam=params[DECAY_PARAM].getValue();
      float sustainParam=params[SUSTAIN_PARAM].getValue();
      float releaseParam=params[RELEASE_PARAM].getValue();

      for(int c=0;c<channels;c+=4) {
        attackLambda[c/4]=simd::pow(LAMBDA_BASE,-attackParam)/MIN_TIME;
        decayLambda[c/4]=simd::pow(LAMBDA_BASE,-decayParam)/MIN_TIME;
        releaseLambda[c/4]=simd::pow(LAMBDA_BASE,-releaseParam)/MIN_TIME;
        sustain[c/4]=sustainParam;
      }
    }
    float_4 gate[4]={};

    for(int c=0;c<channels;c+=4) {
      // Gate
      gate[c/4]=inputs[GATE_INPUT].getPolyVoltageSimd<float_4>(c)>=1.f;

      // Retrigger
      float_4 triggered=trigger[c/4].process(inputs[RETRIG_INPUT].getPolyVoltageSimd<float_4>(c));
      attacking[c/4]=simd::ifelse(triggered,float_4::mask(),attacking[c/4]);

      // Get target and lambda for exponential decay
      float_4 gain=params[GAIN_PARAM].getValue();
      if(inputs[GAIN_INPUT].isConnected())
        gain*=(inputs[GAIN_INPUT].getPolyVoltageSimd<float_4>(c)*0.1f);

      const float_4 attackTarget=1.2f*gain;
      float_4 target=simd::ifelse(gate[c/4],simd::ifelse(attacking[c/4],attackTarget,sustain[c/4]*gain),0.f);
      float_4 lambda=simd::ifelse(gate[c/4],simd::ifelse(attacking[c/4],attackLambda[c/4],decayLambda[c/4]),releaseLambda[c/4]);

      // Adjust env
      env[c/4]+=(target-env[c/4])*lambda*args.sampleTime;

      // Turn off attacking state if envelope is HIGH
      attacking[c/4]=simd::ifelse(env[c/4]>=gain,float_4::zero(),attacking[c/4]);

      // Turn on attacking state if gate is LOW
      attacking[c/4]=simd::ifelse(gate[c/4],attacking[c/4],float_4::mask());

      // Set output
      outputs[ENV_OUTPUT].setVoltageSimd(10.f*(env[c/4]),c);

      float_4 input=inputs[CV_INPUT].getVoltageSimd<float_4>(c);
      outputs[CV_OUTPUT].setVoltageSimd(input*env[c/4],c);
    }
    outputs[CV_OUTPUT].setChannels(inChannels);
    outputs[ENV_OUTPUT].setChannels(gateChannels);
  }
};


struct EVAWidget : ModuleWidget {
  EVAWidget(EVA *module) {
    setModule(module);
    setPanel(createPanel(asset::plugin(pluginInstance,"res/EVA.svg")));

    float x=1.9;
    float y=9;
    addParam(createParam<TrimbotWhite>(mm2px(Vec(x,y)),module,EVA::ATTACK_PARAM));
    y+=11;
    addParam(createParam<TrimbotWhite>(mm2px(Vec(x,y)),module,EVA::DECAY_PARAM));
    y+=11;
    addParam(createParam<TrimbotWhite>(mm2px(Vec(x,y)),module,EVA::SUSTAIN_PARAM));
    y+=11;
    addParam(createParam<TrimbotWhite>(mm2px(Vec(x,y)),module,EVA::RELEASE_PARAM));
    y=53;
    addParam(createParam<TrimbotWhite>(mm2px(Vec(x,y)),module,EVA::GAIN_PARAM));
    addInput(createInput<SmallPort>(mm2px(Vec(x,y+8)),module,EVA::GAIN_INPUT));
    y=72;
    addInput(createInput<SmallPort>(mm2px(Vec(x,y)),module,EVA::RETRIG_INPUT));
    y+=11;
    addInput(createInput<SmallPort>(mm2px(Vec(x,y)),module,EVA::GATE_INPUT));
    y+=11;
    addInput(createInput<SmallPort>(mm2px(Vec(x,y)),module,EVA::CV_INPUT));
    y+=11;
    addOutput(createOutput<SmallPort>(mm2px(Vec(x,y)),module,EVA::CV_OUTPUT));
    y+=11;
    addOutput(createOutput<SmallPort>(mm2px(Vec(x,y)),module,EVA::ENV_OUTPUT));
  }
};


Model *modelEVA=createModel<EVA,EVAWidget>("EVA");