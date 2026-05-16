#include "dcb.h"
#include <thread>

#include "worker.h"
using simd::float_4;
#define NUM_VOICES 4
template<typename T, size_t P>
struct SawOsc {
  T phs = 0.f;
  T pp[P];
  T baseFreq = 261.636f;
  T cutoffFreq = 22500.f;
  float inv_s[10][512] = {{0.0f}};
  RND rnd;
  //float inv_s[P]={};
  SawOsc() {
    reset(0,0,0);
    for (int i = 0; i < 10; ++i) {
      int maxPartials = 1 << i;

      int taperStart = maxPartials - (maxPartials / 2);
      auto taperLength = static_cast<float>(maxPartials - taperStart);

      for (int n = 1; n <= maxPartials; ++n) {
        float amp = 1.0f / static_cast<float>(n);

        if (n > taperStart && taperLength > 0.0f) {
          float phase = static_cast<float>(n - taperStart) / taperLength;
          amp *= std::cos(phase * 1.570796f); // Apply the Cosine taper
        }

        inv_s[i][n - 1] = amp;
      }
    }
  }

  void reset(int mode=0, float param=0.5f, float seed=0.f) {
    uint64_t s=0;
    if(seed!=0.f) {
      s=static_cast<uint64_t>(seed*1000000);
    }
    rnd.reset(s);
    switch(mode) {
    case 0:
    default:
      for(unsigned k=0; k<P; k++) {
        pp[k]=0.f;
      }
      break;
    case 1:
      for(unsigned k=0; k<P; k++) {
        pp[k]=rnd.nextWeibull(1+param*20);
      }
      break;
    case 2:
      for(unsigned k=0; k<P; k++) {
        pp[k]=rnd.nextCauchy(param);
      }
      break;
    case 3:
      for(unsigned k=0; k<P; k++) {
        pp[k]=rnd.nextTri(static_cast<int>(param*20.f));
      }
      break;
    }
  }


  void updatePhs(float sampleTime) {
    phs += simd::fmin(baseFreq * sampleTime, 0.5f);
    phs -= simd::floor(phs);
  }

  T fast_sin(T x) {
    T y = 4.f * x - 4.f * x * simd::fabs(x);
    return 0.225f * (y * simd::fabs(y) - y) + y;
  }
  unsigned findMSB(unsigned n) {
    int position = -1; // Initialize position
    while (n > 0) {
      n >>= 1; // Right shift the number
      position++; // Increment position
    }
    return position; // Return the position of the MSB
  }
  T process(float sampleTime, unsigned numPartials=P) {
    unsigned k=findMSB(numPartials);
    updatePhs(sampleTime);
    T out = 0.f;

    for (unsigned i = 0; i < numPartials; i++) {
      T s = T(i + 1);
      T current_freq = baseFreq * s;

      if (simd::movemask(current_freq < cutoffFreq) == 0) {
        break; // Stop computing high partials instantly!
      }

      // Calculate phase for THIS partial
      T p = (phs+pp[i]) * s;

      // Wrap the phase to [0.0, 1.0)
      p -= simd::floor(p);

      // Shift phase to [-1.0, 1.0) for the fast sine math
      p = p * 2.f - 1.f;

      // Compute the sine and scale by 1/s (Inverse Harmonic)
      T sine_val = fast_sin(p) * inv_s[k][i];

      // Add to output only if below Nyquist
      out += simd::ifelse(current_freq < cutoffFreq, sine_val, 0.f);
    }
    return out;
  }
};

struct OscS : Module {
  enum ParamId {
    FREQ_PARAM, FINE_PARAM,RST_PARAM,PHASE_PARAM,PARTIALS_PARAM,SEED_PARAM, RND_PARAM,PARAMS_LEN
  };
  enum InputId {
    VOCT_INPUT,RST_INPUT,INPUTS_LEN
  };
  enum OutputId {
    SAW_OUTPUT,OUTPUTS_LEN
  };
  enum LightId {
    LIGHTS_LEN
  };
  dsp::SchmittTrigger rstTrig;
  dsp::SchmittTrigger rstInputTrig;
  OscWorker<SawOsc<float_4,512>,float_4> sawOscProc[4];
  SawOsc<float_4,512> sawOsc[4];
  bool useThread=false;
	OscS() {
    config(PARAMS_LEN, INPUTS_LEN, OUTPUTS_LEN, LIGHTS_LEN);
    configParam(FREQ_PARAM,-8.f,4.f,0.f,"Frequency"," Hz",2,dsp::FREQ_C4);
    configParam(FINE_PARAM,-0.1,0.1,0,"Fine");
    configParam(SEED_PARAM,0,1,0,"Seed");
    configParam(RND_PARAM,0,1,0,"Dist");
	  configButton(RST_PARAM, "Reset");
	  configParam(PARTIALS_PARAM,0,9,8,"Partials","",2,1);
	  getParamQuantity(PARTIALS_PARAM)->snapEnabled=true;
	  configInput(RST_INPUT, "Reset");
	  configSwitch(PHASE_PARAM, 0, 3, 0, "Phase reset mode", {"Zero", "Weibull", "Cauchy","Normal"});
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
	  bool rst=rstTrig.process(params[RST_PARAM].getValue());
	  bool rstIn=rstInputTrig.process(inputs[RST_INPUT].getVoltage());
	  if(rst||rstIn) {
	    for(int k=0; k<NUM_VOICES; k++) {
	      sawOsc[k].reset(static_cast<int>(params[PHASE_PARAM].getValue()), params[RND_PARAM].getValue(), params[SEED_PARAM].getValue());
	    }
	  }
	  int channels=std::max(inputs[VOCT_INPUT].getChannels(),1);
	  int c=0;
	  for(;c<channels;c+=4) {
	    auto *osc=&sawOsc[c/4];
	    float_4 pitch=freq+fine+inputs[VOCT_INPUT].getVoltageSimd<float_4>(c);
	    osc->baseFreq=dsp::FREQ_C4*dsp::approxExp2_taylor5(pitch+30.f)/std::pow(2.f,30.f);
	    outputs[SAW_OUTPUT].setVoltageSimd(osc->process(args.sampleTime,1 << static_cast<size_t>(params[PARTIALS_PARAM].getValue()))* 5.f,c);
	  }
	  outputs[SAW_OUTPUT].setChannels(channels);
	}

	void processT(const ProcessArgs& args)  {
    float freq=params[FREQ_PARAM].getValue();
    float fine=params[FINE_PARAM].getValue();
	  for(int k=0; k<NUM_VOICES; k++) {
	    sawOscProc[k].numPartials=1 << static_cast<size_t>(params[PARTIALS_PARAM].getValue());
	  }
	  bool rst=rstTrig.process(params[RST_PARAM].getValue());
	  bool rstIn=rstInputTrig.process(inputs[RST_INPUT].getVoltage());
	  if(rst||rstIn) {
	    for(int k=0; k<NUM_VOICES; k++) {
	      sawOscProc[k].osc.reset(static_cast<int>(params[PHASE_PARAM].getValue()), params[RND_PARAM].getValue(), params[SEED_PARAM].getValue());
	    }
	  }
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
	  for(int k=0; k<NUM_VOICES; k++) {
	    sawOsc[k].reset(static_cast<int>(params[PHASE_PARAM].getValue()), params[RND_PARAM].getValue(), params[SEED_PARAM].getValue());
	  }
	  for(int k=0; k<NUM_VOICES; k++) {
	    sawOscProc[k].osc.reset(static_cast<int>(params[PHASE_PARAM].getValue()), params[RND_PARAM].getValue(), params[SEED_PARAM].getValue());
	  }
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
	  addParam(createParam<TrimbotWhite>(mm2px(Vec(x,45)),module,OscS::PARTIALS_PARAM));
	  addParam(createParam<TrimbotWhite>(mm2px(Vec(x,57)),module,OscS::RND_PARAM));
	  addParam(createParam<TrimbotWhite>(mm2px(Vec(x,69)),module,OscS::SEED_PARAM));
	  auto selectParam=createParam<SelectParam>(mm2px(Vec(x, 81)), module, OscS::PHASE_PARAM);
	  selectParam->box.size=Vec(20, 40);
	  selectParam->init({"0", "WB", "Chy","Nrm"});
	  addParam(selectParam);
	  addParam(createParam<MLEDM>(mm2px(Vec(x, 96.75)), module, OscS::RST_PARAM));
	  addInput(createInput<SmallPort>(mm2px(Vec(x, 105)), module, OscS::RST_INPUT));

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