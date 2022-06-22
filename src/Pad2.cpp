#include "dcb.h"
#include "rnd.h"
#include <thread>
#include <mutex>
#include <chrono>
#define LENGTH 262144
#include "Pad2.hpp"

struct Pad2 : Module {
  enum ParamId {
    BW_PARAM,BW_CV_PARAM,SCALE_PARAM,SCALE_CV_PARAM,PSEED_PARAM,MTH_PARAM,GEN_PARAM,FUND_PARAM,PARTIAL_PARAM,PARAMS_LEN=PARTIAL_PARAM+48
  };
  enum InputId {
    VOCT_INPUT,BW_INPUT,SCALE_INPUT,PARTIALS_INPUT,RND_TRIGGER_INPUT=PARTIALS_INPUT+3,INPUTS_LEN
  };
  enum OutputId {
    L_OUTPUT,R_OUTPUT,OUTPUTS_LEN
  };
  enum LightId {
    LIGHTS_LEN
  };

  enum Methods {
    HARMONIC_MIN,EVEN_MIN,ODD_MIN,HARMONIC_WB,EVEN_WB,ODD_WB,NUM_METHODS
  };

  RND rnd;
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

  dsp::SchmittTrigger rndTrigger;
  dsp::SchmittTrigger manualRndTrigger;
  double phs[16]={};
  float lastFund=0;
  float lastBw=0;
  float lastScale=0;
  bool inputChanged=false;
  bool updateFund=false;
  bool partialInputConnected=false;
  float fund=32.7;
  float currentFund=32.7;
  float lastPitch;
  bool adjustFund=false;

#define WORKER_THREAD

#ifdef WORKER_THREAD
  bool changed=false;
  bool exiting=false;
  float _sr=48000.f;
  float _pSeed=0.5;
  float _fund=32.7;
  float _bw=10.f;
  float _scl=1;
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
#endif

  Pad2() {
    config(PARAMS_LEN,INPUTS_LEN,OUTPUTS_LEN,LIGHTS_LEN);
    for(int k=0;k<48;k++) {
      configParam(PARTIAL_PARAM+k,0,1,0,"Partial "+std::to_string(k+2));
    }
    configParam(BW_PARAM,0.5f,60,10,"Bandwidth");
    configParam(BW_CV_PARAM,0.0f,3,0,"Bandwidth CV");
    configParam(SCALE_PARAM,0.5f,4.f,1,"Bandwidth Scale");
    configParam(SCALE_CV_PARAM,0.0f,2.f,0.f,"Bandwidth Scale CV");
    configButton(GEN_PARAM,"Generate");
    configParam(PSEED_PARAM,0.f,1.f,0.5,"Phase Seed");
    configParam(FUND_PARAM,4.f,11.f,5.03f,"Frequency"," Hz",2,1);
    configSwitch(MTH_PARAM,0.f,NUM_METHODS-1.01,0,"Method",{"Harmonic Min","Even Min","Odd Min","Harmonic Weibull","Even Weibull","Odd Weibull"});
    getParamQuantity(MTH_PARAM)->snapEnabled=true;
    configInput(VOCT_INPUT,"V/Oct");
    configInput(PARTIALS_INPUT,"Partials 2-17");
    configInput(RND_TRIGGER_INPUT,"Rnd Trigger");
    configInput(PARTIALS_INPUT+1,"Partials 18-33");
    configInput(PARTIALS_INPUT+2,"Partials 34-49");
    configOutput(L_OUTPUT,"Left");
    configOutput(R_OUTPUT,"Right");
    divider.setDivision(SMOOTH_PERIOD*2);
    logDivider.setDivision(24000);

  }
#ifdef WORKER_THREAD
  ~Pad2() {
    exiting=true;
    if(thread.joinable()) {
      thread.join();
    }
  }
#endif

  void onAdd(const AddEvent &e) override {
    update(APP->engine->getSampleRate());
    updateFund=true;
  }

  void generatePartials() {
    partials.clear();
    //partials.push_back(0.f);
    //partials.push_back(0.5f);
    for(int k=0;k<48;k++) {
      partials.push_back(params[PARTIAL_PARAM+k].getValue());
    }
  }

  void regenerateSave(float bw,float scale,float sr,float pSeed,float fund) {
    if(mutex.try_lock()) {
      pt.generate(partials,sr,fund,bw,scale,pSeed);
      mutex.unlock();
    }
  }
  void doWork(std::vector<float> ps,float bw,float scale,float sr,float pSeed,float fund) {
#ifdef WORKER_THREAD
    changed=true;
    _partials=ps;
    _bw=bw;
    _scl=scale;
    _sr=sr;
    _pSeed=pSeed;
    _fund=fund;
#else
    std::thread t1(&Pad2::regenerateSave,this,bw,scale,sr,pSeed,fund);
    t1.detach();
#endif
  }

  void randomizePartials(int method) {
    switch(method) {
      case EVEN_MIN:
        for(int k=0;k<48;k++) {
          if(k%2==0)
            getParamQuantity(PARTIAL_PARAM+k)->setValue(rnd.nextMin(k/4));
          else
            getParamQuantity(PARTIAL_PARAM+k)->setValue(0);
        }
        break;
      case ODD_MIN:
        for(int k=0;k<48;k++) {
          if(k%2==1)
            getParamQuantity(PARTIAL_PARAM+k)->setValue(rnd.nextMin(k/4));
          else
            getParamQuantity(PARTIAL_PARAM+k)->setValue(0);
        }
        break;
      case HARMONIC_WB:
        for(int k=0;k<48;k++) {
          getParamQuantity(PARTIAL_PARAM+k)->setValue(rnd.nextWeibull(k/4.f));
        }
        break;
      case EVEN_WB:
        for(int k=0;k<48;k++) {
          if(k%2==0)
            getParamQuantity(PARTIAL_PARAM+k)->setValue(rnd.nextWeibull(k/4.f));
          else
            getParamQuantity(PARTIAL_PARAM+k)->setValue(0);
        }
        break;
      case ODD_WB:
        for(int k=0;k<48;k++) {
          if(k%2==1)
            getParamQuantity(PARTIAL_PARAM+k)->setValue(rnd.nextWeibull(k/4.f));
          else
            getParamQuantity(PARTIAL_PARAM+k)->setValue(0);
        }
        break;
      case HARMONIC_MIN:
      default:
        for(int k=0;k<48;k++) {
          getParamQuantity(PARTIAL_PARAM+k)->setValue(rnd.nextMin(k/4));
        }
        break;
    }
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
          float oldParam=params[PARTIAL_PARAM+k*16+chn].getValue();
          float newParam=clamp(inputs[PARTIALS_INPUT+k].getVoltage(chn),0.f,10.f)/10.f;
          if(oldParam!=newParam) inputChanged=true;
          getParamQuantity(PARTIAL_PARAM+k*16+chn)->setValue(newParam);
        }
        partialInputConnected=true;
      } else {
        if(k==0) break;
      }
    }
    float bw=params[BW_PARAM].getValue();
    if(inputs[BW_INPUT].isConnected()) {
      bw+=inputs[BW_INPUT].getVoltage()*params[BW_CV_PARAM].getValue();
      bw=clamp(bw,0.5f,60.f);
    }
    float scale=params[SCALE_PARAM].getValue();
    if(inputs[SCALE_INPUT].isConnected()) {
      scale+=inputs[SCALE_INPUT].getVoltage()*params[SCALE_CV_PARAM].getValue()*0.1f;
      scale=clamp(scale,0.5f,4.f);
    }

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

  float lowestPitch(int channels) {
    float min=10;
    for(int k=0;k<channels;k++) {
      float voltage=inputs[VOCT_INPUT].getVoltage(k);
      if(voltage<min) min=voltage;
    }
    return min;
  }

  void process(const ProcessArgs &args) override {
    int channels=inputs[VOCT_INPUT].getChannels();
    if(divider.process()) {
      float minPitch=lowestPitch(channels);
      if(adjustFund) {
        fund=dsp::FREQ_C4 * dsp::approxExp2_taylor5(minPitch-1+30.f)/std::pow(2.f,30.f);
      } else if(updateFund) {
        fund=dsp::approxExp2_taylor5(params[FUND_PARAM].getValue()-1+30.f)/std::pow(2.f,30.f);
        updateFund=false;
      }
      update(args.sampleRate);
    }
    if(rndTrigger.process(inputs[RND_TRIGGER_INPUT].getVoltage()) | manualRndTrigger.process(params[GEN_PARAM].getValue())) {
      if(!partialInputConnected) randomizePartials(params[MTH_PARAM].getValue());
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

struct PartialFader : SliderKnob {
  Pad2 *module=nullptr;
  float pos=0;

  PartialFader() : SliderKnob() {
    box.size=mm2px(Vec(4.5f,33.f));
    forceLinear=true;
    speed=2.0;
  }

  void drawLayer(const DrawArgs &args,int layer) override {
    if(layer==1) {
      _draw(args);
    }
    Widget::drawLayer(args,layer);
  }

  void _draw(const DrawArgs &args) {
    ParamQuantity *pq=getParamQuantity();
    if(pq) {
      pos=pq->getSmoothValue();
    }
    nvgBeginPath(args.vg);
    nvgRect(args.vg,0,0,box.size.x,box.size.y);
    nvgFillColor(args.vg,nvgRGB(0x55,0x55,0x55));
    nvgStrokeColor(args.vg,nvgRGB(0x88,0x88,0x88));
    nvgFill(args.vg);
    nvgStroke(args.vg);
    nvgFillColor(args.vg,nvgRGB(0x77,0x77,0x77));

    nvgBeginPath(args.vg);
    nvgRect(args.vg,1,box.size.y-box.size.y*pos,box.size.x-2,box.size.y*pos);
    nvgFill(args.vg);

    nvgFillColor(args.vg,nvgRGB(0x0,0xee,0x88));
    nvgRect(args.vg,1,box.size.y-box.size.y*pos-1,box.size.x-2,2);
    nvgFill(args.vg);

  }

  void onButton(const event::Button &e) override {
    if(e.button==GLFW_MOUSE_BUTTON_LEFT&&e.action==GLFW_PRESS&&(e.mods&RACK_MOD_MASK)==0) {
      float pct=(box.size.y-e.pos.y)/box.size.y;
      ParamQuantity *pq=getParamQuantity();
      if(pq) {
        pq->setValue(pct);
      }
      e.consume(this);
    }
    SliderKnob::onButton(e);
  }

  void onChange(const event::Change &e) override {
    if(module) {
      module->inputChanged=true;
    }
    SliderKnob::onChange(e);
  }

};


struct Pad2Widget : ModuleWidget {
  Pad2Widget(Pad2 *module) {
    setModule(module);
    setPanel(createPanel(asset::plugin(pluginInstance,"res/Pad2.svg")));

    addChild(createWidget<ScrewSilver>(Vec(0,0)));
    addChild(createWidget<ScrewSilver>(Vec(box.size.x-15,0)));
    addChild(createWidget<ScrewSilver>(Vec(0,365)));
    addChild(createWidget<ScrewSilver>(Vec(box.size.x-15,365)));

    addParam(createParam<TrimbotWhite>(mm2px(Vec(3,TY(108))),module,Pad2::BW_PARAM));
    addInput(createInput<SmallPort>(mm2px(Vec(3,TY(98))),module,Pad2::BW_INPUT));
    addParam(createParam<TrimbotWhite>(mm2px(Vec(3,TY(88))),module,Pad2::BW_CV_PARAM));
    addParam(createParam<TrimbotWhite>(mm2px(Vec(12.9,TY(108))),module,Pad2::SCALE_PARAM));
    addInput(createInput<SmallPort>(mm2px(Vec(12.9,TY(98))),module,Pad2::SCALE_INPUT));
    addParam(createParam<TrimbotWhite>(mm2px(Vec(12.9,TY(88))),module,Pad2::SCALE_CV_PARAM));

    addParam(createParam<TrimbotWhite>(mm2px(Vec(3,56)),module,Pad2::MTH_PARAM));
    addParam(createParam<MLEDM>(mm2px(Vec(13,56)),module,Pad2::GEN_PARAM));
    addInput(createInput<SmallPort>(mm2px(Vec(8,64)),module,Pad2::RND_TRIGGER_INPUT));

    auto fundParam=createParam<UpdateOnReleaseKnob>(mm2px(Vec(8,82)),module,Pad2::FUND_PARAM);
    fundParam->update=&module->updateFund;
    addParam(fundParam);

    addParam(createParam<TrimbotWhite>(mm2px(Vec(8,94)),module,Pad2::PSEED_PARAM));


    addInput(createInput<SmallPort>(mm2px(Vec(8,106)),module,Pad2::VOCT_INPUT));
    for(int k=0;k<3;k++)
      addInput(createInput<SmallPort>(mm2px(Vec(25+k*10,119)),module,Pad2::PARTIALS_INPUT+k));
    addOutput(createOutput<SmallPort>(mm2px(Vec(78,119)),module,Pad2::L_OUTPUT));
    addOutput(createOutput<SmallPort>(mm2px(Vec(88,119)),module,Pad2::R_OUTPUT));

    for(int i=0;i<3;i++) {
      for(int k=0;k<16;k++) {
        auto faderParam=createParam<PartialFader>(mm2px(Vec(24+k*4.5,5+38*i)),module,Pad2::PARTIAL_PARAM+(i*16+k));
        faderParam->module=module;
        addParam(faderParam);
      }
    }
  }
};


Model *modelPad2=createModel<Pad2,Pad2Widget>("Pad2");