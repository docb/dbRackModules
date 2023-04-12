#include "dcb.h"

using simd::float_4;

struct PhO : Module {
  enum ParamId {
    DMP_PARAM,PHS_PARAM,PARAMS_LEN
  };
  enum InputId {
    PHASE_INPUT,AMP_1_16_INPUT,DMP_INPUT,INPUTS_LEN
  };
  enum OutputId {
    V_OUTPUT,OUTPUTS_LEN
  };
  enum LightId {
    LIGHTS_LEN
  };

  bool blockDC=false;
  DCBlocker<float_4> dcBlocker[4];

  double fastPow(double a,double b) {
    union {
      double d;
      int x[2];
    }u={a};
    u.x[1]=(int)(b*(u.x[1]-1072632447)+1072632447);
    u.x[0]=0;
    return u.d;
  }

  void onReset(const ResetEvent &e) override {
    for(auto dcb:dcBlocker)
      dcb.reset();
  }

  PhO() {
    config(PARAMS_LEN,INPUTS_LEN,OUTPUTS_LEN,LIGHTS_LEN);
    configParam(DMP_PARAM,0.f,10.f,0.f,"Damp");
    configParam(PHS_PARAM,0,1,0,"Phase Offset");
    configInput(AMP_1_16_INPUT,"AMP 1-16");
    configInput(PHASE_INPUT,"Phase");
    configInput(DMP_INPUT,"Damp");
    configOutput(V_OUTPUT,"Wave");
  }

  void process(const ProcessArgs &args) override {
    int channels=inputs[PHASE_INPUT].getChannels();
    float amps[16]={1.f,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0};
    float dmp=10.f-params[DMP_PARAM].getValue();
    if(inputs[DMP_INPUT].isConnected()) {
      dmp=10.f-clamp(inputs[DMP_INPUT].getVoltage(),0.f,10.f);
      setImmediateValue(getParamQuantity(DMP_PARAM),10.f-dmp);
    }
    if(inputs[AMP_1_16_INPUT].isConnected()) {
      for(int k=0;k<16;k++) {
        amps[k]=(inputs[AMP_1_16_INPUT].getVoltage(k)/10.f);
        if(amps[k]>0)
          amps[k]*=fastPow(M_E,-dmp*((float)k/32.f));
      }
    }
    for(int c=0;c<channels;c+=4) {
      float_4 phs=inputs[PHASE_INPUT].getVoltageSimd<float_4>(c)*(float(M_PI)/5.f)+TWOPIF*params[PHS_PARAM].getValue();
      float_4 out=0;
      for(int k=0;k<16;k++) {
        if(amps[k]>0) {
          out+=(amps[k]*simd::sin(phs*float(k+1)));
        }
      }
      if(blockDC) {
        out=dcBlocker[c/4].process(out);
      }
      outputs[V_OUTPUT].setVoltageSimd(out*5.f,c);
    }
    outputs[V_OUTPUT].setChannels(channels);

  }
  json_t* dataToJson() override {
    json_t *root=json_object();
    json_object_set_new(root,"blockDC",json_boolean(blockDC));
    return root;
  }

  void dataFromJson(json_t *root) override {
    json_t *jBlockDC=json_object_get(root,"blockDC");
    if(jBlockDC) {
      blockDC=json_boolean_value(jBlockDC);
    }
  }
};


struct PhOWidget : ModuleWidget {
  PhOWidget(PhO *module) {
    setModule(module);
    setPanel(createPanel(asset::plugin(pluginInstance,"res/PhO.svg")));

    float x=1.9;
    addInput(createInput<SmallPort>(mm2px(Vec(x,9)),module,PhO::AMP_1_16_INPUT));
    addParam(createParam<TrimbotWhite>(mm2px(Vec(x,21)),module,PhO::DMP_PARAM));
    addInput(createInput<SmallPort>(mm2px(Vec(x,31)),module,PhO::DMP_INPUT));
    addInput(createInput<SmallPort>(mm2px(Vec(x,MHEIGHT-67)),module,PhO::PHASE_INPUT));
    addParam(createParam<TrimbotWhite>(mm2px(Vec(x,MHEIGHT-55)),module,PhO::PHS_PARAM));
    addOutput(createOutput<SmallPort>(mm2px(Vec(x,116)),module,PhO::V_OUTPUT));
  }

  void appendContextMenu(Menu *menu) override {
    PhO *module=dynamic_cast<PhO *>(this->module);
    assert(module);
    menu->addChild(new MenuSeparator);
    menu->addChild(createCheckMenuItem("block dc","",[=]() {
      return module->blockDC;
    },[=]() {
      module->blockDC=!module->blockDC;
    }));
  }
};


Model *modelPhO=createModel<PhO,PhOWidget>("PhO");