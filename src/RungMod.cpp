#include "dcb.h"

template<size_t S>
struct SR {
  bool v[S]={};
  bool xr=false;

  void step(bool in) {
    for(int i=S-1;i>0;i--) {
      v[i]=v[i-1];
    }
    if(xr)
      v[0]=(in!=v[S-1]);
    else
      v[0]=in;
  }

  void loopStep() {
    for(int i=S-1;i>0;i--) {
      v[i]=v[i-1];
    }
    v[0]=v[S-1];
  }

  void reset() {
    for(unsigned int k=0;k<S;k++) {
      v[k]=false;
    }
  }
};

struct RungMod : Module {
  enum ParamId {
    TRUE1_PARAM,TRUE2_PARAM=TRUE1_PARAM+8,TRUE3_PARAM=TRUE2_PARAM+8,OFS_PARAM=TRUE3_PARAM+8,SCL_PARAM,CMP_PARAM,PARAMS_LEN
  };
  enum InputId {
    CLK_INPUT,RST_INPUT,DATA_INPUT,TRUE1_INPUT,TRUE2_INPUT,TRUE3_INPUT,INPUTS_LEN
  };
  enum OutputId {
    RUNGLER_OUTPUT,SR_OUTPUT,OUTPUTS_LEN
  };
  enum LightId {
    LIGHTS_LEN
  };

  bool orMode=false;
  std::string trueParamLabels[3][8]={{"SR6==0&SR3==0&SR0==0","SR6==0&SR3==0&SR0==1","SR6==0&SR3==1&SR0==0","SR6==0&SR3==1&SR0==1","SR6==1&SR3==0&SR0==0","SR6==1&SR3==0&SR0==1","SR6==1&SR3==1&SR0==0","SR6==1&SR3==1&SR0==1"},{"SR7==0&SR4==0&SR0==1","SR7==0&SR4==0&SR1==1","SR7==0&SR4==1&SR0==1","SR7==0&SR4==1&SR1==1","SR7==1&SR4==0&SR0==1","SR7==1&SR4==0&SR1==1","SR7==1&SR4==1&SR1==0","SR7==1&SR4==1&SR1==1"},
                                     {"SR8==0&SR5==0&SR0==2","SR8==0&SR5==0&SR2==1","SR8==0&SR5==1&SR0==2","SR8==0&SR5==1&SR2==1","SR8==1&SR5==0&SR0==2","SR8==1&SR5==0&SR2==1","SR8==1&SR5==1&SR2==0","SR8==1&SR5==1&SR2==1"}};
  SR<16> sr;
  dsp::SchmittTrigger clockTrigger;
  dsp::SchmittTrigger rstTrigger;
  dsp::ClockDivider divider;
  bool states[3][8]={};
  float out=0;
  RND rnd;

  void fillStates() {
    for(int k=0;k<3;k++) {
      for(int j=0;j<8;j++) {
        states[k][j]=params[TRUE1_PARAM+k*8+j].getValue()>0.5;
      }
    }
  }

  float getValue() {
    int q=0;
    if(sr.v[6])
      q+=4;
    if(sr.v[3])
      q+=2;
    if(sr.v[0])
      q+=1;
    bool a=states[0][q];
    q=0;
    if(sr.v[7])
      q+=4;
    if(sr.v[4])
      q+=2;
    if(sr.v[1])
      q+=1;
    bool b=states[1][q];
    q=0;
    if(sr.v[8])
      q+=4;
    if(sr.v[5])
      q+=2;
    if(sr.v[2])
      q+=1;
    bool c=states[2][q];
    float ret=(a?32.0f:0.0f);
    ret=ret+(b?64.0f:0.0f);
    ret=ret+(c?128.0f:0.0f);
    return (ret/255.0f);
  }

  RungMod() {
    config(PARAMS_LEN,INPUTS_LEN,OUTPUTS_LEN,LIGHTS_LEN);
    for(int k=0;k<3;k++) {
      for(int j=0;j<8;j++) {
        configButton(TRUE1_PARAM+k*8+j,trueParamLabels[k][j]);
      }
    }
    configParam(OFS_PARAM,-5,5,0,"Offset");
    configParam(SCL_PARAM,0,10,10,"Scale");
    configParam(CMP_PARAM,-5,5,0,"Compare");
    configInput(TRUE1_INPUT,"True 1");
    configInput(TRUE2_INPUT,"True 2");
    configInput(TRUE3_INPUT,"True 3");
    configInput(CLK_INPUT,"Clock");
    configInput(RST_INPUT,"Reset");
    configInput(DATA_INPUT,"Data");
    configOutput(SR_OUTPUT,"SR");
    configOutput(RUNGLER_OUTPUT,"Out");
    divider.setDivision(32);
  }

  void process(const ProcessArgs &args) override {
    if(divider.process()) {
      for(int k=0;k<3;k++) {
        if(inputs[TRUE1_INPUT+k].isConnected()) {
          for(int j=0;j<8;j++) {
            getParamQuantity(TRUE1_PARAM+k*8+j)->setValue(inputs[TRUE1_INPUT+k].getVoltage(j)>1.f?1.f:0.f);
          }
        }
      }
      fillStates();
    }
    if(clockTrigger.process(inputs[CLK_INPUT].getVoltage())) {
      sr.step(inputs[DATA_INPUT].getVoltage()>params[CMP_PARAM].getValue());
      out=getValue();
    }
    if(rstTrigger.process(inputs[RST_INPUT].getVoltage())) {
      sr.reset();
    }
    outputs[RUNGLER_OUTPUT].setVoltage(out*params[SCL_PARAM].getValue()+params[OFS_PARAM].getValue());
    for(int k=0;k<16;k++) {
      outputs[SR_OUTPUT].setVoltage(sr.v[k]?10.f:0.f,k);
    }
    outputs[SR_OUTPUT].setChannels(16);
  }

  void onRandomize(const RandomizeEvent &e) override {
    for(int k=0;k<3;k++) {
      for(int j=0;j<8;j++) {
        getParamQuantity(TRUE1_PARAM+k*8+j)->setValue(rnd.nextCoin()?1.f:0.f);
      }
    }
  }

};


struct RungModWidget : ModuleWidget {
  RungModWidget(RungMod *module) {
    setModule(module);
    setPanel(createPanel(asset::plugin(pluginInstance,"res/RungMod.svg")));
    float x0=3;
    float y0=10;
    for(int k=0;k<3;k++) {
      for(int j=0;j<8;j++) {
        addParam(createParam<MLED>(mm2px(Vec(x0+k*9,y0+j*8)),module,RungMod::TRUE1_PARAM+k*8+j));
      }
    }
    addParam(createParam<TrimbotWhite>(mm2px(Vec(12,104)),module,RungMod::CMP_PARAM));
    addParam(createParam<TrimbotWhite>(mm2px(Vec(21,104)),module,RungMod::OFS_PARAM));
    addParam(createParam<TrimbotWhite>(mm2px(Vec(21,92)),module,RungMod::SCL_PARAM));
    addInput(createInput<SmallPort>(mm2px(Vec(3,74)),module,RungMod::TRUE1_INPUT));
    addInput(createInput<SmallPort>(mm2px(Vec(12,74)),module,RungMod::TRUE2_INPUT));
    addInput(createInput<SmallPort>(mm2px(Vec(21,74)),module,RungMod::TRUE3_INPUT));
    addInput(createInput<SmallPort>(mm2px(Vec(3,92)),module,RungMod::CLK_INPUT));
    addInput(createInput<SmallPort>(mm2px(Vec(12,92)),module,RungMod::RST_INPUT));
    addInput(createInput<SmallPort>(mm2px(Vec(3,104)),module,RungMod::DATA_INPUT));
    addOutput(createOutput<SmallPort>(mm2px(Vec(3,116)),module,RungMod::SR_OUTPUT));
    addOutput(createOutput<SmallPort>(mm2px(Vec(21,116)),module,RungMod::RUNGLER_OUTPUT));
  }
};


Model *modelRungMod=createModel<RungMod,RungModWidget>("RungMod");