#include "dcb.h"
#include "rnd.h"

/***
 * port from the reverbsc opcode from Csound (C implementation (C) 2005 Istvan Varga)
 */

#define DEFAULT_SRATE   44100.0
#define DELAYPOS_SHIFT  28
#define DELAYPOS_SCALE  0x10000000
#define DELAYPOS_MASK   0x0FFFFFFF

struct ReverbParam {
  float delayTime;
  float rndDerivation;
  float rndVariationFreq;
  uint rndSeed;
};

template<size_t S>
struct DelayLine {
  ReverbParam rvParam;
  int writePos;
  int bufferSize;
  int readPos;
  int readPosFrac;
  int readPosFracInc;
  int rndVal;
  int randLineCnt;
  float filterState;
  float buf[S];

  void nextRnd() {
    if(rndVal<0) rndVal += 0x10000;
    rndVal = (rndVal * 15625 +1) & 0xFFFF;
    if(rndVal >= 0x8000) rndVal -= 0x10000;
  }

  void nextLineSeg(float sampleRate, float pitchMod) {
    nextRnd();
    randLineCnt = int(lroundf(sampleRate/rvParam.rndVariationFreq));
    double prvDel = writePos;
    prvDel -= (readPos + (double(readPosFrac)/double(DELAYPOS_SCALE)));
    while(prvDel < 0) prvDel += bufferSize;
    prvDel = prvDel / sampleRate;
    double nxtDel = rvParam.delayTime + (rndVal * rvParam.rndDerivation / 32768.0)*pitchMod;
    double phsInc =((prvDel-nxtDel)/double(randLineCnt))*sampleRate+1.0;
    readPosFracInc = int(lround(phsInc*DELAYPOS_SCALE));

  }
  void init(float sampleRate, float pitchMod) {
    bufferSize = uint((rvParam.delayTime + rvParam.rndDerivation * pitchMod * 1.125f)*sampleRate+16.5f);
    writePos = 0;
    rndVal = rvParam.rndSeed;
    double _readPos = rndVal * rvParam.rndDerivation / 32768.0;
    _readPos = rvParam.delayTime + (_readPos*pitchMod);
    _readPos = bufferSize - (_readPos*sampleRate);
    readPos = int(_readPos);
    _readPos = (_readPos - readPos) * (double)DELAYPOS_SCALE;
    readPosFrac = int(lround(_readPos));
    nextLineSeg(sampleRate,pitchMod);
    filterState = 0;
    memset(buf,0,S);
  }
  float process(float in, float fb, float dmp,float pm,float sampleRate) {
    buf[writePos] = in - filterState;
    if (++writePos >= bufferSize)
      writePos -= bufferSize;
    /* read from delay line with cubic interpolation */
    if (readPosFrac >= DELAYPOS_SCALE) {
      readPos += (readPosFrac >> DELAYPOS_SHIFT);
      readPosFrac &= DELAYPOS_MASK;
    }
    if (readPos >= bufferSize)
      readPos -= bufferSize;
    //readPos = lp->readPos;
    double frac = (double) readPosFrac * (1.0 / (double) DELAYPOS_SCALE);
    /* calculate interpolation coefficients */
    double a0,a1,a2,am1;
    a2 = frac * frac; a2 -= 1.0; a2 *= (1.0 / 6.0);
    a1 = frac; a1 += 1.0; a1 *= 0.5; am1 = a1 - 1.0;
    a0 = 3.0 * a2; a1 -= a0; am1 -= a2; a0 -= frac;
    /* read four samples for interpolation */
    double vm1,v0,v1,v2;
    if (readPos > 0 && readPos < (bufferSize - 2)) {
      vm1 = (double) (buf[readPos - 1]);
      v0  = (double) (buf[readPos]);
      v1  = (double) (buf[readPos + 1]);
      v2  = (double) (buf[readPos + 2]);
    }
    else {
      /* at buffer wrap-around, need to check index */
      if (--readPos < 0) readPos += bufferSize;
      vm1 = (double) buf[readPos];
      if (++readPos >= bufferSize) readPos -= bufferSize;
      v0 = (double) buf[readPos];
      if (++readPos >= bufferSize) readPos -= bufferSize;
      v1 = (double) buf[readPos];
      if (++readPos >= bufferSize) readPos -= bufferSize;
      v2 = (double) buf[readPos];
    }
    v0 = (am1 * vm1 + a0 * v0 + a1 * v1 + a2 * v2) * frac + v0;
    /* update buffer read position */
    readPosFrac += readPosFracInc;
    /* apply feedback gain and lowpass filter */
    v0 *= (double) fb;
    v0 = (filterState - v0) * dmp + v0;
    filterState = float(v0);
    if (--(randLineCnt) <= 0)
      nextLineSeg(sampleRate,pm);

    return float(v0);
  }
};

struct RSC : Module {
	enum ParamId {
		CF_PARAM,FB_PARAM,PM_PARAM,WET_PARAM,PARAMS_LEN
	};
	enum InputId {
    L_INPUT,
    R_INPUT,
    INPUTS_LEN
	};
	enum OutputId {
    L_OUTPUT,
    R_OUTPUT,
		OUTPUTS_LEN
	};
	enum LightId {
		LIGHTS_LEN
	};
  DelayLine<131072> delayLines[8];
  float dmp = 1.f;
  float prvLPFreq = 0.f;
  bool update;
  float pm;

  ReverbParam rvParam[8] = {
    { (2473.0 / DEFAULT_SRATE), 0.0010, 3.100,  1966 },
    { (2767.0 / DEFAULT_SRATE), 0.0011, 3.500, 29491 },
    { (3217.0 / DEFAULT_SRATE), 0.0017, 1.110, 22937 },
    { (3557.0 / DEFAULT_SRATE), 0.0006, 3.973,  9830 },
    { (3907.0 / DEFAULT_SRATE), 0.0010, 2.341, 20643 },
    { (4127.0 / DEFAULT_SRATE), 0.0011, 1.897, 22937 },
    { (2143.0 / DEFAULT_SRATE), 0.0017, 0.891, 29491 },
    { (1933.0 / DEFAULT_SRATE), 0.0006, 3.221, 14417 }
  };

  void init(float sampleRate, float pitchMod) {
    for(int k=0;k< 8;k++) {
      delayLines[k].rvParam = rvParam[k];
      delayLines[k].init(sampleRate,pitchMod);
    }
  }
  void onAdd(const AddEvent &e) override {
    pm = params[PM_PARAM].getValue();
    init(APP->engine->getSampleRate(),pm);
  }
	RSC() {
		config(PARAMS_LEN, INPUTS_LEN, OUTPUTS_LEN, LIGHTS_LEN);
    configParam(CF_PARAM,7.f,14.f,14.f,"Frequency"," Hz",2,1);
    configParam(FB_PARAM,0.f,1.f,0.6f,"Feedback");
    configParam(PM_PARAM,0.0f,6.f,0.5f,"Pitch Mod");
    configParam(WET_PARAM,0.f,1.f,0.5f,"Wet");
    configInput(L_INPUT,"Left");
    configInput(R_INPUT,"Right");
    configOutput(L_OUTPUT,"Left");
    configOutput(R_OUTPUT,"Right");
    configBypass(L_INPUT,L_OUTPUT);
    configBypass(R_INPUT,R_OUTPUT);
	}

	void process(const ProcessArgs& args) override {
    if(update) {
      update = false;
      pm = params[PM_PARAM].getValue();
      init(args.sampleRate,pm);
    }
    if(inputs[L_INPUT].isConnected()) {
      float lInput = inputs[L_INPUT].getVoltage();
      float rInput = inputs[R_INPUT].getNormalVoltage(lInput);
      float _inL = lInput / 5.f;
      float _inR = rInput / 5.f;
      float freq = params[CF_PARAM].getValue();
      float cf=powf(2.0f,freq);

      if(cf!=prvLPFreq) {
        prvLPFreq = cf;
        dmp = 2.0f - cosf(prvLPFreq * 2*M_PI_2f32 / args.sampleRate);
        dmp = dmp - sqrtf(dmp * dmp - 1.0);
      }
      float inL = 0.f;
      float inR = 0.f;
      for (int k = 0; k < 8; k++)
        inL += delayLines[k].filterState;
      inL *= 0.25;
      inR =inL+_inR;
      inL =inL+_inL;
      float outL = 0.f;
      float outR = 0.f;
      float fb = params[FB_PARAM].getValue();
      for(int k=0;k<8;k++) {
        if(k&1) {
          outR += delayLines[k].process(inR,fb,dmp,pm,args.sampleRate);
        } else {
          outL += delayLines[k].process(inL,fb,dmp,pm,args.sampleRate);
        }
      }
      float wet = params[WET_PARAM].getValue();
      outputs[L_OUTPUT].setVoltage(wet*outL*5.f + (1-wet)*lInput);
      outputs[R_OUTPUT].setVoltage(wet*outR*5.f + (1-wet)*rInput);
    }

  }
};


struct RSCWidget : ModuleWidget {
	RSCWidget(RSC* module) {
		setModule(module);
		setPanel(createPanel(asset::plugin(pluginInstance, "res/RSC.svg")));

		addChild(createWidget<ScrewSilver>(Vec(RACK_GRID_WIDTH, 0)));
		addChild(createWidget<ScrewSilver>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, 0)));
		addChild(createWidget<ScrewSilver>(Vec(RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));
		addChild(createWidget<ScrewSilver>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));
    addParam(createParam<TrimbotWhite>(mm2px(Vec(2,MHEIGHT-115.f)),module,RSC::CF_PARAM));
    addParam(createParam<TrimbotWhite>(mm2px(Vec(2,MHEIGHT-91.f)),module,RSC::FB_PARAM));
    auto pm = createParam<UpdateOnReleaseKnob>(mm2px(Vec(2,MHEIGHT-79.f)),module,RSC::PM_PARAM);
    pm->update = &module->update;
    addParam(pm);
    addParam(createParam<TrimbotWhite>(mm2px(Vec(2,MHEIGHT-67.f)),module,RSC::WET_PARAM));

    addInput(createInput<SmallPort>(mm2px(Vec(1.9f,MHEIGHT-55.f)),module,RSC::L_INPUT));
    addInput(createInput<SmallPort>(mm2px(Vec(1.9f,MHEIGHT-43.f)),module,RSC::R_INPUT));
    addOutput(createOutput<SmallPort>(mm2px(Vec(1.9f,MHEIGHT-31.f)),module,RSC::L_OUTPUT));
    addOutput(createOutput<SmallPort>(mm2px(Vec(1.9f,MHEIGHT-19.f)),module,RSC::R_OUTPUT));

  }
};


Model* modelRSC = createModel<RSC, RSCWidget>("RSC");