#include "dcb.h"

static float scaleOutput(float _in)
{
  float out = _in+M_PI;
  out = out / (2*TWOPIF);
  return out;
}

struct SMOsc {
  float phs=0.f;
  float xOld=0.5f;
  float xNew=0.5f;
  float yOld=0.5f;
  float yNew=0.5f;
  float outX=0;
  float outY=0;
  void process(float delta, float _r) {
    float r = _r*8.f;
    phs+=delta;
    if(phs>=1.f) {
      xNew = std::fmod((xOld + (r * std::sin(yOld))), TWOPIF);
      yNew = std::fmod((yOld + xNew), TWOPIF);
      xOld = xNew;
      yOld = yNew;
      phs-=std::floor(phs);
    }
    outX=xOld+(xNew-xOld)*phs;
    outY=yOld+(yNew-yOld)*phs;
  }
  void reset(float x=0.5f,float y=0.5f) {
    xOld=x;
    yOld=y;
    phs=0;
  }
};
struct SMSqrOsc {
  float xOld=0.5f;
  float xNew=0.5f;
  float yOld=0.5f;
  float yNew=0.5f;
  void process(float _r) {
    float r = _r*8.f;
    xNew = std::fmod((xOld + (r * std::sin(yOld))), TWOPIF);
    yNew = std::fmod((yOld + xNew), TWOPIF);
    xOld = xNew;
    yOld = yNew;
  }
  void reset(float x=0.5f,float y=0.5f) {
    xOld=x;
    yOld=y;
  }
};

struct Map2 : Module {
  enum ParamId {
    FREQ_PARAM,P1_PARAM,P1_CV_PARAM,P2_PARAM,P2_CV_PARAM,X0_PARAM,Y0_PARAM,MODE_PARAM,PARAMS_LEN
  };
  enum InputId {
    VOCT_INPUT,P1_INPUT,P2_INPUT,CLK_INPUT,RST_INPUT,X0_INPUT,Y0_INPUT,INPUTS_LEN
  };
  enum OutputId {
    X_OUTPUT,Y_OUTPUT,OUTPUTS_LEN
  };
  enum LightId {
    LIGHTS_LEN
  };
  enum Mode {
    STD, MODE_LEN
  };
  SMOsc smOsc[16];
  SMSqrOsc smOscSq[16];
  dsp::SchmittTrigger clockTrigger[16];
  dsp::SchmittTrigger rstTrigger;
  dsp::PulseGenerator rstPulse;
  std::vector<std::string> labels = {"Standard Map"};
  float outX[16]={};
  float outY[16]={};
  float x0[16]={};
  float y0[16]={};

  Map2() {
		config(PARAMS_LEN, INPUTS_LEN, OUTPUTS_LEN, LIGHTS_LEN);
    configParam(FREQ_PARAM,-10.f,6.f,0.f,"Frequency"," Hz",2,dsp::FREQ_C4);
    configSwitch(MODE_PARAM,0.f,MODE_PARAM-1,0,"Mode",labels);
    configParam(P1_PARAM,0.f,1.f,0.5f,"P1");
    configParam(P1_CV_PARAM,0,1,0,"R CV","%",0,100);
    configParam(P2_PARAM,0.f,1.f,0.5f,"P2");
    configParam(P2_CV_PARAM,0,1,0,"R CV","%",0,100);
    configParam(X0_PARAM,0.f,1.f,0.5f,"X0");
    configParam(Y0_PARAM,0.f,1.f,0.5f,"X0");
    configInput(VOCT_INPUT,"V/Oct");
    configInput(P1_INPUT,"P1");
    configInput(P2_INPUT,"P2");
    configInput(Y0_INPUT,"Y0");
    configInput(X0_INPUT,"X0");
    configInput(CLK_INPUT,"Clock");
    configInput(RST_INPUT,"Reset");

    configOutput(X_OUTPUT,"X");
    configOutput(Y_OUTPUT,"X");
    for(int k=0;k<16;k++) {
      x0[k]=0.5f;
      y0[k]=0.5f;
    }
	}

	void process(const ProcessArgs& args) override {
    Mode mode = static_cast<Mode>((int)params[MODE_PARAM].getValue());
    float x0param=params[X0_PARAM].getValue();
    float y0param=params[Y0_PARAM].getValue();
    float freqParam=params[FREQ_PARAM].getValue();
    float p1Param=params[P1_PARAM].getValue();
    float p1CV=params[P1_CV_PARAM].getValue();
    bool clk=inputs[CLK_INPUT].isConnected();
    if(rstTrigger.process(inputs[RST_INPUT].getVoltage())) {
      for(int k=0;k<16;k++) {
        x0[k]=clamp(x0param+inputs[X0_INPUT].getVoltage(k),0.f,1.f);
        y0[k]=clamp(y0param+inputs[Y0_INPUT].getVoltage(k),0.f,1.f);
        reset();
      }
      rstPulse.trigger();
    }
    bool rstGate=rstPulse.process(args.sampleTime);
    int channels=clk?std::max(inputs[CLK_INPUT].getChannels(),1):std::max(inputs[VOCT_INPUT].getChannels(),1);
    for(int c=0;c<channels;c++) {
      float p1=p1Param+inputs[P1_INPUT].getPolyVoltage(c)*0.05f*p1CV;
      p1=simd::clamp(p1,0.f,1.f);
      if(clk) {
        if(clockTrigger[c].process(inputs[CLK_INPUT].getVoltage(c))&&!rstGate) {
          switch(mode) {
            default:
              smOscSq[c].process(p1);
              outX[c]=scaleOutput(smOscSq[c].xNew);
              outY[c]=scaleOutput(smOscSq[c].yNew);
          }
        }
      } else {
        float pitch=freqParam+inputs[VOCT_INPUT].getVoltage(c);
        float freq=dsp::FREQ_C4*std::pow(2.f,pitch)*2;
        float delta=freq*args.sampleTime;
        switch(mode) {
          default:
            smOsc[c].process(delta,p1);
            outX[c]=scaleOutput(smOsc[c].outX);
            outY[c]=scaleOutput(smOsc[c].outY);
        }

      }
      outputs[X_OUTPUT].setVoltage(outX[c]* 10.f - 5.f,c);
      outputs[Y_OUTPUT].setVoltage(outY[c]* 10.f - 5.f,c);
    }
    outputs[X_OUTPUT].setChannels(channels);
    outputs[Y_OUTPUT].setChannels(channels);
	}

  void reset() {
    for(int k=0;k<16;k++) {
      smOsc[k].reset(x0[k],y0[k]);
      smOscSq[k].reset(x0[k],y0[k]);
    }
  }

  void onReset(const ResetEvent &e) override {
    reset();
  }
};


struct Map2Widget : ModuleWidget {
	Map2Widget(Map2* module) {
		setModule(module);
		setPanel(createPanel(asset::plugin(pluginInstance, "res/Map2.svg")));
    float x0=4.f;
    float x1=16.f;
    addParam(createParam<TrimbotWhite>(mm2px(Vec(9,10)),module,Map2::FREQ_PARAM));
    addInput(createInput<SmallPort>(mm2px(Vec(9,22)),module,Map2::VOCT_INPUT));
    addInput(createInput<SmallPort>(mm2px(Vec(x0,34)),module,Map2::CLK_INPUT));
    addInput(createInput<SmallPort>(mm2px(Vec(x1,34)),module,Map2::RST_INPUT));
    //auto modeParam=createParam<SelectParam>(mm2px(Vec(1.2,52.5)),module,Map2::MODE_PARAM);
    //modeParam->box.size=Vec(22,44);
    //modeParam->init({"Log","Tent","Circ","Saw"});
    //addParam(modeParam);
    addParam(createParam<TrimbotWhite>(mm2px(Vec(x0,72)),module,Map2::X0_PARAM));
    addParam(createParam<TrimbotWhite>(mm2px(Vec(x1,72)),module,Map2::Y0_PARAM));
    addInput(createInput<SmallPort>(mm2px(Vec(x0,79)),module,Map2::X0_INPUT));
    addInput(createInput<SmallPort>(mm2px(Vec(x1,79)),module,Map2::Y0_INPUT));
    addParam(createParam<TrimbotWhite>(mm2px(Vec(x0,90)),module,Map2::P1_PARAM));
    addInput(createInput<SmallPort>(mm2px(Vec(x0,97)),module,Map2::P1_INPUT));
    addParam(createParam<TrimbotWhite>(mm2px(Vec(x0,104)),module,Map2::P1_CV_PARAM));

    addOutput(createOutput<SmallPort>(mm2px(Vec(x0,116)),module,Map2::X_OUTPUT));
    addOutput(createOutput<SmallPort>(mm2px(Vec(x1,116)),module,Map2::Y_OUTPUT));
	}
};


Model* modelMap2 = createModel<Map2, Map2Widget>("Map2");