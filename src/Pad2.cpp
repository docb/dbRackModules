#include "dcb.h"
#include "rnd.h"
#include <thread>
#include <mutex>
#include <chrono>
#define LENGTH 262144
/**
 *  padsynth algorithm from Paul Nasca
 */
#define SMOOTH_PERIOD 1028

struct Pad2Table {
  float *table[2];
  float rndPhs[LENGTH];
  float currentSeed;
  int currentTable=0;
  dsp::RealFFT _realFFT;
  RND rnd;
  int counter=0;
  double lastDuration;

  Pad2Table() : _realFFT(LENGTH) {
    table[0]=new float[LENGTH];
    table[1]=new float[LENGTH];
    reset(0.5);
  }

  ~Pad2Table() {
    delete[] table[0];
    delete[] table[1];
  }

  void reset(float pSeed) {
    rnd.reset((unsigned long long)(floor((double)pSeed*(double)ULONG_MAX)));
    for(int k=0;k<LENGTH;k++) {
      rndPhs[k]=float(rnd.nextDouble()*M_PI*2);
    }
    currentSeed=pSeed;
  }

  float profile(float fi,float bwi) {
    float x=fi/bwi;
    x*=x;
    if(x>14.71280603)
      return 0.0;//this avoids computing the e^(-x^2) where it's results are very close to zero
    return expf(-x)/bwi;
  };

  void generate(const std::vector<float> &partials,float sampleRate,float fundFreq,float partialBW,float bwScale,float pSeed) {
    auto input=new float[LENGTH*2];
    auto work=new float[LENGTH*2];
    memset(input,0,LENGTH*2*sizeof(float));
    input[0]=0.f;
    input[1]=0.f;
    for(unsigned int k=1;k<partials.size();k++) {
      if(partials[k]>0.f) {
        float partialHz=fundFreq*float(k);
        float freqIdx=partialHz/((float)sampleRate);
        float bandwidthHz=(std::pow(2.0f,partialBW/1200.0f)-1.0f)*fundFreq*std::pow(float(k),bwScale);
        float bandwidthSamples=bandwidthHz/(2.0f*sampleRate);
        for(int i=0;i<LENGTH;i++) {
          float fttIdx=((float)i)/((float)LENGTH*2);
          float profileIdx=fttIdx-freqIdx;
          float profileSample=profile(profileIdx,bandwidthSamples);
          input[i*2]+=(profileSample*partials[k]);
        }
      }
    }
    if(currentSeed!=pSeed)
      reset(pSeed);
    for(int k=0;k<LENGTH;k++) {
      auto randomPhase=rndPhs[k];
      input[k*2]=(input[k*2]*cosf(randomPhase));
      input[k*2+1]=(input[k*2]*sinf(randomPhase));
    };
    int idx=(currentTable+1)%2;
    pffft_transform_ordered(_realFFT.setup,input,table[idx],work,PFFFT_BACKWARD);
    _realFFT.scale(table[idx]);
    currentTable=idx;
    counter=SMOOTH_PERIOD;
    delete[] input;
    delete[] work;
  }

  float lookup(float phs) {
    if(counter>0) {
      float mix=float(counter)/float(SMOOTH_PERIOD);
      float future=table[currentTable][int(phs*float(LENGTH))&(LENGTH-1)];
      int lastTable=currentTable==0?1:0;
      float last=table[lastTable][int(phs*float(LENGTH))&(LENGTH-1)];
      counter--;
      return future*(1-mix)+last*mix;
    }
    return table[currentTable][int(phs*float(LENGTH))&(LENGTH-1)];
  }


};

struct Pad2 : Module {
  enum ParamId {
    BW_PARAM,BW_CV_PARAM,SCALE_PARAM,SCALE_CV_PARAM,PSEED_PARAM,MTH_PARAM,GEN_PARAM,FUND_PARAM,PARTIAL_PARAM,PARAMS_LEN=PARTIAL_PARAM+48
  };
  enum InputId {
    VOCT_INPUT,BW_INPUT,SCALE_INPUT,INPUTS_LEN
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
  Pad2Table pt;
  std::vector<float> partials;
  std::mutex mutex;
  dsp::ClockDivider divider;
  dsp::ClockDivider logDivider;

  dsp::SchmittTrigger rndTrigger;
  float phs[16]={};
  float lastFund=0;
  float lastBw=0;
  float lastScale=0;
  bool inputChanged=false;
  bool updateFund=false;
  float fund=32.7;

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
        pt.generate(_partials,_sr,_fund,_bw,_scl,_pSeed);
        changed=false;
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
    partials.push_back(0.f);
    partials.push_back(1.f);
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



  void update(float sr) {
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
    if(bw!=lastBw||scale!=lastScale||pSeed!=pt.currentSeed||inputChanged||lastFund!=fund) {
      doWork(partials,bw,scale,sr,pSeed,fund);
    }
    inputChanged=false;
    lastBw=bw;
    lastScale=scale;
    lastFund=fund;

  }

  void process(const ProcessArgs &args) override {
    if(divider.process())
      update(args.sampleRate);
    if(rndTrigger.process(params[GEN_PARAM].getValue())) {
      randomizePartials(params[MTH_PARAM].getValue());
    }
    if(updateFund) {
      fund = dsp::approxExp2_taylor5(params[FUND_PARAM].getValue() + 30.f) / std::pow(2.f, 30.f);
      updateFund=false;
    }
    int channels=inputs[VOCT_INPUT].getChannels();
    for(int k=0;k<channels;k++) {
      float voct=inputs[VOCT_INPUT].getVoltage(k);
      float freq = dsp::FREQ_C4 * dsp::approxExp2_taylor5((voct-1) + 30.f) / std::pow(2.f, 30.f);
      float oscFreq=(freq*args.sampleRate)/(float(LENGTH)*fund);
      float dPhase=oscFreq*args.sampleTime;
      phs[k]+=dPhase;
      phs[k]-=floorf(phs[k]);

      float outL=pt.lookup(phs[k]);
      float outR=pt.lookup(phs[k]+0.5);
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

    addParam(createParam<TrimbotWhite>(mm2px(Vec(3,TY(66))),module,Pad2::MTH_PARAM));
    addParam(createParam<MLEDM>(mm2px(Vec(12.9,TY(66))),module,Pad2::GEN_PARAM));

    auto fundParam=createParam<UpdateOnReleaseKnob>(mm2px(Vec(8,TY(54))),module,Pad2::FUND_PARAM);
    fundParam->update=&module->updateFund;
    addParam(fundParam);

    addParam(createParam<TrimbotWhite>(mm2px(Vec(8,TY(42))),module,Pad2::PSEED_PARAM));


    addInput(createInput<SmallPort>(mm2px(Vec(8,TY(28))),module,Pad2::VOCT_INPUT));
    addOutput(createOutput<SmallPort>(mm2px(Vec(3,TY(8))),module,Pad2::L_OUTPUT));
    addOutput(createOutput<SmallPort>(mm2px(Vec(12.9,TY(8))),module,Pad2::R_OUTPUT));

    for(int i=0;i<3;i++) {
      for(int k=0;k<16;k++) {
        auto faderParam=createParam<PartialFader>(mm2px(Vec(24+k*4.5,MHEIGHT-(9+38*(2-i))-33)),module,Pad2::PARTIAL_PARAM+(i*16+k));
        faderParam->module=module;
        addParam(faderParam);
      }
    }
  }
};


Model *modelPad2=createModel<Pad2,Pad2Widget>("Pad2");