#include "dcb.h"
#include <thread>
using simd::float_4;
#define NUM_VOICES 4
#include "buffer.hpp"

template<typename T,size_t P>
struct SawOsc {
  T phs=0.f;
  T currentFreq=261.636f;
  T cutoffFreq=22500.f;
  void updatePhs(float sampleTime,T freq) {
    phs+=simd::fmin(freq*sampleTime,0.5f);
    phs-=simd::floor(phs);
    currentFreq=freq;
  }
  T process() {
    T out=0;
    for(unsigned int i=0;i<P;i++) {
      T s=T(i+1);
      if(simd::movemask(currentFreq*s<cutoffFreq)) {
        out+=simd::ifelse(currentFreq*s<cutoffFreq,simd::sin(phs*s*TWOPIF)/s,0.f);
      }
    }
    return out;
  }
};

template<typename O,typename T>
struct SawOscProc {
  O osc;
  T voct=0.f;
  float freq=0;
  bool run=false;
  std::thread *thread=nullptr;
  RBufferMgr<float_4> bufMgr;
  void processThread(float sampleTime) {
    while(run) {
      if(!bufMgr.buffer->out_full()) {
        T pitch=freq+voct;
        T frq=dsp::FREQ_C4*dsp::approxExp2_taylor5(pitch+30.f)/std::pow(2.f,30.f);
        osc.updatePhs(sampleTime,frq);
        bufMgr.buffer->out_push(osc.process()*2.5f);
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
      thread=new std::thread(&SawOscProc::processThread,this,sampleTime);
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

struct OscS : Module {
  enum ParamId {
    FREQ_PARAM, FINE_PARAM, PARAMS_LEN
  };
  enum InputId {
    VOCT_INPUT,INPUTS_LEN
  };
  enum OutputId {
    SAW_OUTPUT,OUTPUTS_LEN
  };
  enum LightId {
    LIGHTS_LEN
  };

  SawOscProc<SawOsc<float_4,512>,float_4> sawOscProc[4];

	OscS() {
    config(PARAMS_LEN, INPUTS_LEN, OUTPUTS_LEN, LIGHTS_LEN);
    configParam(FREQ_PARAM,-4.f,4.f,0.f,"Frequency"," Hz",2,dsp::FREQ_C4);
    configParam(FINE_PARAM,-0.1,0.1,0,"Fine");
    configInput(VOCT_INPUT,"V/Oct");
    configOutput(SAW_OUTPUT,"SAW");
	}
  void setBufferSize(int s) {
    for(int k=0;k<NUM_VOICES;k++) {
      sawOscProc[k].bufMgr.setBufferSize(s);
    }
  }
  int getBufferSize() {
    return sawOscProc[0].bufMgr.bufferSizeIndex;
  }
  void stopAllThreads() {
    for(int k=0;k<NUM_VOICES;k++) {
      sawOscProc[k].stopThread();
    }
  }

  ~OscS() {
    stopAllThreads();
  }

  void onRemove() override {
    stopAllThreads();
  }

	void process(const ProcessArgs& args) override {
    float freq=params[FREQ_PARAM].getValue();
    float fine=params[FINE_PARAM].getValue();
    int channels=std::max(inputs[VOCT_INPUT].getChannels(),1);
    int c=0;
    for(;c<channels;c+=4) {
      auto *osc=&sawOscProc[c/4];
      osc->voct=inputs[VOCT_INPUT].getVoltageSimd<float_4>(c);
      osc->freq=freq+fine;
      osc->checkThread(args.sampleTime);
      outputs[SAW_OUTPUT].setVoltageSimd(osc->getOutput(),c);
    }
    outputs[SAW_OUTPUT].setChannels(channels);
	}

  json_t *dataToJson() override {
    json_t *data=json_object();
    json_object_set_new(data,"bufferSizeIndex",json_integer(sawOscProc[0].bufMgr.bufferSizeIndex));
    return data;
  }

  void dataFromJson(json_t *rootJ) override {
    json_t *jBufferSizeIndex =json_object_get(rootJ,"bufferSizeIndex");
    if(jBufferSizeIndex!=nullptr)
      setBufferSize(json_integer_value(jBufferSizeIndex));
  }
};


struct OscSWidget : ModuleWidget {
	OscSWidget(OscS* module) {
		setModule(module);
		setPanel(createPanel(asset::plugin(pluginInstance, "res/OscS.svg")));

    float x=1.9;
    addParam(createParam<TrimbotWhite>(mm2px(Vec(x,9)),module,OscS::FREQ_PARAM));
    addParam(createParam<TrimbotWhite>(mm2px(Vec(x,21)),module,OscS::FINE_PARAM));
    addInput(createInput<SmallPort>(mm2px(Vec(x,33)),module,OscS::VOCT_INPUT));
    addOutput(createOutput<SmallPort>(mm2px(Vec(x,116)),module,OscS::SAW_OUTPUT));
  }

  void appendContextMenu(Menu *menu) override {
    auto *module=dynamic_cast<OscS *>(this->module);
    assert(module);

    menu->addChild(new MenuSeparator);

    auto bufSizeSelect=new BufferSizeSelectItem<OscS>(module);
    bufSizeSelect->text="Thread buffer size";
    bufSizeSelect->rightText=bufSizeSelect->labels[module->getBufferSize()]+"  "+RIGHT_ARROW;
    menu->addChild(bufSizeSelect);
  }
};


Model* modelOscS = createModel<OscS, OscSWidget>("OscS");