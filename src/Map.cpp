#include "dcb.h"
struct SawMapOsc {
  float phs=0.f;
  float xOld=0.5f;
  float xNew=0.5f;
  float process(float delta, float _r) {
    float r = rescale(_r,0.f,1.f,1.00001f,3.99999f);
    if(r==2.f) r=2.00001f;
    phs+=delta;
    if(phs>=1.f) {
      xNew = fmodf(r*xOld,1.f);
      if(xNew==0.f) xNew=0.00001f;
      xOld = xNew;
      phs-=std::floor(phs);
    }
    float out=xOld+(xNew-xOld)*phs;
    return out;
  }
  void reset(float _x=0.5f) {
    xOld=_x;
    phs=0;
  }
};
struct SawMapSqrOsc {
  float xOld=0.5f;
  float xNew=0.5f;
  float process(float _r) {
    float r = rescale(_r,0.f,1.f,1.00001f,3.99999f);
    if(r==2.f) r=2.00001f;
    xNew = fmodf(r*xOld,1.f);
    if(xNew==0.f) xNew=0.00001f;
    xOld = xNew;
    return xNew;
  }
  void reset(float _x=0.5f) {
    xOld=_x;
  }
};
struct CircOsc {
  float phs=0.f;
  float xOld=0.5f;
  float xNew=0.5f;
  float omega = 1/3.f;
  float process(float delta, float _r) {
    float r = _r*2.f;
    phs+=delta;
    if(phs>=1.f) {
      xNew = xOld + omega + r*(std::sin(xOld*TWOPIF));
      xNew = fmodf(xNew,1.f);
      xOld = xNew;
      phs-=std::floor(phs);
    }
    float out=xOld+(xNew-xOld)*phs;
    return (out+1)*0.5f;
  }
  void reset(float _x=0.5f) {
    xOld=_x;
    phs=0;
  }
};
struct CircSqrOsc {
  float xOld=0.5f;
  float xNew=0.5f;
  float omega = 1/3.f;
  float process(float _r) {
    float r = _r*2.f;
    xNew = xOld + omega + r*(std::sin(xOld*TWOPIF));
    xNew = fmodf(xNew,1.f);
    xOld = xNew;
    return (xNew+1)*0.5f;
  }
  void reset(float _x=0.5f) {
    xOld=_x;
  }
};
struct TentOsc {
  float phs=0.f;
  float xOld=0.5f;
  float xNew=0.5f;
  float process(float delta, float _r) {
    float r = _r+1.f;
    if(r>=2.f) r=1.9999f;
    phs+=delta;
    if(phs>=1.f) {
      xOld=xNew;
      xNew = r * (xOld < 0.5f ? xOld : 1.f - xOld);
      phs-=std::floor(phs);
    }
    return xOld+(xNew-xOld)*phs;
  }
  void reset(float _x=0.5f) {
    xNew=_x;
    phs=0;
  }
};
struct TentSqrOsc {
  float xOld=0.5f;
  float xNew=0.5f;
  float process(float _r) {
    float r = _r+1.f;
    if(r>=2.f) r=1.9999f;
    xOld=xNew;
    xNew = r * (xOld < 0.5f ? xOld : 1.f - xOld);
    return xNew;
  }
  void reset(float _x=0.5f) {
    xNew=_x;
  }
};
struct LMOsc {
  float phs=0.f;
  float xOld=0.5f;
  float xNew=0.5f;
  float process(float delta, float _r) {
    float r = _r+3.f;
    phs+=delta;
    if(phs>=1.f) {
      xOld=xNew;
      xNew=r * xOld * (1.f - xOld);
      phs-=std::floor(phs);
    }
    return xOld+(xNew-xOld)*phs;
  }
  void reset(float _x=0.5f) {
    xNew=_x;
    phs=0;
  }
};
struct LMSqrOsc {
  float xOld=0.5f;
  float xNew=0.5f;
  float process(float _r) {
    float r = _r+3.f;
    xOld=xNew;
    xNew=r * xOld * (1.f - xOld);
    return xNew;
  }
  void reset(float _x=0.5f) {
    xNew=_x;
  }
};
struct Map : Module {
	enum ParamId {
		FREQ_PARAM,R_PARAM,R_CV_PARAM,X_PARAM,MODE_PARAM,PARAMS_LEN
	};
	enum InputId {
		VOCT_INPUT,R_INPUT,CLK_INPUT,RST_INPUT,X0_INPUT,INPUTS_LEN
	};
	enum OutputId {
		OUTPUT,OUTPUTS_LEN
	};
	enum LightId {
		LIGHTS_LEN
	};
  enum Mode {
    LOG, TENT, CIRC, SAW, MODE_LEN
  };
  LMOsc osc[16];
  LMSqrOsc oscSq[16];
  TentOsc tentOsc[16];
  TentSqrOsc tentOscSq[16];
  CircOsc circOsc[16];
  CircSqrOsc circOscSq[16];
  SawMapOsc sawMapOsc[16];
  SawMapSqrOsc sawMapOscSq[16];
  dsp::SchmittTrigger clockTrigger[16];
  dsp::SchmittTrigger rstTrigger;
  dsp::PulseGenerator rstPulse;
  bool update=false;
  std::vector<std::string> labels = {"Logistic Map","Tent Map", "Circle Map","Saw Tooth Map"};
  float out[16];
  float x0[16];
	Map() {
		config(PARAMS_LEN, INPUTS_LEN, OUTPUTS_LEN, LIGHTS_LEN);
    configParam(FREQ_PARAM,-10.f,6.f,0.f,"Frequency"," Hz",2,dsp::FREQ_C4);
    configSwitch(MODE_PARAM,0.f,MODE_PARAM-1,0,"Mode",labels);
    configParam(R_PARAM,0.f,1.f,0.5f,"R");
    configParam(X_PARAM,0.1f,0.9f,0.5f,"X0");
    configParam(R_CV_PARAM,0,1,0,"R CV","%",0,100);
    configInput(VOCT_INPUT,"V/Oct");
    configInput(R_INPUT,"R");
    configInput(X0_INPUT,"X0");
    configInput(CLK_INPUT,"Clock");
    configInput(RST_INPUT,"Reset");

    configOutput(OUTPUT,"CV");
    for(int k=0;k<16;k++) x0[k]=0.5f;
	}

  void process(const ProcessArgs& args) override {
    Mode mode = static_cast<Mode>((int)params[MODE_PARAM].getValue());
    float x0param=params[X_PARAM].getValue();
    float freqParam=params[FREQ_PARAM].getValue();
    float rParam=params[R_PARAM].getValue();
    float rCV=params[R_CV_PARAM].getValue();
    bool clk=inputs[CLK_INPUT].isConnected();
    if(rstTrigger.process(inputs[RST_INPUT].getVoltage())) {
      for(int k=0;k<16;k++) {
        x0[k]=clamp(x0param+inputs[X0_INPUT].getVoltage(k),0.f,1.f);
        reset();
      }
      rstPulse.trigger();
    }
    bool rstGate=rstPulse.process(args.sampleTime);
    int channels=clk?std::max(inputs[CLK_INPUT].getChannels(),1):std::max(inputs[VOCT_INPUT].getChannels(),1);
    for(int c=0;c<channels;c++) {
      float r=rParam+inputs[R_INPUT].getPolyVoltage(c)*0.05f*rCV;
      r=simd::clamp(r,0.f,1.f);
      if(clk) {
        if(clockTrigger[c].process(inputs[CLK_INPUT].getVoltage(c))&&!rstGate) {
          switch(mode) {
            case TENT:
              out[c]=tentOscSq[c].process(r);
              break;
            case CIRC:
              out[c]=circOscSq[c].process(r);
              break;
            case SAW:
              out[c]=sawMapOscSq[c].process(r);
              break;
            default:
              out[c]=oscSq[c].process(r);
          }
        }
      } else {
        float pitch=freqParam+inputs[VOCT_INPUT].getVoltage(c);
        float freq=dsp::FREQ_C4*std::pow(2.f,pitch)*2;
        float delta=freq*args.sampleTime;
        switch(mode) {
          case TENT:
            out[c]=tentOsc[c].process(delta,r);
            break;
          case CIRC:
            out[c]=circOsc[c].process(delta,r);
            break;
          case SAW:
            out[c]=sawMapOsc[c].process(delta,r);
            break;
          default:
            out[c]=osc[c].process(delta,r);
        }

      }
      outputs[OUTPUT].setVoltage(out[c]* 10.f - 5.f,c);
    }
    outputs[OUTPUT].setChannels(channels);
  }

  void reset() {
    for(int k=0;k<16;k++) {
      osc[k].reset(x0[k]);
      oscSq[k].reset(x0[k]);
      tentOsc[k].reset(x0[k]);
      tentOscSq[k].reset(x0[k]);
      circOsc[k].reset(x0[k]);
      circOscSq[k].reset(x0[k]);
      sawMapOsc[k].reset(x0[k]);
      sawMapOscSq[k].reset(x0[k]);
      out[k]=x0[k];
    }
  }

  void onReset(const ResetEvent &e) override {
    reset();
  }

};

struct MapWidget : ModuleWidget {
	MapWidget(Map* module) {
		setModule(module);
		setPanel(createPanel(asset::plugin(pluginInstance, "res/Map.svg")));
    float x=1.9f;
    addParam(createParam<TrimbotWhite>(mm2px(Vec(x,9)),module,Map::FREQ_PARAM));
    addInput(createInput<SmallPort>(mm2px(Vec(x,21)),module,Map::VOCT_INPUT));
    addInput(createInput<SmallPort>(mm2px(Vec(x,45)),module,Map::RST_INPUT));
    addInput(createInput<SmallPort>(mm2px(Vec(x,33)),module,Map::CLK_INPUT));
    auto modeParam=createParam<SelectParam>(mm2px(Vec(1.2,52.5)),module,Map::MODE_PARAM);
    modeParam->box.size=Vec(22,44);
    modeParam->init({"Log","Tent","Circ","Saw"});
    addParam(modeParam);
    addParam(createParam<TrimbotWhite>(mm2px(Vec(x,72)),module,Map::X_PARAM));
    addInput(createInput<SmallPort>(mm2px(Vec(x,79)),module,Map::X0_INPUT));
    addParam(createParam<TrimbotWhite>(mm2px(Vec(x,90)),module,Map::R_PARAM));
    addInput(createInput<SmallPort>(mm2px(Vec(x,97)),module,Map::R_INPUT));
    addParam(createParam<TrimbotWhite>(mm2px(Vec(x,104)),module,Map::R_CV_PARAM));

    addOutput(createOutput<SmallPort>(mm2px(Vec(x,116)),module,Map::OUTPUT));
	}
};


Model* modelMap = createModel<Map, MapWidget>("Map");