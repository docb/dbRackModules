#include "dcb.h"


using simd::float_4;

#define MSIZE 16

struct X16 : Module {
  enum ParamId {
    MPARAM,PARAMS_LEN=MPARAM+MSIZE*MSIZE
  };
  enum InputId {
    CV_INPUTS,MOD_INPUTS=CV_INPUTS+MSIZE,INPUTS_LEN=MOD_INPUTS+MSIZE
  };
  enum OutputId {
    CV_OUTPUTS,OUTPUTS_LEN=CV_OUTPUTS+MSIZE
  };
	enum LightId {
		LIGHTS_LEN
	};

  bool inputConnected[MSIZE]={};
  bool outputConnected[MSIZE]={};
  bool modInputsConnected[MSIZE]={};
  dsp::ClockDivider conDivider;

	X16() {
		config(PARAMS_LEN, INPUTS_LEN, OUTPUTS_LEN, LIGHTS_LEN);
    for(int k=0;k<MSIZE;k++) {
      for(int j=0;j<MSIZE;j++) {
        configParam(k*MSIZE+j,0,1,0,rack::string::f("CV %d %d",k+1,j+1));
      }
      configInput(CV_INPUTS+k,"In " + std::to_string(k+1));
      configInput(MOD_INPUTS+k,"Mod Column " + std::to_string(k+1));
      configOutput(CV_OUTPUTS+k,"Out" + std::to_string(k+1));
    }
    conDivider.setDivision(32);
	}
  void checkConnections() {
    if(conDivider.process()) {
      for(int k=0;k<MSIZE;k++) {
        inputConnected[k]=inputs[CV_INPUTS+k].isConnected();
        modInputsConnected[k]=inputs[MOD_INPUTS+k].isConnected();
        outputConnected[k]=outputs[CV_OUTPUTS+k].isConnected();
      }
    }
  }
	void processChannel(int chn) {
    float_4 inCV[MSIZE]={};
    for(int k=0;k<MSIZE;k++) {
      if(inputConnected[k])
        inCV[k]=inputs[CV_INPUTS+k].getVoltageSimd<float_4>(chn);
    }
    for(int j=0;j<MSIZE;j++) {
      if(outputConnected[j]) {
        float_4 sum=0.f;
        for(int k=0;k<MSIZE;k++) {
          float modIn=modInputsConnected[j]?clamp(inputs[MOD_INPUTS+j].getVoltage(k)*0.1f,0.f,1.f):1.f;
          sum += params[MPARAM+k*MSIZE+j].getValue()*inCV[k]*modIn;
        }
        outputs[CV_OUTPUTS+j].setVoltageSimd(sum,chn);
      }
    }
	}

  void process(const ProcessArgs& args) override {
    checkConnections();
    int channels=1;
    for(int k=0;k<MSIZE;k++) {
      if(inputConnected[k]) {
        int chnls=inputs[CV_INPUTS+k].getChannels();
        if(chnls>channels) channels=chnls;
      }
    }
    for(int c=0;c<channels;c+=4) {
      processChannel(c);
    }
    for(int j=0;j<MSIZE;j++) {
      if(outputConnected[j]) outputs[CV_OUTPUTS+j].setChannels(channels);
    }
  }
};


struct X16Widget : ModuleWidget {
	X16Widget(X16* module) {
		setModule(module);
		setPanel(createPanel(asset::plugin(pluginInstance, "res/X16.svg")));

    float x=2;
    float y=8.5;
    for(int j=0;j<MSIZE;j++) {
      addInput(createInput<SmallPort>(mm2px(Vec(x,y)),module,X16::CV_INPUTS+j));
      y+=7;
    }
    x=10;y=1;
    for(int j=0;j<MSIZE;j++) {
      addInput(createInput<SmallPort>(mm2px(Vec(x,y)),module,X16::MOD_INPUTS+j));
      x+=7;
    }
    y=8.5;
    for(int k=0;k<MSIZE;k++) {
      x=10;
      for(int j=0;j<MSIZE;j++) {
        addParam(createParam<ColorKnob>(mm2px(Vec(x,y)),module,X16::MPARAM+k*MSIZE+j));
        x+=7;
      }
      y+=7;
    }

    x=10;y=121;
    for(int j=0;j<MSIZE;j++) {
      addOutput(createOutput<SmallPort>(mm2px(Vec(x,y)),module,X16::CV_OUTPUTS+j));
      x+=7;
    }

	}
};


Model* modelX16 = createModel<X16, X16Widget>("X16");