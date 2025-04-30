#include "dcb.h"
#include <thread>

using simd::float_4;
#define NUM_VOICES 4

#include "buffer.hpp"

template<typename T>
struct SinPOsc {
  T phs=0.f;
  T freq=261.636f;

  T sin2pi_pade_05_5_4(T x) {
    x-=0.5f;
    T x2=x*x;
    T x3=x2*x;
    T x4=x2*x2;
    T x5=x2*x3;
    return (T(-6.283185307)*x+T(33.19863968)*x3-T(32.44191367)*x5)/
           (1+T(1.296008659)*x2+T(0.7028072946)*x4);
  }

  void updatePhs(float sampleTime) {
    phs+=simd::fmin(freq*sampleTime,0.5f);
    phs-=simd::floor(phs);
  }

  T process(float sampleTime) {
    updatePhs(sampleTime);
    return sin2pi_pade_05_5_4(phs);
  }

  void reset(T trigger) {
    phs=simd::ifelse(trigger,0.f,phs);
  }
};

template<typename T,size_t P>
struct ParaSinOscBank {
  SinPOsc<T> partials[P];
  T decay=1.f;
  T oddEven=0.f;
  T stretch=0.f;
  T baseFreq=0.f;
  float combAmt=0;
  float combFreq=10;
  float combPhs=0;
  float combWarp=1;
  float combPeek=1;
  float faParam=1;
  unsigned numPartials=256;

  float cutoff=24000.f;
  int offset=0;

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

  T ampCurve(int partialNr) {
    T dmp=1.f/fastPow(partialNr+1,decay);
    if(partialNr==0) {
      dmp*=faParam;
    }
    if(partialNr%2==0) {
      dmp*=simd::ifelse(oddEven>0.f,1.0f-oddEven,1.f);
    } else {
      dmp*=simd::ifelse(oddEven<0.f,1.0f+oddEven,1.f);
    }
    if(combAmt>0.01f) {
      dmp*=powf_fast_ub(
        1+combAmt*cosf(combFreq*powf_fast_ub(TWOPIF*float(partialNr)/256.f,combWarp)+combPhs),
        combPeek);
      //dmp*=simd::pow(1+combAmt*cosf(combFreq*simd::pow(TWOPIF*float(partialNr)/256.f,combWarp)+combPhs),combPeek);
      //dmp*=sqrtf(1+combAmt*cosf(combFreq*TWOPIF*float(partialNr)/256.f+combPhs));
    }
    return dmp;
  }

  T process(float sampleTime) {
    T ret=0.f;
    for(unsigned int k=0;k<numPartials;k++) {

      T freq=baseFreq*float((k+1)+offset);
      if(k>0) {
        freq*=simd::ifelse(stretch!=0.f,fastPow(float(k),stretch),1.f);
      }
      partials[k].freq=freq;
      if(simd::movemask(freq<cutoff)) {
        T dmp=ampCurve(k);
        ret+=simd::ifelse(freq<cutoff,partials[k].process(sampleTime),0.f)*dmp;
      }
    }
    return ret;
  }

  void reset() {
    for(unsigned k=0;k<P;k++) {
      partials[k].phs=0;
    }
  }
};

template<typename O,typename T>
struct OscA1Proc {
  O osc;
  T voct=0.f;
  float freq=261.636f;
  float fmParam=0.f;
  bool run=false;
  bool linear=true;
  std::thread *thread=nullptr;
  RBufferMgr<T> bufMgr;

  //rack::dsp::RingBuffer<T,S> buffer={};
  //rack::dsp::RingBuffer<T,S> inBuffer={};
  void processThread(float sampleTime) {
    while(run) {
      if(!bufMgr.buffer->out_full()) {
        T pitch=freq+voct;
        if(linear) {
          osc.baseFreq=dsp::FREQ_C4*dsp::approxExp2_taylor5(pitch+30.f)/std::pow(2.f,30.f);
          if(!bufMgr.buffer->in_empty()) {
            osc.baseFreq+=dsp::FREQ_C4*bufMgr.buffer->in_shift()*fmParam;
          }
        } else {
          if(!bufMgr.buffer->in_empty()) {
            pitch+=bufMgr.buffer->in_shift()*fmParam;
          }
          osc.baseFreq=dsp::FREQ_C4*dsp::approxExp2_taylor5(pitch+30.f)/std::pow(2.f,30.f);
        }
        //osc.baseFreq=261.626f*simd::pow(2.0f,pitch);
        bufMgr.buffer->out_push(osc.process(sampleTime)*5.f);
      } else {
        std::this_thread::sleep_for(std::chrono::duration<double>(sampleTime*2));
      }
    }
  }

  void stopThread() {
    run=false;
  }

  void checkThread(float sampleTime) {
    if(!run) {
      run=true;
      thread=new std::thread(&OscA1Proc::processThread,this,sampleTime);
#ifdef ARCH_LIN
      std::string threadName="OSC10";
      pthread_setname_np(thread->native_handle(),threadName.c_str());
#endif
      thread->detach();
    }
  }

  T getOutput() {
    return bufMgr.buffer->out_empty()?0.f:bufMgr.buffer->out_shift();
  }

  void pushFM(T sample) {
    if(!bufMgr.buffer->in_full()) {
      bufMgr.buffer->in_push(sample);
    }
  }
};


struct OscA1 : Module {
  enum ParamId {
    FREQ_PARAM,CUTOFF_PARAM,DECAY_PARAM,ODD_EVEN_PARAM,
    OFFSET_PARAM,STRETCH_PARAM,DECAY_CV_PARAM,ODD_EVEN_CV_PARAM,STRETCH_CV_PARAM,
    AMP_PARAM,AMP_CV_PARAM,COMB_PHS_PARAM,COMB_FREQ_PARAM,COMB_AMT_PARAM,NUM_PARTIALS_PARAM,
    COMB_PHS_CV_PARAM,COMB_FREQ_CV_PARAM,COMB_AMT_CV_PARAM,COMB_WARP_PARAM,COMB_WARP_CV_PARAM,
    COMB_PEEK_PARAM,COMB_PEEK_CV_PARAM,FM_PARAM,LINEAR_FM_PARAM,FA_PARAM,RST_PARAM,
    PARAMS_LEN
  };
  enum InputId {
    VOCT_INPUT,
    CUTOFF_INPUT,
    DECAY_INPUT,
    ODD_EVEN_INPUT,
    STRETCH_INPUT,
    OFFSET_INPUT,
    AMP_INPUT,
    NUM_PARTIALS_INPUT,
    COMB_AMT_INPUT,
    COMB_PEEK_INPUT,
    COMB_FREQ_INPUT,
    COMB_WARP_INPUT,
    COMB_PHS_INPUT,
    FM_INPUT,
    INPUTS_LEN
  };
  enum OutputId {
    V_OUTPUT,OUTPUTS_LEN
  };
  enum LightId {
    LIGHTS_LEN
  };

  OscA1Proc<ParaSinOscBank<float_4,256>,float_4> sineBankOsc[NUM_VOICES];

  int bufferType=0;
  dsp::SchmittTrigger rstTrig;

  OscA1() {
    config(PARAMS_LEN,INPUTS_LEN,OUTPUTS_LEN,LIGHTS_LEN);
    configParam(FREQ_PARAM,-14.f,4.f,0.f,"Frequency"," Hz",2,dsp::FREQ_C4);
    configParam(CUTOFF_PARAM,-11.f,6.3f,6.3,"Cutoff"," Hz",2,dsp::FREQ_C4);
    configParam(NUM_PARTIALS_PARAM,1,256,128,"Num Partials");
    getParamQuantity(NUM_PARTIALS_PARAM)->snapEnabled=true;
    configParam(COMB_AMT_PARAM,0,1,0,"Comb Amp");
    configParam(COMB_PHS_PARAM,0,TWOPIF,0,"Comb Phs");
    configParam(COMB_FREQ_PARAM,1,100,10,"Comb Freq");
    configParam(COMB_WARP_PARAM,0.2,1,1,"Comb Warp");
    configParam(COMB_PEEK_PARAM,0.1,4,1,"Comb Peek");
    configParam(COMB_AMT_CV_PARAM,0,1,0,"Comb Amp CV"," %",0,100);
    configParam(COMB_FREQ_CV_PARAM,0,1,0,"Comb Freq CV"," %",0,100);
    configParam(COMB_PEEK_CV_PARAM,0,1,0,"Comb Peek CV"," %",0,100);
    configParam(COMB_WARP_CV_PARAM,0,1,0,"Comb Warp CV"," %",0,100);
    configParam(COMB_PHS_CV_PARAM,0,1,0,"Comb Phs CV"," %",0,100);
    configInput(COMB_AMT_INPUT,"Comb Amt");
    configInput(COMB_FREQ_INPUT,"Comb Freq");
    configInput(COMB_PEEK_INPUT,"Comb Peek Input");
    configInput(COMB_WARP_INPUT,"Comb Warp");
    configInput(COMB_PHS_INPUT,"Comb Phs");
    configInput(NUM_PARTIALS_INPUT,"Num Partials");
    configParam(FM_PARAM,0,3,0,"FM Amt"," %",0,100);
    configButton(LINEAR_FM_PARAM,"Linear FM");
    configInput(FM_INPUT,"FM_INPUT");
    configButton(RST_PARAM,"Reset");

    configParam(FA_PARAM,0,1,1,"Fundamental Amp");

    configParam(OFFSET_PARAM,0,128,0,"First Partial ");
    getParamQuantity(OFFSET_PARAM)->snapEnabled=true;
    configInput(OFFSET_INPUT,"First Partial ");

    configParam(ODD_EVEN_PARAM,-1,1,0,"Odd/Even ");
    configParam(ODD_EVEN_CV_PARAM,0,1,0,"Odd/Even CV ","%",0,100);
    configInput(ODD_EVEN_INPUT,"Odd/Even ");

    configParam(DECAY_PARAM,0.1,3,1,"Decay ");
    configParam(DECAY_CV_PARAM,0,1,0,"Decay CV ","%",0,100);
    configInput(DECAY_INPUT,"Decay ");

    configParam(STRETCH_PARAM,-1.25,1.25,0,"Stretch ");
    configInput(STRETCH_INPUT,"Stretch ");
    configParam(STRETCH_CV_PARAM,0,1,0,"Stretch CV ","%",0,100);

    configParam(AMP_PARAM,0,1,0.5,"Amp");
    configInput(AMP_INPUT,"Amp ");
    configParam(AMP_CV_PARAM,0,1,0,"Amp CV ","%",0,100);

    configInput(VOCT_INPUT,"V/Oct");
    configOutput(V_OUTPUT,"CV");
  }

  void setBufferSize(int s) {
    for(int k=0;k<NUM_VOICES;k++) {
      sineBankOsc[k].bufMgr.setBufferSize(s);
    }
  }

  int getBufferSize() {
    return sineBankOsc[0].bufMgr.bufferSizeIndex;
  }

  void stopAllThreads() {
    for(int k=0;k<NUM_VOICES;k++) {
      sineBankOsc[k].stopThread();
    }
  }

  ~OscA1() {
    stopAllThreads();
  }

  void onRemove() override {
    stopAllThreads();
  }

  void onReset(const ResetEvent &e) override {
    Module::onReset(e);
    for(int k=0;k<NUM_VOICES;k++) {
      sineBankOsc[k].osc.reset();
    }
  }

  void process(const ProcessArgs &args) override {
    if(rstTrig.process(params[RST_PARAM].getValue())) {
      for(int k=0;k<NUM_VOICES;k++) {
        sineBankOsc[k].osc.reset();
      }
    }
    if(inputs[NUM_PARTIALS_INPUT].isConnected()) {
      setImmediateValue(getParamQuantity(NUM_PARTIALS_PARAM),
                        clamp(inputs[NUM_PARTIALS_INPUT].getVoltage()*25.6f,1.f,256.f));
    }
    if(inputs[OFFSET_INPUT].isConnected()) {
      setImmediateValue(getParamQuantity(OFFSET_PARAM),
                        clamp(inputs[OFFSET_INPUT].getVoltage()*12.8f,1.f,128.f));
    }

    float combAmt=clamp(params[COMB_AMT_PARAM].getValue()+
                        inputs[COMB_AMT_INPUT].getVoltage()*0.1f*params[COMB_AMT_CV_PARAM].getValue());
    float combFreq=clamp(params[COMB_FREQ_PARAM].getValue()+
                         inputs[COMB_FREQ_INPUT].getVoltage()*10.f*params[COMB_FREQ_CV_PARAM].getValue(),1.f,
                         100.f);
    float combPhs=clamp(params[COMB_PHS_PARAM].getValue()+
                        inputs[COMB_PHS_INPUT].getVoltage()*params[COMB_PHS_CV_PARAM].getValue(),0.f,TWOPIF);
    float combPeek=clamp(params[COMB_PEEK_PARAM].getValue()+
                         inputs[COMB_PEEK_INPUT].getVoltage()*0.5f*params[COMB_PEEK_CV_PARAM].getValue(),
                         0.1f,4.f);
    float combWarp=clamp(params[COMB_WARP_PARAM].getValue()+
                         inputs[COMB_WARP_INPUT].getVoltage()*0.1f*params[COMB_WARP_CV_PARAM].getValue(),
                         0.2f,1.f);
    int numPartials=params[NUM_PARTIALS_PARAM].getValue();
    int offset=params[OFFSET_PARAM].getValue();
    float decay=params[DECAY_PARAM].getValue();
    float decayCV=params[DECAY_CV_PARAM].getValue();
    float stretch=params[STRETCH_PARAM].getValue();
    float stretchCV=params[STRETCH_CV_PARAM].getValue();
    float oddEven=params[ODD_EVEN_PARAM].getValue();
    float oddEvenCV=params[ODD_EVEN_CV_PARAM].getValue();
    float cutoff=261.626f*simd::pow(2.0f,params[CUTOFF_PARAM].getValue());
    float freq=params[FREQ_PARAM].getValue();
    int channels=std::max(inputs[VOCT_INPUT].getChannels(),1);
    int c=0;
    for(;c<channels;c+=4) {
      auto *osc=&sineBankOsc[c/4];
      osc->voct=inputs[VOCT_INPUT].getVoltageSimd<float_4>(c);
      osc->freq=freq;
      osc->linear=params[LINEAR_FM_PARAM].getValue()>0;
      osc->fmParam=params[FM_PARAM].getValue();
      if(inputs[FM_INPUT].isConnected()) {
        osc->pushFM(inputs[FM_INPUT].getVoltageSimd<float_4>(c));
      }
      osc->osc.cutoff=cutoff;
      osc->osc.faParam=params[FA_PARAM].getValue();
      osc->osc.decay=simd::clamp(decay+inputs[DECAY_INPUT].getPolyVoltageSimd<float_4>(c)*decayCV,0.1f,
                                 3.f);
      osc->osc.oddEven=simd::clamp(
        oddEven+inputs[ODD_EVEN_INPUT].getPolyVoltageSimd<float_4>(c)*oddEvenCV*0.1,-1.f,1.f);
      osc->osc.stretch=simd::clamp(
        stretch+inputs[STRETCH_INPUT].getPolyVoltageSimd<float_4>(c)*stretchCV*0.1f,-1.25f,1.25f);
      osc->osc.offset=offset;
      osc->osc.combAmt=combAmt;
      osc->osc.combFreq=combFreq;
      osc->osc.combPhs=combPhs;
      osc->osc.combPeek=combPeek;
      osc->osc.combWarp=combWarp;
      osc->osc.numPartials=numPartials;
      osc->checkThread(args.sampleTime);
      float_4 out=osc->getOutput()*(params[AMP_PARAM].getValue()+
                                    inputs[AMP_INPUT].getPolyVoltageSimd<float_4>(c)*
                                    params[AMP_CV_PARAM].getValue());
      outputs[V_OUTPUT].setVoltageSimd(out,c);
    }
    outputs[V_OUTPUT].setChannels(channels);
  }

  json_t *dataToJson() override {
    json_t *data=json_object();
    json_object_set_new(data,"bufferSizeIndex",json_integer(getBufferSize()));
    return data;
  }

  void dataFromJson(json_t *rootJ) override {
    json_t *jBufferSizeIndex=json_object_get(rootJ,"bufferSizeIndex");
    if(jBufferSizeIndex!=nullptr)
      setBufferSize(json_integer_value(jBufferSizeIndex));
  }
};

struct OscA1Widget : ModuleWidget {

  OscA1Widget(OscA1 *module) {
    setModule(module);
    setPanel(createPanel(asset::plugin(pluginInstance,"res/OscA1.svg")));
    float x=3;
    float x2=33;
    float y=14;
    addParam(createParam<TrimbotWhite9>(mm2px(Vec(10,11)),module,OscA1::FREQ_PARAM));
    addParam(createParam<TrimbotWhite9>(mm2px(Vec(x2,11)),module,OscA1::CUTOFF_PARAM));

    addParam(createParam<TrimbotWhite9>(mm2px(Vec(10,27)),module,OscA1::NUM_PARTIALS_PARAM));
    addInput(createInput<SmallPort>(mm2px(Vec(x,28)),module,OscA1::NUM_PARTIALS_INPUT));

    addParam(createParam<TrimbotWhite9>(mm2px(Vec(x2,27)),module,OscA1::OFFSET_PARAM));
    addInput(createInput<SmallPort>(mm2px(Vec(x2+10,28)),module,OscA1::OFFSET_INPUT));

    y=47;
    float xc=3;
    addParam(createParam<TrimbotWhite>(mm2px(Vec(xc,y)),module,OscA1::COMB_AMT_PARAM));
    addInput(createInput<SmallPort>(mm2px(Vec(xc,y+7)),module,OscA1::COMB_AMT_INPUT));
    addParam(createParam<TrimbotWhite>(mm2px(Vec(xc,y+14)),module,OscA1::COMB_AMT_CV_PARAM));
    xc+=10;
    addParam(createParam<TrimbotWhite>(mm2px(Vec(xc,y)),module,OscA1::COMB_FREQ_PARAM));
    addInput(createInput<SmallPort>(mm2px(Vec(xc,y+7)),module,OscA1::COMB_FREQ_INPUT));
    addParam(createParam<TrimbotWhite>(mm2px(Vec(xc,y+14)),module,OscA1::COMB_FREQ_CV_PARAM));
    xc+=10;
    addParam(createParam<TrimbotWhite>(mm2px(Vec(xc,y)),module,OscA1::COMB_PHS_PARAM));
    addInput(createInput<SmallPort>(mm2px(Vec(xc,y+7)),module,OscA1::COMB_PHS_INPUT));
    addParam(createParam<TrimbotWhite>(mm2px(Vec(xc,y+14)),module,OscA1::COMB_PHS_CV_PARAM));
    xc+=10;
    addParam(createParam<TrimbotWhite>(mm2px(Vec(xc,y)),module,OscA1::COMB_WARP_PARAM));
    addInput(createInput<SmallPort>(mm2px(Vec(xc,y+7)),module,OscA1::COMB_WARP_INPUT));
    addParam(createParam<TrimbotWhite>(mm2px(Vec(xc,y+14)),module,OscA1::COMB_WARP_CV_PARAM));
    xc+=10;
    addParam(createParam<TrimbotWhite>(mm2px(Vec(xc,y)),module,OscA1::COMB_PEEK_PARAM));
    addInput(createInput<SmallPort>(mm2px(Vec(xc,y+7)),module,OscA1::COMB_PEEK_INPUT));
    addParam(createParam<TrimbotWhite>(mm2px(Vec(xc,y+14)),module,OscA1::COMB_PEEK_CV_PARAM));

    y=78;
    addParam(createParam<TrimbotWhite9>(mm2px(Vec(10,y)),module,OscA1::STRETCH_PARAM));
    addParam(createParam<TrimbotWhite>(mm2px(Vec(x,y-2)),module,OscA1::STRETCH_CV_PARAM));
    addInput(createInput<SmallPort>(mm2px(Vec(x,y+5)),module,OscA1::STRETCH_INPUT));

    addParam(createParam<TrimbotWhite9>(mm2px(Vec(x2,y)),module,OscA1::ODD_EVEN_PARAM));
    addParam(createParam<TrimbotWhite>(mm2px(Vec(x2+10,y-2)),module,OscA1::ODD_EVEN_CV_PARAM));
    addInput(createInput<SmallPort>(mm2px(Vec(x2+10,y+5)),module,OscA1::ODD_EVEN_INPUT));

    y=97;
    addParam(createParam<TrimbotWhite9>(mm2px(Vec(10,y)),module,OscA1::DECAY_PARAM));
    addParam(createParam<TrimbotWhite>(mm2px(Vec(x,y-2)),module,OscA1::DECAY_CV_PARAM));
    addInput(createInput<SmallPort>(mm2px(Vec(x,y+5)),module,OscA1::DECAY_INPUT));

    addParam(createParam<TrimbotWhite9>(mm2px(Vec(x2,y)),module,OscA1::AMP_PARAM));
    addParam(createParam<TrimbotWhite>(mm2px(Vec(x2+10,y-2)),module,OscA1::AMP_CV_PARAM));
    addInput(createInput<SmallPort>(mm2px(Vec(x2+10,y+5)),module,OscA1::AMP_INPUT));

    addParam(createParam<TrimbotWhite>(mm2px(Vec(24,83)),module,OscA1::FA_PARAM));

    addParam(createParam<TrimbotWhite>(mm2px(Vec(24,100)),module,OscA1::FM_PARAM));
    addParam(createParam<MLED>(mm2px(Vec(24,108)),module,OscA1::LINEAR_FM_PARAM));
    addInput(createInput<SmallPort>(mm2px(Vec(24,116)),module,OscA1::FM_INPUT));


    addInput(createInput<SmallPort>(mm2px(Vec(3,116)),module,OscA1::VOCT_INPUT));
    addParam(createParam<MLEDM>(mm2px(Vec(14,116)),module,OscA1::RST_PARAM));
    addOutput(createOutput<SmallPort>(mm2px(Vec(43,116)),module,OscA1::V_OUTPUT));

  }

  void appendContextMenu(Menu *menu) override {
    auto *module=dynamic_cast<OscA1 *>(this->module);
    assert(module);

    menu->addChild(new MenuSeparator);

    auto bufSizeSelect=new BufferSizeSelectItem<OscA1>(module);
    bufSizeSelect->text="Thread buffer size";
    bufSizeSelect->rightText=bufSizeSelect->labels[module->getBufferSize()]+"  "+RIGHT_ARROW;
    menu->addChild(bufSizeSelect);
  }
};


Model *modelOscA1=createModel<OscA1,OscA1Widget>("OscA1");