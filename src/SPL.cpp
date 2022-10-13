#include "dcb.h"

struct StepOsc {
  int pos=0;
  float x;
  float phs;
  float *pts;
  int len;

  void init(float *_pts,int _len) {
    pts=_pts;
    len=_len;
    len=std::max(_len,2);
    x=pts[0];
    phs=0;
    pos=1;
  }

  void next() {
    x=pts[pos];
    pos=(pos+1)%len;
  }

  void updatePhs(float sampleTime,float freq) {
    phs+=(sampleTime*freq*len);
    if(phs>1.f) {
      phs=phs-float((int)phs);
      next();
    }
  }

  float process() {
    return x;
  }

  void reset() {
    x=pts[0];
    phs=0;
    pos=1;
  }
};

struct LinOsc {
  int pos=0;
  float x0,x1;
  float phs;
  float *pts;
  int len;

  void init(float *_pts,int _len) {
    pts=_pts;
    len=_len;
    len=std::max(_len,2);
    x0=pts[0];
    x1=pts[1];
    phs=0;
    pos=2;
  }

  void next() {
    x0=x1;
    x1=pts[pos];
    pos=(pos+1)%len;
  }

  void updatePhs(float sampleTime,float freq) {
    phs+=(sampleTime*freq*len);
    if(phs>1.f) {
      phs=phs-float((int)phs);
      next();
    }
  }

  float process() {
    return x0+phs*(x1-x0);
  }

  void reset() {
    x0=pts[0];
    x1=pts[1];
    phs=0;
    pos=2;
  }
};

struct SPLOsc {
  int pos=0;
  float curPts[4];
  float a0,a1,a2,a3;
  float phs;
  float *pts;
  int len;

  void init(float *_pts,int _len) {
    pts=_pts;
    len=std::max(_len,4);
    for(int j=0;j<4;j++) {
      curPts[j]=pts[j];
    }
    phs=0;
    pos=3;
    a0=curPts[3]-curPts[2]-curPts[0]+curPts[1];
    a1=curPts[0]-curPts[1]-a0;
    a2=curPts[2]-curPts[0];
    a3=curPts[1];
  }

  void next() {
    curPts[0]=curPts[1];
    curPts[1]=curPts[2];
    curPts[2]=curPts[3];
    curPts[3]=pts[pos];
    a0=curPts[3]-curPts[2]-curPts[0]+curPts[1];
    a1=curPts[0]-curPts[1]-a0;
    a2=curPts[2]-curPts[0];
    a3=curPts[1];
    pos=(pos+1)%len;
  }

  void updatePhs(float sampleTime,float freq) {
    phs+=(sampleTime*freq*len);
    if(phs>1.f) {
      phs=phs-float((int)phs);
      next();
    }
  }

  float process() {
    return ((a0*phs+a1)*phs+a2)*phs+a3;
  }

  void reset() {
    phs=0;
    pos=3;
    a0=curPts[3]-curPts[2]-curPts[0]+curPts[1];
    a1=curPts[0]-curPts[1]-a0;
    a2=curPts[2]-curPts[0];
    a3=curPts[1];
  }
};

struct SPLPhsOsc {
  float *pts;
  int len=0;
  void init(float *_pts,int _len) {
    pts=_pts;
    len=_len;
  }
  float process(float phs) {
    if(len==0) return 0;
    if(phs>=1.f) phs=0.99999f;
    if(phs<0.f) phs = 0.f;
    int idx0 = int(phs*float(len))%len;
    //idx0 = (idx0==0)?len-1:idx0-1;
    int idx1 = (idx0+1)%len;
    int idx2 = (idx0+2)%len;
    int idx3 = (idx0+3)%len;
    float a0=pts[idx3]-pts[idx2]-pts[idx0]+pts[idx1];
    float a1=pts[idx0]-pts[idx1]-a0;
    float a2=pts[idx2]-pts[idx0];
    float a3=pts[idx1];
    float start = float(idx0)/float(len);
    float stepPhs = (phs - start)*len;
    return ((a0*stepPhs+a1)*stepPhs+a2)*stepPhs+a3;
  }
};
struct LinPhsOsc {
  float *pts;
  int len=0;
  void init(float *_pts,int _len) {
    pts=_pts;
    len=_len;
  }
  float process(float phs) {
    if(len==0) return 0;
    if(phs>=1.f) phs=0.99999f;
    if(phs<0.f) phs = 0.f;
    int idx = int(phs*float(len))%len;
    int idx2 = (idx+1)%len;
    float start = float(idx)/float(len);
    float stepPhs = (phs - start)*len;
    return pts[idx]+stepPhs*(pts[idx2]-pts[idx]);
  }
};

struct StepPhsOsc {
  float *pts;
  int len=0;
  void init(float *_pts,int _len) {
    pts=_pts;
    len=_len;
  }
  float process(float phs) {
    if(len==0) return 0;
    if(phs>=1.f) phs=0.99999f;
    if(phs<0.f) phs = 0.f;
    int idx = int(phs*float(len))%len;
    return pts[idx];
  }
};


struct SPL : Module {
  enum ParamId {
    FREQ_PARAM,PARAMS_LEN
  };
  enum InputId {
    VOCT_INPUT,PTS_INPUT,RST_INPUT,PHS_INPUT,INPUTS_LEN
  };
  enum OutputId {
    SPL_OUTPUT,LIN_OUTPUT,STEP_OUTPUT,OUTPUTS_LEN
  };
  enum LightId {
    LIGHTS_LEN
  };
  SPLOsc osc[16];
  LinOsc linosc[16];
  StepOsc steposc[16];
  LinPhsOsc linPhsOsc[16];
  StepPhsOsc stepPhsOsc[16];
  SPLPhsOsc splPhsOsc[16];
  float pts[16]={};
  int len=0;
  dsp::SchmittTrigger rstTrigger[16];
  dsp::ClockDivider divider;
	SPL() {
		config(PARAMS_LEN, INPUTS_LEN, OUTPUTS_LEN, LIGHTS_LEN);
    configParam(FREQ_PARAM,-14.f,4.f,0.f,"Frequency"," Hz",2,dsp::FREQ_C4);
    configInput(VOCT_INPUT,"V/Oct");
    configInput(PTS_INPUT,"Points");
    configInput(PHS_INPUT,"Phase");
    configInput(RST_INPUT,"Reset");
    configOutput(STEP_OUTPUT,"Steps");
    configOutput(LIN_OUTPUT,"Lines");
    configOutput(SPL_OUTPUT,"Cubic Splines");
    divider.setDivision(32);
	}
  void onAdd(const AddEvent &e) override {
    init();
  }

  void init() {
    for(int c=0;c<16;c++) {
      osc[c].init(pts,len);
      linosc[c].init(pts,len);
      steposc[c].init(pts,len);
      splPhsOsc[c].init(pts,len);
      linPhsOsc[c].init(pts,len);
      stepPhsOsc[c].init(pts,len);
    }
  }
  void processVOct(const ProcessArgs &args) {
    int channels=std::max(inputs[VOCT_INPUT].getChannels(),1);
    for(int c=0;c<channels;c++) {
      bool rst = rstTrigger[c].process(inputs[RST_INPUT].getPolyVoltage(c));
      float freq=dsp::FREQ_C4*powf(2.0f,inputs[VOCT_INPUT].getVoltage(c)+params[FREQ_PARAM].getValue());
      if(outputs[SPL_OUTPUT].isConnected()) {
        float outL=osc[c].process();
        osc[c].updatePhs(args.sampleTime,freq);
        if(rst) osc[c].reset();
        outputs[SPL_OUTPUT].setVoltage(outL*4.f,c);
      }

      if(outputs[LIN_OUTPUT].isConnected()) {
        if(rst) linosc[c].reset();
        float outLin=linosc[c].process();
        linosc[c].updatePhs(args.sampleTime,freq);

        outputs[LIN_OUTPUT].setVoltage(outLin*5.f,c);
      }
      if(outputs[STEP_OUTPUT].isConnected()) {
        if(rst) steposc[c].reset();
        float outStep=steposc[c].process();
        steposc[c].updatePhs(args.sampleTime,freq);
        outputs[STEP_OUTPUT].setVoltage(outStep*5.f,c);
      }
    }
    if(outputs[SPL_OUTPUT].isConnected())
      outputs[SPL_OUTPUT].setChannels(channels);
    if(outputs[LIN_OUTPUT].isConnected())
      outputs[LIN_OUTPUT].setChannels(channels);
    if(outputs[STEP_OUTPUT].isConnected())
      outputs[STEP_OUTPUT].setChannels(channels);
  }

  void processPhs(const ProcessArgs& args) {

    int channels = std::max(inputs[PHS_INPUT].getChannels(),1);
    for(int c=0;c<channels;c++) {
      float phs = (inputs[PHS_INPUT].getVoltage(c)+5.f)/10.f;
      outputs[LIN_OUTPUT].setVoltage(linPhsOsc[c].process(phs)*5.f,c);
      outputs[STEP_OUTPUT].setVoltage(stepPhsOsc[c].process(phs)*5.f,c);
      outputs[SPL_OUTPUT].setVoltage(splPhsOsc[c].process(phs)*5.f,c);
    }
    outputs[LIN_OUTPUT].setChannels(channels);
    outputs[STEP_OUTPUT].setChannels(channels);
    outputs[SPL_OUTPUT].setChannels(channels);
  }

	void process(const ProcessArgs& args) override {
    if(divider.process()) {
      int ptsChannels=std::max(inputs[PTS_INPUT].getChannels(),2);
      if(ptsChannels!=len) {
        for(int c=0;c<16;c++) {
          len=ptsChannels;
          osc[c].init(pts,len);
          linosc[c].init(pts,len);
          steposc[c].init(pts,len);
          linPhsOsc[c].init(pts,len);
          stepPhsOsc[c].init(pts,len);
          splPhsOsc[c].init(pts,len);
        }
      }
      for(int k=0;k<16;k++) {
        pts[k]=inputs[PTS_INPUT].getVoltage(k)/5.f;
      }
    }
    if(inputs[PHS_INPUT].isConnected()) {
      processPhs(args);
    } else {
      processVOct(args);
    }
	}
};


struct SPLWidget : ModuleWidget {
	SPLWidget(SPL* module) {
		setModule(module);
		setPanel(createPanel(asset::plugin(pluginInstance, "res/SPL.svg")));

    float x=1.9;
    addParam(createParam<TrimbotWhite>(mm2px(Vec(x,9)),module,SPL::FREQ_PARAM));
    addInput(createInput<SmallPort>(mm2px(Vec(x,21)),module,SPL::VOCT_INPUT));
    addInput(createInput<SmallPort>(mm2px(Vec(x,33)),module,SPL::PTS_INPUT));
    addInput(createInput<SmallPort>(mm2px(Vec(x,45)),module,SPL::RST_INPUT));
    addInput(createInput<SmallPort>(mm2px(Vec(x,57)),module,SPL::PHS_INPUT));
    addOutput(createOutput<SmallPort>(mm2px(Vec(x,92)),module,SPL::STEP_OUTPUT));
    addOutput(createOutput<SmallPort>(mm2px(Vec(x,104)),module,SPL::LIN_OUTPUT));
    addOutput(createOutput<SmallPort>(mm2px(Vec(x,116)),module,SPL::SPL_OUTPUT));
	}
};


Model* modelSPL = createModel<SPL, SPLWidget>("SPL");