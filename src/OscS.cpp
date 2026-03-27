#include "dcb.h"
#include <thread>

#include "worker.h"
using simd::float_4;
#define NUM_VOICES 4
#include "buffer.hpp"
template<typename T, size_t P>
struct SawOsc {
  T phs = 0.f;
  T baseFreq = 261.636f;
  T cutoffFreq = 22500.f;
  float inv_s[P]={};
  SawOsc()
  {
    for(unsigned i=0; i<P; i++) inv_s[i] = 1.0f / static_cast<float>(i + 1);
  }

  void updatePhs(float sampleTime) {
    phs += simd::fmin(baseFreq * sampleTime, 0.5f);
    phs -= simd::floor(phs); // phs is strictly [0.0, 1.0)
  }

  // The blazing fast SIMD Sine Approximation
  // Expects 'x' to be in the range [-1.0, 1.0]
  inline T fast_sin(T x) {
    T y = 4.f * x - 4.f * x * simd::fabs(x);
    return 0.225f * (y * simd::fabs(y) - y) + y;
  }

  T process(float sampleTime) {
    updatePhs(sampleTime);
    T out = 0.f;

    for (unsigned int i = 0; i < P; i++) {
      T s = T(i + 1);
      T current_freq = baseFreq * s;

      // The essential early exit
      if (simd::movemask(current_freq < cutoffFreq) == 0) {
        break; // Stop computing high partials instantly!
      }

      // 1. Calculate phase for THIS partial
      T p = phs * s;

      // 2. Wrap the phase to [0.0, 1.0)
      p -= simd::floor(p);

      // 3. Shift phase to [-1.0, 1.0) for the fast sine math
      p = p * 2.f - 1.f;

      // 4. Compute the sine and scale by 1/s (Inverse Harmonic)
      T sine_val = fast_sin(p) * inv_s[i];

      // 5. Add to output only if below Nyquist
      out += simd::ifelse(current_freq < cutoffFreq, sine_val, 0.f);
    }
    return out;
  }
};
template<typename T,size_t P>
struct SawOsc1 {
  T phs=0.f;
  T baseFreq=261.636f;
  T cutoffFreq=22500.f;
  float inv_s[P];
  SawOsc1() {
    for(unsigned i=0; i<P; i++) inv_s[i] = 1.0f / static_cast<float>(i + 1);
  }
  void updatePhs(float sampleTime) {
    phs+=simd::fmin(baseFreq*sampleTime,0.5f);
    phs-=simd::floor(phs);
  }
  inline T fast_sin(T x) {
    T y = 4.f * x - 4.f * x * simd::fabs(x);
    return 0.225f * (y * simd::fabs(y) - y) + y;
  }
  T process(float sampleTime) {
    updatePhs(sampleTime);
    T out=0;
    T phs_rad = phs * TWOPIF;
    for(unsigned int i=0;i<P;i++) {
      T s=T(i+1);
      if(simd::movemask(baseFreq*s<cutoffFreq)) {
        out+=simd::ifelse(baseFreq*s<cutoffFreq,simd::sin(phs_rad*s)*inv_s[i],0.f);
      } else {
        break;
      }
    }
    return out;
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

  OscWorker<SawOsc<float_4,512>,float_4> sawOscProc[4];
  SawOsc<float_4,512> sawOsc[4];
  bool useThread=false;
	OscS() {
    config(PARAMS_LEN, INPUTS_LEN, OUTPUTS_LEN, LIGHTS_LEN);
    configParam(FREQ_PARAM,-4.f,4.f,0.f,"Frequency"," Hz",2,dsp::FREQ_C4);
    configParam(FINE_PARAM,-0.1,0.1,0,"Fine");
    configInput(VOCT_INPUT,"V/Oct");
    configOutput(SAW_OUTPUT,"SAW");
	}
  void setBufferSize(int s) {
    for(int k=0;k<NUM_VOICES;k++) {
      sawOscProc[k].setChunkSizeIndex(s);
    }
  }
  int getBufferSize() {
    return sawOscProc[0].getChunkSizeIndex();
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
	  if(useThread) {
	    processT(args);
	  } else {
	    process1(args);
	  }
	}

  void process1(const ProcessArgs& args) {
	  float freq=params[FREQ_PARAM].getValue();
	  float fine=params[FINE_PARAM].getValue();
	  int channels=std::max(inputs[VOCT_INPUT].getChannels(),1);
	  int c=0;
	  for(;c<channels;c+=4) {
	    auto *osc=&sawOsc[c/4];
	    float_4 pitch=freq+fine+inputs[VOCT_INPUT].getVoltageSimd<float_4>(c);
	    osc->baseFreq=dsp::FREQ_C4*dsp::approxExp2_taylor5(pitch+30.f)/std::pow(2.f,30.f);
	    outputs[SAW_OUTPUT].setVoltageSimd(osc->process(args.sampleTime)* 5.f,c);
	  }
	  outputs[SAW_OUTPUT].setChannels(channels);
	}

	void processT(const ProcessArgs& args)  {
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
    json_object_set_new(data,"bufferSizeIndex",json_integer(getBufferSize()));
	  json_object_set_new(data,"useThread",json_boolean(useThread));
    return data;
  }

  void dataFromJson(json_t *rootJ) override {
    json_t *jBufferSizeIndex =json_object_get(rootJ,"bufferSizeIndex");
    if(jBufferSizeIndex!=nullptr)
      setBufferSize(json_integer_value(jBufferSizeIndex));
	  json_t *jUseThread =json_object_get(rootJ,"useThread");
	  if(jUseThread!=nullptr)
	    useThread=json_boolean_value(jUseThread);
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
	  menu->addChild(createBoolPtrMenuItem("use thread","",&module->useThread));
  }
};


Model* modelOscS = createModel<OscS, OscSWidget>("OscS");