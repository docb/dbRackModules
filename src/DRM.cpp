#include "dcb.h"

#include <limits>

#define P(x) params[ (x) ].getValue()
using simd::float_4;

// Algorithm taken from https://github.com/eres-j/VCVRack-plugin-JE

template<typename T>
struct Diode {
  T process(T v,T vb,T vlMinusVb,T h) {
    T vl=vlMinusVb+vb;
    T vlVbDenom=2.f*(vlMinusVb);
    T vlAdd=h*vlMinusVb*vlMinusVb/vlVbDenom;
    T hVl=h*vl;
    T av=simd::fabs(v);
    return simd::ifelse(av<=vb,0.f,simd::ifelse(av<=vl,h*(av-vb)*(av-vb)/vlVbDenom,h*av-hVl+vlAdd));
  }

};

struct DRM : Module {
  enum ParamId {
    IN_CV_PARAM,IN_POLAR_PARAM,CAR_CV_PARAM,CAR_POLAR_PARAM,OFFSET_PARAM,OFFSET_CV_PARAM,VB_PARAM,VB_CV_PARAM,VL_VB_PARAM,VL_VB_CV_PARAM,GAIN_PARAM,GAIN_CV_PARAM,PARAMS_LEN
  };
  enum InputId {
    IN_INPUT,CAR_INPUT,OFFSET_INPUT,VB_INPUT,VL_VB_INPUT,GAIN_INPUT,INPUTS_LEN
  };
  enum OutputId {
    RING_OUTPUT,SUM_OUTPUT,DIFF_OUTPUT,MIN_OUTPUT,MAX_OUTPUT,OUTPUTS_LEN
  };
  enum LightId {
    LIGHTS_LEN
  };

  Diode<float_4> diode;
  DCBlocker<float_4> dcbSum[4];
  DCBlocker<float_4> dcbDiff[4];
  DCBlocker<float_4> dcbMin[4];
  DCBlocker<float_4> dcbMax[4];
  DCBlocker<float_4> dcbRing[4];
  bool blockDC=true;

  DRM() {
    config(PARAMS_LEN,INPUTS_LEN,OUTPUTS_LEN,LIGHTS_LEN);
    configSwitch(IN_POLAR_PARAM,0,2,1,"In Polarity",{"+","0","-"});
    configSwitch(CAR_POLAR_PARAM,0,2,1,"Carrier Polarity",{"+","0","-"});
    configParam(IN_CV_PARAM,0.f,1.f,1.f,"Input level");
    configParam(CAR_CV_PARAM,0.f,1.f,1.f,"Carrier level");
    configParam(OFFSET_PARAM,-5,5,0.f,"Carrier offset");
    configParam(VB_PARAM,std::numeric_limits<float>::epsilon(),5,0.2f,"Diode forward-bias voltage (Vb)");
    configParam(VL_VB_PARAM,std::numeric_limits<float>::epsilon(),5,0.5f,"Diode voltage beyond which the function is linear - Vb");
    configParam(GAIN_PARAM,0.f,1.f,0.9f,"Diode slope of the linear section");
    configParam(OFFSET_CV_PARAM,0,1,0,"Offset CV");
    configParam(VB_CV_PARAM,0,1,0,"VB CV");
    configParam(VL_VB_CV_PARAM,0,1,0,"VL-VB CV");
    configParam(GAIN_CV_PARAM,0,1,0,"Gain CV");
    configInput(IN_INPUT,"In");
    configInput(CAR_INPUT,"Carrier");
    configInput(OFFSET_INPUT,"Offset");
    configInput(VB_INPUT,"VB");
    configInput(VL_VB_INPUT,"VL-VB");
    configInput(GAIN_INPUT,"Gain");
    configOutput(SUM_OUTPUT,"Sum");
    configOutput(DIFF_OUTPUT,"Diff");
    configOutput(MIN_OUTPUT,"Min");
    configOutput(MAX_OUTPUT,"Max");
    configOutput(RING_OUTPUT,"Ring");
  }

  void process(const ProcessArgs &args) override {
    int channels=std::max(inputs[IN_INPUT].getChannels(),inputs[CAR_INPUT].getChannels());
    for(int c=0;c<channels;c+=4) {
      float_4 VB=simd::clamp(P(VB_PARAM)+inputs[VB_INPUT].getPolyVoltageSimd<float_4>(c)*P(VB_CV_PARAM),std::numeric_limits<float>::epsilon(),5.f);
      float_4 VL_VB=simd::clamp(P(VL_VB_PARAM)+inputs[VL_VB_INPUT].getPolyVoltageSimd<float_4>(c)*P(VL_VB_CV_PARAM),std::numeric_limits<float>::epsilon(),5.f);
      float_4 G=simd::clamp(P(GAIN_PARAM)+inputs[GAIN_INPUT].getPolyVoltageSimd<float_4>(c)*P(GAIN_CV_PARAM),0.f,1.f);

      float_4 inValue=inputs[IN_INPUT].getPolyVoltageSimd<float_4>(c)*P(IN_CV_PARAM);
      int mode=P(IN_POLAR_PARAM);
      switch(mode) {
        case 0:
          inValue=simd::ifelse(inValue<0.0f,-diode.process(inValue,VB,VL_VB,G),0.f);
          break;
        case 2:
          inValue=simd::ifelse(inValue>0.0f,diode.process(inValue,VB,VL_VB,G),0.f);
          break;
        default:
          break;
      }

      float_4 carValue=inputs[CAR_INPUT].getPolyVoltageSimd<float_4>(c)*P(CAR_CV_PARAM);
      mode=P(CAR_POLAR_PARAM);
      switch(mode) {
        case 0:
          carValue=simd::ifelse(carValue<0.0f,-diode.process(carValue,VB,VL_VB,G),0.f);
          break;
        case 2:
          carValue=simd::ifelse(carValue>0.0f,diode.process(carValue,VB,VL_VB,G),0.f);
          break;
        default:
          break;
      }
      carValue=simd::clamp(carValue+(P(OFFSET_PARAM)+inputs[OFFSET_INPUT].getPolyVoltageSimd<float_4>(c)*P(OFFSET_CV_PARAM)),-5.f,5.f);

      float_4 ci=carValue+inValue;
      float_4 cmi=carValue-inValue;

      float_4 sum=diode.process(ci,VB,VL_VB,G);
      if(outputs[SUM_OUTPUT].isConnected()) {
        outputs[SUM_OUTPUT].setVoltageSimd(blockDC?dcbSum[c/4].process(sum):sum,c);
      }
      float_4 diff=diode.process(cmi,VB,VL_VB,G);
      if(outputs[DIFF_OUTPUT].isConnected())
        outputs[DIFF_OUTPUT].setVoltageSimd(blockDC?dcbDiff[c/4].process(diff):diff,c);

      if(outputs[MIN_OUTPUT].isConnected())
        outputs[MIN_OUTPUT].setVoltageSimd(blockDC?dcbMin[c/4].process(simd::ifelse(sum<diff,sum,diff)):simd::ifelse(sum<diff,sum,diff),c);
      if(outputs[MAX_OUTPUT].isConnected())
        outputs[MAX_OUTPUT].setVoltageSimd(blockDC?dcbMax[c/4].process(simd::ifelse(sum>diff,sum,diff)):simd::ifelse(sum>diff,sum,diff),c);

      float_4 out=sum-diff;
      outputs[RING_OUTPUT].setVoltageSimd(blockDC?dcbRing[c/4].process(out):out,c);

    }
    outputs[SUM_OUTPUT].setChannels(channels);
    outputs[DIFF_OUTPUT].setChannels(channels);
    outputs[MIN_OUTPUT].setChannels(channels);
    outputs[MAX_OUTPUT].setChannels(channels);
    outputs[RING_OUTPUT].setChannels(channels);
  }

  json_t *dataToJson() override {
    json_t *data=json_object();
    json_object_set_new(data,"dc",json_boolean(blockDC));
    return data;
  }

  void dataFromJson(json_t *rootJ) override {
    json_t *jDcBlock=json_object_get(rootJ,"dc");
    if(jDcBlock!=nullptr) blockDC=json_boolean_value(jDcBlock);
  }
};


struct DRMWidget : ModuleWidget {
  DRMWidget(DRM *module) {
    setModule(module);
    setPanel(createPanel(asset::plugin(pluginInstance,"res/DRM.svg")));
    float x=1.9;
    float x2=12.9;
    float y=8;
    addInput(createInput<SmallPort>(mm2px(Vec(x,y)),module,DRM::IN_INPUT));
    auto inPolarParam=createParam<SelectParam>(mm2px(Vec(9,y)),module,DRM::IN_POLAR_PARAM);
    inPolarParam->box.size=Vec(8,22);
    inPolarParam->init({"+","0","-"});
    addParam(inPolarParam);
    addParam(createParam<TrimbotWhite>(mm2px(Vec(x2,y)),module,DRM::IN_CV_PARAM));
    y=19;
    addInput(createInput<SmallPort>(mm2px(Vec(x,y)),module,DRM::CAR_INPUT));
    auto carPolarParam=createParam<SelectParam>(mm2px(Vec(9,y)),module,DRM::CAR_POLAR_PARAM);
    carPolarParam->box.size=Vec(8,22);
    carPolarParam->init({"+","0","-"});
    addParam(carPolarParam);
    addParam(createParam<TrimbotWhite>(mm2px(Vec(x2,y)),module,DRM::CAR_CV_PARAM));

    y=33;
    float y2=31;
    addParam(createParam<TrimbotWhite9>(mm2px(Vec(x,y)),module,DRM::OFFSET_PARAM));
    addParam(createParam<TrimbotWhite>(mm2px(Vec(x2,y2)),module,DRM::OFFSET_CV_PARAM));
    addInput(createInput<SmallPort>(mm2px(Vec(x2,y2+7)),module,DRM::OFFSET_INPUT));
    y+=17;
    y2+=17;
    addParam(createParam<TrimbotWhite9>(mm2px(Vec(x,y)),module,DRM::VB_PARAM));
    addParam(createParam<TrimbotWhite>(mm2px(Vec(x2,y2)),module,DRM::VB_CV_PARAM));
    addInput(createInput<SmallPort>(mm2px(Vec(x2,y2+7)),module,DRM::VB_INPUT));
    y+=17;
    y2+=17;
    addParam(createParam<TrimbotWhite9>(mm2px(Vec(x,y)),module,DRM::VL_VB_PARAM));
    addParam(createParam<TrimbotWhite>(mm2px(Vec(x2,y2)),module,DRM::VL_VB_CV_PARAM));
    addInput(createInput<SmallPort>(mm2px(Vec(x2,y2+7)),module,DRM::VL_VB_INPUT));
    y+=17;
    y2+=17;
    addParam(createParam<TrimbotWhite9>(mm2px(Vec(x,y)),module,DRM::GAIN_PARAM));
    addParam(createParam<TrimbotWhite>(mm2px(Vec(x2,y2)),module,DRM::GAIN_CV_PARAM));
    addInput(createInput<SmallPort>(mm2px(Vec(x2,y2+7)),module,DRM::GAIN_INPUT));


    addOutput(createOutput<SmallPort>(mm2px(Vec(7,108.5)),module,DRM::RING_OUTPUT));
    y=99;
    addOutput(createOutput<SmallPort>(mm2px(Vec(x,y)),module,DRM::SUM_OUTPUT));
    addOutput(createOutput<SmallPort>(mm2px(Vec(x2,y)),module,DRM::DIFF_OUTPUT));
    y=116;
    addOutput(createOutput<SmallPort>(mm2px(Vec(x,y)),module,DRM::MIN_OUTPUT));
    addOutput(createOutput<SmallPort>(mm2px(Vec(x2,y)),module,DRM::MAX_OUTPUT));


  }

  void appendContextMenu(Menu *menu) override {
    DRM *module=dynamic_cast<DRM *>(this->module);
    assert(module);
    menu->addChild(new MenuSeparator);
    menu->addChild(createBoolPtrMenuItem("Block DC","",&module->blockDC));
  }
};


Model *modelDRM=createModel<DRM,DRMWidget>("DRM");