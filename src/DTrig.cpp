#include "dcb.h"
using simd::float_4;
template<typename T>
struct Detector {
  float window=0.f;
  T last=0.f;
  T process(T in) {
    T mask = window==0.f?in!=last:(in<last+window)&(in>last-window);
    last=in;
    return mask;
  }
};

struct DTrig : Module {
	enum ParamId {
		PARAMS_LEN=3
	};
	enum InputId {
		INPUTS_LEN=3
	};
	enum OutputId {
		OUTPUTS_LEN=3
	};
	enum LightId {
		LIGHTS_LEN
	};

  Detector<float_4> detectors[3][4];

	DTrig() {
		config(PARAMS_LEN, INPUTS_LEN, OUTPUTS_LEN, LIGHTS_LEN);
    for(int k=0;k<3;k++) {
      configParam(k,0,1,0,"Window");
      configInput(k,"CV");
      configOutput(k,"CV");
    }
	}

	void process(const ProcessArgs& args) override {
    for(int k=0;k<3;k++) {
      int channels=inputs[k].getChannels();
      float window=params[k].getValue();
      for(int chn=0;chn<channels;chn+=4) {
        detectors[k][chn/4].window=window;
        float_4 mask=detectors[k][chn/4].process(inputs[k].getVoltageSimd<float_4>(chn));
        outputs[k].setVoltageSimd(simd::ifelse(mask,10.f,0.f),chn);
      }
      outputs[k].setChannels(channels);
    }
	}
};


struct DTrigWidget : ModuleWidget {
	DTrigWidget(DTrig* module) {
		setModule(module);
		setPanel(createPanel(asset::plugin(pluginInstance, "res/DTrig.svg")));
    float x=1.9;
    float y=10.f;
		for(int k=0;k<3;k++) {
      addParam(createParam<TrimbotWhite>(mm2px(Vec(x,y)),module,k));
      y+=12;
      addInput(createInput<SmallPort>(mm2px(Vec(x,y)),module,k));
      y+=12;
      addOutput(createOutput<SmallPort>(mm2px(Vec(x,y)),module,k));
      y+=17;
    }
	}
};


Model* modelDTrig = createModel<DTrig, DTrigWidget>("DTrig");