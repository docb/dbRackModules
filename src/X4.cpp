#include "dcb.h"

using simd::float_4;

//#define MSIZE 4
template<size_t MSIZE>
struct XX : Module {
	enum ParamId {
		M_PARAM,MOD_CV_PARAM=M_PARAM+MSIZE*MSIZE,PARAMS_LEN=MOD_CV_PARAM+MSIZE*MSIZE
	};
	enum InputId {
		M_INPUT,MOD_INPUT=M_INPUT+MSIZE,INPUTS_LEN=MOD_INPUT+MSIZE*MSIZE
	};
	enum OutputId {
		OUTPUTS_LEN=MSIZE
	};
	enum LightId {
		LIGHTS_LEN
	};
  bool inputConnected[MSIZE]={};
  bool outputConnected[MSIZE]={};
  bool modInputConnected[MSIZE*MSIZE]={};
  dsp::ClockDivider conDivider;

	XX() {
		config(PARAMS_LEN, INPUTS_LEN, OUTPUTS_LEN, LIGHTS_LEN);
    for(unsigned int k=0;k<MSIZE;k++) {
      for(unsigned int j=0;j<MSIZE;j++) {
        configParam(k*MSIZE+j,0,1,0,rack::string::f("Mix %d %d",k+1,j+1));
        configInput(MOD_INPUT+k*MSIZE+j,rack::string::f("Mod %d %d",k+1,j+1));
        configParam(MOD_CV_PARAM+k*MSIZE+j,0,1,0,rack::string::f("Mod %d %d",k+1,j+1),"%",0,100);
      }
      configInput(M_INPUT+k,"In " + std::to_string(k+1));
      configOutput(k,"Out" + std::to_string(k+1));
    }
    conDivider.setDivision(32);
	}
  void checkConnections() {
    if(conDivider.process()) {
      for(unsigned int k=0;k<MSIZE;k++) {
        inputConnected[k]=inputs[M_INPUT+k].isConnected();
        for(unsigned int j=0;j<MSIZE;j++)
          modInputConnected[k*MSIZE+j]=inputs[MOD_INPUT+k*MSIZE+j].isConnected();
        outputConnected[k]=outputs[k].isConnected();
      }
    }
  }

  void processChannel(int chn) {
    float_4 inCV[MSIZE]={};
    for(unsigned int k=0;k<MSIZE;k++) {
      if(inputConnected[k])
        //inCV[k]=this->inputs[M_INPUT+k].getVoltageSimd<simd::float_4>(chn);
       inCV[k]=float_4::load(&this->inputs[M_INPUT+k].voltages[chn]);
    }
    for(unsigned int j=0;j<MSIZE;j++) {
      if(outputConnected[j]) {
        float_4 sum=0.f;
        for(unsigned int k=0;k<MSIZE;k++) {
          float_4 param=params[M_PARAM+k*MSIZE+j].getValue();
          if(modInputConnected[k*MSIZE+j]) {
            float modParam=params[MOD_CV_PARAM+k*MSIZE+j].getValue();
            if(modParam>0) {
              float_4 modIn=float_4::load(&this->inputs[MOD_INPUT+k*MSIZE+j].voltages[chn])*0.1f;
              //float_4 modIn=this->inputs[MOD_INPUT+k*MSIZE+j].getPolyVoltageSimd<float_4>(chn)*0.1f;
              param+=(modIn*modParam);
              param=clamp(param,0.f,1.f);
            }
          }
          sum += param*inCV[k];
        }
        outputs[j].setVoltageSimd(sum,chn);
      }
    }
  }

	void process(const ProcessArgs& args) override {
    checkConnections();
    int channels=1;
    for(unsigned int k=0;k<MSIZE;k++) {
      if(inputConnected[k]) {
        int chnls=inputs[M_INPUT+k].getChannels();
        if(chnls>channels) channels=chnls;
      }
    }
    for(int c=0;c<channels;c+=4) {
      processChannel(c);
    }
    for(unsigned int j=0;j<MSIZE;j++) {
      if(outputConnected[j]) outputs[j].setChannels(channels);
    }
	}
};

#define X4 XX<4>
struct X4Widget : ModuleWidget {
	X4Widget(X4* module) {
		setModule(module);
		setPanel(createPanel(asset::plugin(pluginInstance, "res/X4.svg")));
    float x=2;
    float y=15;
    for(int j=0;j<4;j++) {
      addInput(createInput<SmallPort>(mm2px(Vec(x,y)),module,X4::M_INPUT+j));
      y+=28;
    }
    y=8;
    for(int k=0;k<4;k++) {
      x=10;
      for(int j=0;j<4;j++) {
        addParam(createParam<TrimbotWhite>(mm2px(Vec(x,y)),module,X4::M_PARAM+k*4+j));
        addInput(createInput<SmallPort>(mm2px(Vec(x,y+7)),module,X4::MOD_INPUT+k*4+j));
        addParam(createParam<TrimbotWhite>(mm2px(Vec(x,y+14)),module,X4::MOD_CV_PARAM+k*4+j));
        x+=7;
      }
      y+=28;
    }
    x=10;y=116;
    for(int j=0;j<4;j++) {
      addOutput(createOutput<SmallPort>(mm2px(Vec(x,y)),module,j));
      x+=7;
    }
	}
};
#define X6 XX<6>
struct X6Widget : ModuleWidget {
  X6Widget(X6* module) {
    setModule(module);
    setPanel(createPanel(asset::plugin(pluginInstance, "res/X6.svg")));
    float x=2;
    float y=12;
    for(int j=0;j<6;j++) {
      addInput(createInput<SmallPort>(mm2px(Vec(x,y)),module,X6::M_INPUT+j));
      y+=17.5;
    }
    y=7;
    for(int k=0;k<6;k++) {
      x=10;
      for(int j=0;j<6;j++) {
        addParam(createParam<TrimbotWhite9>(mm2px(Vec(x+2,y)),module,X6::M_PARAM+k*6+j));
        addInput(createInput<SmallPort>(mm2px(Vec(x+7,y+9)),module,X6::MOD_INPUT+k*6+j));
        addParam(createParam<TrimbotWhite>(mm2px(Vec(x,y+9)),module,X6::MOD_CV_PARAM+k*6+j));
        x+=14;
      }
      y+=17.5;
    }
    x=13.75;y=116;
    for(int j=0;j<6;j++) {
      addOutput(createOutput<SmallPort>(mm2px(Vec(x,y)),module,j));
      x+=14;
    }
  }
};


Model* modelX4 = createModel<X4, X4Widget>("X4");
Model* modelX6 = createModel<X6, X6Widget>("X6");