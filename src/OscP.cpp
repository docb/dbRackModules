#include "dcb.h"
#include <thread>
#include "worker.h"
using simd::float_4;
#define NUM_VOICES 4

template<typename T, size_t P>
struct SqrOsc {
	T phs = 0.f;
	T baseFreq = 261.636f;
	T cutoffFreq = 24000.f;
	T pw = 0.5f;
	float pp[P];
	float inv_s[10][512] = {{0.0f}};
  RND rnd;
	// Precomputed inverse pi array to eliminate the division!
	float inv_s_pi[P];

	SqrOsc() {
		for (unsigned int i = 0; i < P; i++) {
			inv_s_pi[i] = 1.0f / (float(i + 1) * M_PI);
			pp[i]=0;
		}
		for (int i = 0; i < 10; ++i) {
			int maxPartials = 1 << i;

			int taperStart = maxPartials - (maxPartials / 2);
			auto taperLength = static_cast<float>(maxPartials - taperStart);

			for (int n = 1; n <= maxPartials; ++n) {
				float amp = 1.0f / (static_cast<float>(n)*M_PIf);

				if (n > taperStart && taperLength > 0.0f) {
					float phase = static_cast<float>(n - taperStart) / taperLength;
					amp *= std::cos(phase * 1.570796f); // Apply the Cosine taper
				}

				inv_s[i][n - 1] = amp;
			}
		}
	}
	void reset(int mode) {
		switch(mode) {
		case 0:
		default:
			for(unsigned k=0; k<P; k++) {
				pp[k]=0.f;
			}
			break;
		case 1:
			for(unsigned k=0; k<P; k++) {
				pp[k]=rnd.nextWeibull(15);
			}
			break;
		case 2:
			for(unsigned k=0; k<P; k++) {
				pp[k]=rnd.nextCauchy(0.5);
			}
			break;
		}
	}
	void updatePhs(float sampleTime) {
		phs += simd::fmin(baseFreq * sampleTime, 0.5f);
		phs -= simd::floor(phs);
	}

	inline T fast_sin(T x) {
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
	T process(float sampleTime,size_t numPartials=P) {
		unsigned idx=findMSB(numPartials);
		updatePhs(sampleTime);
		T out = 0.f;

		// Calculate the two phase offsets OUTSIDE the loop
		T p1 = phs + (pw * 0.5f);
		T p2 = phs - (pw * 0.5f);

		for (unsigned int k = 0; k < numPartials; k++) {
			p1+=pp[k];p2+=pp[k];
			T s = T(k + 1);
			T current_freq = baseFreq * s;

			// THE CRUCIAL EARLY EXIT - DO NOT FORGET THE BREAK!
			if (simd::movemask(current_freq < cutoffFreq) == 0) {
				break;
			}

			// 1. Calculate Sawtooth 1 (Phase + pw/2)
			T wrap1 = p1 * s;
			wrap1 -= simd::floor(wrap1);
			wrap1 = wrap1 * 2.f - 1.f;
			T saw1 = fast_sin(wrap1);

			// 2. Calculate Sawtooth 2 (Phase - pw/2)
			T wrap2 = p2 * s;
			wrap2 -= simd::floor(wrap2);
			wrap2 = wrap2 * 2.f - 1.f;
			T saw2 = fast_sin(wrap2);

			// 3. Subtract and scale by 1 / (s * PI)
			T pulse_val = (saw1 - saw2) * inv_s[idx][k];

			out += simd::ifelse(current_freq < cutoffFreq, pulse_val, 0.f);
		}
		return out;
	}

	void setPW(T v) {
		pw = v;
	}
};

struct OscP : Module {
  enum ParamId {
    FREQ_PARAM, PW_PARAM,PW_CV_PARAM,FINE_PARAM,PHASE_PARAM,RST_PARAM,PARTIALS_PARAM,PARAMS_LEN
  };
  enum InputId {
    VOCT_INPUT,PW_INPUT,RST_INPUT,INPUTS_LEN
  };
  enum OutputId {
    SQR_OUTPUT,OUTPUTS_LEN
  };
  enum LightId {
    LIGHTS_LEN
  };
  OscWorker<SqrOsc<float_4,256>,float_4> sqrOscProc[4];
  SqrOsc<float_4,256> sqrOsc[4];
  bool useThread=false;
	dsp::SchmittTrigger rstTrig;
	dsp::SchmittTrigger rstInputTrig;
	OscP() {
		config(PARAMS_LEN, INPUTS_LEN, OUTPUTS_LEN, LIGHTS_LEN);
    configParam(PW_PARAM,0.01,0.99,0.5f, "Pulse Width","%",0,100);
    configParam(FREQ_PARAM,-8.f,4.f,0.f,"Frequency"," Hz",2,dsp::FREQ_C4);
    configParam(FINE_PARAM,-0.1,0.1,0,"Fine");
		configButton(RST_PARAM, "Reset");
		configParam(PARTIALS_PARAM,0,9,8,"Partials","",2,1);
		getParamQuantity(PARTIALS_PARAM)->snapEnabled=true;
		configInput(RST_INPUT, "Reset");
		configSwitch(PHASE_PARAM, 0, 3, 0, "Phase reset mode", {"Zero", "Weibull", "Cauchy"});

    configInput(VOCT_INPUT,"V/Oct");
		configInput(RST_INPUT,"Rst");
    configInput(PW_INPUT,"Pulse Width");
    configOutput(SQR_OUTPUT,"SQR");
	}
  void setBufferSize(int s) {
	  for(int k=0;k<NUM_VOICES;k++) {
	    sqrOscProc[k].setChunkSizeIndex(s);
	  }
	}

  size_t getBufferSize() {
	  return sqrOscProc[0].getChunkSizeIndex();
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
	  if(useThread) {
	    processT(args);
	  } else {
	    process1(args);
	  }
	}

  void process1(const ProcessArgs& args) {
	  float freq=params[FREQ_PARAM].getValue();
	  float pw=params[PW_PARAM].getValue();
	  float fine=params[FINE_PARAM].getValue();
		bool rst=rstTrig.process(params[RST_PARAM].getValue());
		bool rstIn=rstInputTrig.process(inputs[RST_INPUT].getVoltage());
		if(rst||rstIn) {
			for(int k=0; k<NUM_VOICES; k++) {
				sqrOsc[k].reset(static_cast<int>(params[PHASE_PARAM].getValue()));
			}
		}
	  int channels=std::max(inputs[VOCT_INPUT].getChannels(),1);
	  int c=0;
	  for(;c<channels;c+=4) {
	    auto *osc=&sqrOsc[c/4];
	    float_4 pitch=freq+fine+inputs[VOCT_INPUT].getVoltageSimd<float_4>(c);
	    osc->baseFreq=dsp::FREQ_C4*dsp::approxExp2_taylor5(pitch+30.f)/std::pow(2.f,30.f);
	    osc->setPW(simd::clamp(pw+inputs[PW_INPUT].getVoltageSimd<float_4>(c)*params[PW_CV_PARAM].getValue()*0.1f));
	    outputs[SQR_OUTPUT].setVoltageSimd(osc->process(args.sampleTime,1<<static_cast<unsigned>(params[PARTIALS_PARAM].getValue()))* 5.f,c);
	  }
	  outputs[SQR_OUTPUT].setChannels(channels);
	}

	void processT(const ProcessArgs& args)  {
    float freq=params[FREQ_PARAM].getValue();
    float pw=params[PW_PARAM].getValue();
    float fine=params[FINE_PARAM].getValue();
		bool rst=rstTrig.process(params[RST_PARAM].getValue());
		bool rstIn=rstInputTrig.process(inputs[RST_INPUT].getVoltage());
		if(rst||rstIn) {
			for(int k=0; k<NUM_VOICES; k++) {
				sqrOscProc[k].osc.reset(static_cast<int>(params[PHASE_PARAM].getValue()));
			}
		}
    int channels=std::max(inputs[VOCT_INPUT].getChannels(),1);
    int c=0;
    for(;c<channels;c+=4) {
      auto *osc=&sqrOscProc[c/4];
      osc->voct=inputs[VOCT_INPUT].getVoltageSimd<float_4>(c);
      osc->osc.setPW(simd::clamp(pw+inputs[PW_INPUT].getVoltageSimd<float_4>(c)*params[PW_CV_PARAM].getValue()*0.1f));
      osc->freq=freq+fine;
    	osc->numPartials=1<<static_cast<unsigned>(params[PARTIALS_PARAM].getValue());
      osc->checkThread(args.sampleTime);
      outputs[SQR_OUTPUT].setVoltageSimd(osc->getOutput(),c);
    }
    outputs[SQR_OUTPUT].setChannels(channels);
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


struct OscPWidget : ModuleWidget {
	OscPWidget(OscP* module) {
		setModule(module);
		setPanel(createPanel(asset::plugin(pluginInstance, "res/OscP.svg")));
    float x=1.9;
    addParam(createParam<TrimbotWhite>(mm2px(Vec(x,9)),module,OscP::FREQ_PARAM));
    addParam(createParam<TrimbotWhite>(mm2px(Vec(x,21)),module,OscP::FINE_PARAM));
    addInput(createInput<SmallPort>(mm2px(Vec(x,33)),module,OscP::VOCT_INPUT));
    addParam(createParam<TrimbotWhite>(mm2px(Vec(x,45)),module,OscP::PW_PARAM));
    addInput(createInput<SmallPort>(mm2px(Vec(x,53)),module,OscP::PW_INPUT));
		addParam(createParam<TrimbotWhite>(mm2px(Vec(x,61)),module,OscP::PW_CV_PARAM));
		addParam(createParam<TrimbotWhite>(mm2px(Vec(x,73)),module,OscP::PARTIALS_PARAM));
		auto selectParam=createParam<SelectParam>(mm2px(Vec(x, 84.7)), module, OscP::PHASE_PARAM);
		selectParam->box.size=Vec(20, 30);
		selectParam->init({"0", "WB", "Chy"});
		addParam(selectParam);
		addParam(createParam<MLEDM>(mm2px(Vec(x, 97)), module, OscP::RST_PARAM));
		addInput(createInput<SmallPort>(mm2px(Vec(x, 105)), module, OscP::RST_INPUT));

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
	  menu->addChild(createBoolPtrMenuItem("use thread","",&module->useThread));
  }
};


Model* modelOscP = createModel<OscP, OscPWidget>("OscP");