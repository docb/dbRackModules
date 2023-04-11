#include "dcb.h"
#include "Pad2.hpp"
#include <thread>
#include <mutex>
#include <chrono>
#define LENGTH 262144

struct MPad2 : Module {
  enum ParamId {
    BW_PARAM,SCALE_PARAM,PSEED_PARAM,FUND_PARAM,PARAMS_LEN
  };
  enum InputId {
    VOCT_INPUT,BW_INPUT,SCALE_INPUT,PARTIALS_INPUT,INPUTS_LEN=PARTIALS_INPUT+3
  };
  enum OutputId {
    L_OUTPUT,R_OUTPUT,OUTPUTS_LEN
  };
  enum LightId {
    LIGHTS_LEN
  };
  Pad2Table<LENGTH> pt;
  Pad2Table<LENGTH*2> pt2;
  Pad2Table<LENGTH*4> pt4;
  Pad2Table<LENGTH*8> pt8;
  enum PTSR {LESS_88K,LESS_176K,LESS_352K,GT_352K};
  PTSR currentPTSR=LESS_88K;
  std::vector<float> partials;
  std::mutex mutex;
  dsp::ClockDivider divider;
  dsp::ClockDivider logDivider;
  double phs[16]={};
  float lastFund=0;
  float lastBw=0;
  float lastScale=0;
  bool inputChanged=false;
  bool updateFund=false;
  //bool partialInputConnected=false;
  float fund=32.7;
  float currentFund=32.7;
  //float lastPitch;
  bool changed=false;
  bool exiting=false;
  float _sr=48000.f;
  float _pSeed=0.5;
  float _fund=32.7;
  float _bw=10.f;
  float _scl=1;
  float partialParams[48]={};
  std::vector<float> _partials={};
  std::thread thread=std::thread([this] {
    while(!exiting) {
      if(changed) {
        switch(currentPTSR) {
          case GT_352K:
            pt8.generate(_partials,_sr,_fund,_bw,_scl,_pSeed);
            break;
          case LESS_352K:
            pt4.generate(_partials,_sr,_fund,_bw,_scl,_pSeed);
            break;
          case LESS_176K:
            pt2.generate(_partials,_sr,_fund,_bw,_scl,_pSeed);
            break;
          default:
            pt.generate(_partials,_sr,_fund,_bw,_scl,_pSeed);
        }
        changed=false;
        currentFund=_fund;
      }
      std::this_thread::sleep_for(std::chrono::duration<double>(SMOOTH_PERIOD*2.f/_sr));
    }
  });
	MPad2() {
    config(PARAMS_LEN,INPUTS_LEN,OUTPUTS_LEN,LIGHTS_LEN);
    configParam(BW_PARAM,0.5f,60,10,"Bandwidth");
    configParam(SCALE_PARAM,0.5f,4.f,1,"Bandwidth Scale");
    configParam(PSEED_PARAM,0.f,1.f,0.5,"Phase Seed");
    configParam(FUND_PARAM,4.f,11.f,5.03f,"Frequency"," Hz",2,1);
    configInput(VOCT_INPUT,"V/Oct");
    configInput(PARTIALS_INPUT,"Partials 2-17");
    configInput(PARTIALS_INPUT+1,"Partials 18-33");
    configInput(PARTIALS_INPUT+2,"Partials 34-49");
    configOutput(L_OUTPUT,"Left");
    configOutput(R_OUTPUT,"Right");
    divider.setDivision(SMOOTH_PERIOD*2);
    logDivider.setDivision(24000);
	}
  ~MPad2() {
    exiting=true;
    if(thread.joinable()) {
      thread.join();
    }
  }
  void onAdd(const AddEvent &e) override {
    update(APP->engine->getSampleRate());
    updateFund=true;
  }

  void generatePartials() {
    partials.clear();
    for(int k=0;k<48;k++) {
      partials.push_back(partialParams[k]);
    }
  }
  void doWork(std::vector<float> ps,float bw,float scale,float sr,float pSeed,float fund) {
    changed=true;
    _partials=ps;
    _bw=bw;
    _scl=scale;
    _sr=sr;
    _pSeed=pSeed;
    _fund=fund;
  }
  void updatePTSR(float sr) {
    if(sr<88000.f) {
      currentPTSR=LESS_88K;
    } else if(sr<176000.f) {
      currentPTSR=LESS_176K;
    } else if(sr<352000.f) {
      currentPTSR=LESS_352K;
    } else {
      currentPTSR=GT_352K;
    }
  }

  float getLength() {
    switch(currentPTSR) {
      case LESS_88K: return LENGTH;
      case LESS_176K: return LENGTH*2.f;
      case LESS_352K: return LENGTH*4.f;
      default:
        return LENGTH*8.f;
    }
  }

  float lookup(double _phs) {
    switch(currentPTSR) {
      case LESS_88K: return pt.lookup(_phs);
      case LESS_176K: return pt2.lookup(_phs);
      case LESS_352K: return pt4.lookup(_phs);
      default:
        return pt8.lookup(_phs);
    }
  }

  void update(float sr) {
    updatePTSR(sr);
    for(int k=0;k<3;k++) {
      if(inputs[PARTIALS_INPUT+k].isConnected()) {
        for(int chn=0;chn<16;chn++) {
          float oldParam=partialParams[k*16+chn];
          float newParam=clamp(inputs[PARTIALS_INPUT+k].getVoltage(chn),0.f,10.f)/10.f;
          if(oldParam!=newParam) inputChanged=true;
          partialParams[k*16+chn]=newParam;
        }
      } else {
        if(k==0) break;
      }
    }
    if(inputs[BW_INPUT].isConnected()) {
      getParamQuantity(BW_PARAM)->setImmediateValue(rescale(inputs[BW_INPUT].getVoltage(),0.f,10.f,0.5f,60.f));
    }
    float bw=params[BW_PARAM].getValue();
    if(inputs[SCALE_INPUT].isConnected()) {
      getParamQuantity(SCALE_PARAM)->setImmediateValue(rescale(inputs[SCALE_INPUT].getVoltage(),0.f,10.f,0.5f,4.f));
    }
    float scale=params[SCALE_PARAM].getValue();
    float pSeed=params[PSEED_PARAM].getValue();

    if(inputChanged) {
      generatePartials();
    }
    if(bw!=lastBw||scale!=lastScale||pSeed!=pt.currentSeed||inputChanged||lastFund!=fund||_sr!=sr) {
      doWork(partials,bw,scale,sr,pSeed,fund);
    }
    inputChanged=false;
    lastBw=bw;
    lastScale=scale;
    lastFund=fund;
  }
	void process(const ProcessArgs& args) override {
    int channels=inputs[VOCT_INPUT].getChannels();
    if(divider.process()) {
      if(updateFund) {
        fund=dsp::approxExp2_taylor5(params[FUND_PARAM].getValue()-1+30.f)/std::pow(2.f,30.f);
        updateFund=false;
      }
      update(args.sampleRate);
    }

    for(int k=0;k<channels;k++) {
      double voct=inputs[VOCT_INPUT].getVoltage(k)-1;
      double freq = dsp::FREQ_C4 * dsp::approxExp2_taylor5(float(voct) + 30.f) / std::pow(2.f, 30.f);
      double oscFreq=(freq*args.sampleRate)/(getLength()*currentFund);
      double dPhase=oscFreq*args.sampleTime;
      phs[k]+=dPhase;
      phs[k]-=floor(phs[k]);

      float outL=lookup(phs[k]);
      float outR=lookup(phs[k]+0.5);
      outputs[L_OUTPUT].setVoltage(outL/2.5f,k);
      outputs[R_OUTPUT].setVoltage(outR/2.5f,k);
    }
    outputs[L_OUTPUT].setChannels(channels);
    outputs[R_OUTPUT].setChannels(channels);
	}
};


struct MPad2Widget : ModuleWidget {
	MPad2Widget(MPad2* module) {
		setModule(module);
		setPanel(createPanel(asset::plugin(pluginInstance, "res/MPad2.svg")));

    addChild(createWidget<ScrewSilver>(Vec(0, 0)));
    addChild(createWidget<ScrewSilver>(Vec(box.size.x - 15, 0)));
    addChild(createWidget<ScrewSilver>(Vec(0, 365)));
    addChild(createWidget<ScrewSilver>(Vec(box.size.x - 15, 365)));

    addParam(createParam<TrimbotWhite>(mm2px(Vec(2,16)),module,MPad2::BW_PARAM));
    addInput(createInput<SmallPort>(mm2px(Vec(2,24)),module,MPad2::BW_INPUT));
    addParam(createParam<TrimbotWhite>(mm2px(Vec(12,16)),module,MPad2::SCALE_PARAM));
    addInput(createInput<SmallPort>(mm2px(Vec(12,24)),module,MPad2::SCALE_INPUT));

    float x=6.9;
    float y=36;
    for(int k=0;k<3;k++) {
      addInput(createInput<SmallPort>(mm2px(Vec(x,y)),module,MPad2::PARTIALS_INPUT+k));
      y+=12;
    }
    auto fundParam=createParam<UpdateOnReleaseKnob>(mm2px(Vec(x,y)),module,MPad2::FUND_PARAM);
    fundParam->update=&module->updateFund;
    addParam(fundParam);
    y+=12;
    addParam(createParam<TrimbotWhite>(mm2px(Vec(x,y)),module,MPad2::PSEED_PARAM));
    y+=12;
    addInput(createInput<SmallPort>(mm2px(Vec(x,y)),module,MPad2::VOCT_INPUT));
    addOutput(createOutput<SmallPort>(mm2px(Vec(2,114)),module,MPad2::L_OUTPUT));
    addOutput(createOutput<SmallPort>(mm2px(Vec(12,114)),module,MPad2::R_OUTPUT));
  }
};


Model* modelMPad2 = createModel<MPad2, MPad2Widget>("MPad2");