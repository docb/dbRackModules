#include "dcb.h"
#include <thread>
using simd::float_4;
#define NUM_VOICES 4
#include "buffer.hpp"
template<typename T, size_t P>
struct SqrOsc {
  T phs=0.f;
  T currentFreq=261.636f;
  T cutoffFreq=24000.f;
  T pw = 0.5f;
  void updatePhs(float sampleTime,T freq) {
    phs+=simd::fmin(freq*sampleTime,0.5f);
    phs-=simd::floor(phs);
    currentFreq=freq;
  }
  T process() {
    T out=0;
    for(unsigned int k=0;k<P;k++) {
      if(simd::movemask(currentFreq*(k+1)<cutoffFreq))
        out+=simd::ifelse(currentFreq*(k+1)>cutoffFreq,0.f,simd::cos(phs*2*(k+1)*M_PI)*2*simd::sin(pw*T(k+1)*M_PI)/(T(k+1)*M_PI));
    }
    return out;
  }
  void setPW(T v) {
    pw=v;
  }
};
template<typename O,typename T>
struct SqrOscProc {
  O osc;
  T voct=0.f;
  float freq=0;
  T pw=0.5f;
  bool run=false;
  std::thread *thread=nullptr;
  RBufferMgr<float_4> bufMgr;
  void processThread(float sampleTime) {
    while(run) {
      if(!bufMgr.buffer->out_full()) {
        T pitch=freq+voct;
        T frq=dsp::FREQ_C4*dsp::approxExp2_taylor5(pitch+30.f)/std::pow(2.f,30.f);
        osc.setPW(pw);
        osc.updatePhs(sampleTime,frq);
        bufMgr.buffer->out_push(osc.process()*8.f);
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
      thread=new std::thread(&SqrOscProc::processThread,this,sampleTime);
#ifdef ARCH_LIN
      std::string threadName = "OSC11";
      pthread_setname_np(thread->native_handle(),threadName.c_str());
#endif
      thread->detach();
    }
  }
  T getOutput() {
    return bufMgr.buffer->out_empty()?0.f:bufMgr.buffer->out_shift();
  }
};

struct OscP : Module {
  enum ParamId {
    FREQ_PARAM, PW_PARAM,PW_CV_PARAM,FINE_PARAM,PARAMS_LEN
  };
  enum InputId {
    VOCT_INPUT,PW_INPUT,INPUTS_LEN
  };
  enum OutputId {
    SQR_OUTPUT,OUTPUTS_LEN
  };
  enum LightId {
    LIGHTS_LEN
  };
  SqrOscProc<SqrOsc<float_4,512>,float_4> sqrOscProc[4];

	OscP() {
		config(PARAMS_LEN, INPUTS_LEN, OUTPUTS_LEN, LIGHTS_LEN);
    configParam(PW_PARAM,0.01,0.99,0.5f, "Pulse Width","%",0,100);
    configParam(FREQ_PARAM,-4.f,4.f,0.f,"Frequency"," Hz",2,dsp::FREQ_C4);
    configParam(FINE_PARAM,-0.1,0.1,0,"Fine");
    configInput(VOCT_INPUT,"V/Oct");
    configInput(PW_INPUT,"Pulse Width");
    configOutput(SQR_OUTPUT,"SQR");
	}
  void setBufferSize(int s) {
    for(int k=0;k<NUM_VOICES;k++) {
      sqrOscProc[k].bufMgr.setBufferSize(s);
    }
  }
  int getBufferSize() {
    return sqrOscProc[0].bufMgr.bufferSizeIndex;
  }
  void stopAllThreads() {
    for(int k=0;k<NUM_VOICES;k++) {
      sqrOscProc[k].stopThread();
    }
  }

  ~OscP() {
    stopAllThreads();
  }

  void onRemove() override {
    stopAllThreads();
  }

	void process(const ProcessArgs& args) override {
    float freq=params[FREQ_PARAM].getValue();
    float pw=params[PW_PARAM].getValue();
    float fine=params[FINE_PARAM].getValue();
    int channels=std::max(inputs[VOCT_INPUT].getChannels(),1);
    int c=0;
    for(;c<channels;c+=4) {
      auto *osc=&sqrOscProc[c/4];
      osc->voct=inputs[VOCT_INPUT].getVoltageSimd<float_4>(c);
      osc->pw=simd::clamp(pw+inputs[PW_INPUT].getVoltageSimd<float_4>(c)*params[PW_CV_PARAM].getValue()*0.1f);
      osc->freq=freq+fine;
      osc->checkThread(args.sampleTime);
      outputs[SQR_OUTPUT].setVoltageSimd(osc->getOutput(),c);
    }
    outputs[SQR_OUTPUT].setChannels(channels);
	}

  json_t *dataToJson() override {
    json_t *data=json_object();
    json_object_set_new(data,"bufferSizeIndex",json_integer(sqrOscProc[0].bufMgr.bufferSizeIndex));
    return data;
  }

  void dataFromJson(json_t *rootJ) override {
    json_t *jBufferSizeIndex =json_object_get(rootJ,"bufferSizeIndex");
    if(jBufferSizeIndex!=nullptr)
      setBufferSize(json_integer_value(jBufferSizeIndex));
  }
};


struct OscPWidget : ModuleWidget {
	OscPWidget(OscP* module) {
		setModule(module);
		setPanel(createPanel(asset::plugin(pluginInstance, "res/OscP.svg")));
    float x=1.9;
    addParam(createParam<TrimbotWhite>(mm2px(Vec(x,9)),module,OscP::FREQ_PARAM));
    addParam(createParam<TrimbotWhite>(mm2px(Vec(x,21)),module,OscP::FINE_PARAM));
    addInput(createInput<SmallPort>(mm2px(Vec(x,33)),module,OscP::VOCT_INPUT));
    addParam(createParam<TrimbotWhite>(mm2px(Vec(x,46)),module,OscP::PW_PARAM));
    addInput(createInput<SmallPort>(mm2px(Vec(x,54)),module,OscP::PW_INPUT));
    addParam(createParam<TrimbotWhite>(mm2px(Vec(x,62)),module,OscP::PW_CV_PARAM));
    addOutput(createOutput<SmallPort>(mm2px(Vec(x,116)),module,OscP::SQR_OUTPUT));
	}
  void appendContextMenu(Menu *menu) override {
    auto *module=dynamic_cast<OscP *>(this->module);
    assert(module);

    menu->addChild(new MenuSeparator);

    auto bufSizeSelect=new BufferSizeSelectItem<OscP>(module);
    bufSizeSelect->text="Thread buffer size";
    bufSizeSelect->rightText=bufSizeSelect->labels[module->getBufferSize()]+"  "+RIGHT_ARROW;
    menu->addChild(bufSizeSelect);
  }
};


Model* modelOscP = createModel<OscP, OscPWidget>("OscP");