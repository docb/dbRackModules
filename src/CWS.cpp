#include "dcb.h"

using simd::float_4;

struct CWS : Module {
  enum ParamId {
    IN_LEVEL_PARAM,IN_LEVEL_CV_PARAM,PARAMS_LEN
  };
  enum InputId {
    INPUT,COEFF_INPUT,LVL_INPUT,INPUTS_LEN
  };
  enum OutputId {
    OUTPUT,OUTPUTS_LEN
  };
  enum LightId {
    LIGHTS_LEN
  };
  DCBlocker<float_4> dcb[4];
  float defaultCoefficients[16]={10,-5,5,2.5,-2.5,1,-1,0,0,0,0,0,0,0,0,0};
  CWS() {
    config(PARAMS_LEN,INPUTS_LEN,OUTPUTS_LEN,LIGHTS_LEN);
    configParam(IN_LEVEL_PARAM,0,1,0.1,"Lvl");
    configParam(IN_LEVEL_CV_PARAM,0,1,0,"Lvl CV");
    configInput(INPUT,"IN");
    configInput(LVL_INPUT,"Input Level");
    configInput(COEFF_INPUT,"Coefficients");
    configOutput(OUTPUT,"OUT");
    configBypass(INPUT,OUTPUT);
  }

  void process(const ProcessArgs &args) override {
    int nc=0;
    float *coefficients=nullptr;
    if(inputs[COEFF_INPUT].isConnected()) {
      nc=inputs[COEFF_INPUT].getChannels();
      coefficients=inputs[COEFF_INPUT].getVoltages();
    } else {
      coefficients=&defaultCoefficients[0];
      nc=8;
    }
    float chebyN[17]={};
    float newCoefficients[17]={};
    chebyN[0]=chebyN[1]=1.f;

    for(int i=2;i<=nc;++i)
      chebyN[i]=0.f;
    newCoefficients[0]=0;
    if(nc>0)
      newCoefficients[1]=coefficients[0]*0.1f;
    if(nc>1) {
      for(int i=2;i<=nc;++i)
        newCoefficients[i]=0.f;
      for(int i=2;i<=nc;i+=2) {
        chebyN[0]=-chebyN[0];
        newCoefficients[0]+=chebyN[0]*coefficients[i-1]*0.1f;
        for(int j=2;j<=nc;j+=2) {
          chebyN[j]=2*chebyN[j-1]-chebyN[j];
          newCoefficients[j]+=chebyN[j]*coefficients[i-1]*0.1f;
        }
        if(nc>i) {
          for(int j=1;j<=nc;j+=2) {
            chebyN[j]=2*chebyN[j-1]-chebyN[j];
            newCoefficients[j]+=chebyN[j]*coefficients[i]*0.1f;
          }
        }
      }
    }
    int channels=inputs[INPUT].getChannels();
    for(int c=0;c<channels;c+=4) {
      float_4 inputLevel=simd::clamp(params[IN_LEVEL_PARAM].getValue()+inputs[LVL_INPUT].getPolyVoltageSimd<float_4>(c)*params[IN_LEVEL_CV_PARAM].getValue()*0.1f,0.f,1.f);
      float_4 in=inputs[INPUT].getVoltageSimd<float_4>(c)*inputLevel;
      in = simd::ifelse(in>1.5f,1.f,simd::ifelse(in<-1.5f,-1.f,in-(4.f/27.f)*in*in*in));// soft clip input
      float_4 out=newCoefficients[nc];
      for(int i=nc-1;i>=0;--i) {
        out*=in;
        out+=newCoefficients[i];
      }
      outputs[OUTPUT].setVoltageSimd(dcb[c/4].process(out*5),c);
    }
    outputs[OUTPUT].setChannels(channels);
  }
};


struct CWSWidget : ModuleWidget {
  CWSWidget(CWS *module) {
    setModule(module);
    setPanel(createPanel(asset::plugin(pluginInstance,"res/CWS.svg")));
    float x=1.9;
    addInput(createInput<SmallPort>(mm2px(Vec(x,60)),module,CWS::COEFF_INPUT));
    addParam(createParam<TrimbotWhite>(mm2px(Vec(x,76)),module,CWS::IN_LEVEL_PARAM));
    addInput(createInput<SmallPort>(mm2px(Vec(x,84)),module,CWS::LVL_INPUT));
    addParam(createParam<TrimbotWhite>(mm2px(Vec(x,92)),module,CWS::IN_LEVEL_CV_PARAM));
    addInput(createInput<SmallPort>(mm2px(Vec(x,104)),module,CWS::INPUT));
    addOutput(createOutput<SmallPort>(mm2px(Vec(x,116)),module,CWS::OUTPUT));
  }
};


Model *modelCWS=createModel<CWS,CWSWidget>("CWS");