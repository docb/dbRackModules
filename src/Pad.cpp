#include "dcb.h"
#include "rnd.h"
#include <thread>
#include <mutex>

#define LENGTH 262144
/**
 *  padsynth algorithm from Paul Nasca
 */
struct PadTable {
  float *table[2];
  int currentTable=0;
  dsp::RealFFT _realFFT;
  RND rnd;
  PadTable() : _realFFT(LENGTH) {
    table[0]=new float[LENGTH];
    table[1]=new float[LENGTH];
  }

  ~PadTable() {
    delete[] table[0];
    delete[] table[1];
  }

  float profile(float fi,float bwi) {
    float x=fi/bwi;
    x*=x;
    if(x>14.71280603)
      return 0.0;//this avoids computing the e^(-x^2) where it's results are very close to zero
    return expf(-x)/bwi;
  };

  void generate(const std::vector<float> &partials,float sampleRate,float fundFreq,float partialBW,float bwScale) {
    auto input=new float[LENGTH*2];
    memset(input,0,LENGTH*2*sizeof(float));
    input[0]=0.f;
    input[1]=0.f;
    for(uint k=1;k<partials.size();k++) {
      if(partials[k]>0.f) {
        float partialHz=fundFreq*(k);
        float freqIdx=partialHz/((float)sampleRate);
        float bandwidthHz=(std::pow(2.0,partialBW/1200.0)-1.0)*fundFreq*std::pow(float(k),bwScale);
        float bandwidthSamples=bandwidthHz/(2.0*sampleRate);
        for(int i=0;i<LENGTH;i++) {
          float fttIdx=((float)i)/((float)LENGTH*2);
          float profileIdx=fttIdx-freqIdx;
          float profileSample=profile(profileIdx,bandwidthSamples);
          input[i*2]+=(profileSample*partials[k]);
        }
      }
    }
    for(int k=0;k<LENGTH;k++) {
      auto randomPhase=float(rnd.nextDouble()*M_PI*2);
      input[k*2]=(input[k*2]*cosf(randomPhase));
      input[k*2+1]=(input[k*2]*sinf(randomPhase));
    };
    int idx=(currentTable+1)%2;
    _realFFT.irfft(input,table[idx]);
    _realFFT.scale(table[idx]);
    // this does the trick to avoid clicks
    currentTable=idx;
    delete[] input;
  }

  float lookup(float phs) {
    return table[currentTable][int(phs*float(LENGTH))&(LENGTH-1)];
  }


};

struct Pad : Module {
  enum ParamId {
    BW_PARAM,SCALE_PARAM,SEED_PARAM,MTH_PARAM,FUND_PARAM,PARAMS_LEN
  };
  enum InputId {
    VOCT_INPUT,INPUTS_LEN
  };
  enum OutputId {
    L_OUTPUT,R_OUTPUT,OUTPUTS_LEN
  };
  enum LightId {
    LIGHTS_LEN
  };

  enum Methods {
    HARMONIC_MIN,EVEN_MIN,ODD_MIN,HARMONIC_WB,EVEN_WB,ODD_WB,VOICE,NUM_METHODS
  };

  RND rnd;
  PadTable pt;
  std::vector<float> partials;

  void generatePartials() {
    partials.clear();
    partials.push_back(0.f);
    partials.push_back(1.f);
    float seed=params[SEED_PARAM].getValue();
    int method=params[MTH_PARAM].getValue();
    uint is=uint(seed*UINT_MAX);
    rnd.reset(is);
    switch(method) {
      case EVEN_MIN:
        for(int k=2;k<80;k++) {
          if(k%2==0)
            partials.push_back(rnd.nextMin(k/4));
          else
            partials.push_back(0);
        }
        break;
      case ODD_MIN:
        for(int k=2;k<80;k++) {
          if(k%2==1)
            partials.push_back(rnd.nextMin(k/4));
          else
            partials.push_back(0);
        }
        break;
      case HARMONIC_WB:
        for(int k=2;k<80;k++) {
          partials.push_back(rnd.nextWeibull(k/4.f));
        }
        break;
      case EVEN_WB:
        for(int k=2;k<80;k++) {
          if(k%2==0)
            partials.push_back(rnd.nextWeibull(k/4.f));
          else
            partials.push_back(0);
        }
        break;
      case ODD_WB:
        for(int k=2;k<80;k++) {
          if(k%2==1)
            partials.push_back(rnd.nextWeibull(k/4.f));
          else
            partials.push_back(0);
        }
        break;
      case VOICE:
        for(int k=2;k<80;k++) {
          partials.push_back(rnd.nextMin(k/4));
        }
        // add some amp to the vocal freqs
        partials[12]=0.8f;
        partials[13]=0.4f;
        partials[18]=0.8f;
        partials[19]=0.8f;
        partials[32]=0.7f;
        partials[49]=0.7f;
        partials[50]=0.7f;
        partials[53]=0.4f;
        partials[54]=0.8f;
        partials[69]=0.6f;
        partials[75]=0.6f;
        partials[79]=0.6f;
        break;
      case HARMONIC_MIN:
      default:
        for(int k=2;k<80;k++) {
          partials.push_back(rnd.nextMin(k/4));
        }
        break;

    }
    regenerate();
  }

  void regenerate() {
    float bw=params[BW_PARAM].getValue();
    float scale=params[SCALE_PARAM].getValue();
    pt.generate(partials,APP->engine->getSampleRate(),fund,bw,scale);
  }

  float phs[16]={};
  float fund=32.7f;

  Pad() {
    config(PARAMS_LEN,INPUTS_LEN,OUTPUTS_LEN,LIGHTS_LEN);
    configParam(BW_PARAM,0.5f,60,10,"Bandwidth");
    configParam(SCALE_PARAM,0.5f,4.f,1,"Bandwidth Scale");
    configParam(SEED_PARAM,0.f,1.f,0.5,"Seed");
    configParam(FUND_PARAM,4.f,9.f,5.03f,"Frequency"," Hz",2,1);
    configSwitch(MTH_PARAM,0.f,NUM_METHODS-0.1,0,"Method",{"Harmonic Min","Even Min","Odd Min","Harmonic Weibull","Even Weibull","Odd Weibull","Voice"});
    getParamQuantity(MTH_PARAM)->snapEnabled = true;
    configInput(VOCT_INPUT,"V/Oct");
    configOutput(L_OUTPUT,"Left");
    configOutput(R_OUTPUT,"Right");
  }

  void onAdd(const AddEvent &e) override {
    generatePartials();
  }


  void process(const ProcessArgs &args) override {
    int channels=inputs[VOCT_INPUT].getChannels();
    for(int k=0;k<channels;k++) {
      float voct=inputs[VOCT_INPUT].getVoltage(k);
      float freq=261.626f*powf(2.0f,voct-1);
      float oscFreq=(freq*args.sampleRate)/(LENGTH*fund);
      float dPhase=oscFreq*args.sampleTime;
      phs[k]+=dPhase;
      phs[k]-=floorf(phs[k]);

      float outL=pt.lookup(phs[k]);
      float outR=pt.lookup(phs[k]+0.5);
      outputs[L_OUTPUT].setVoltage(outL/2.5f,k);
      outputs[R_OUTPUT].setVoltage(outR/2.5f,k);
    }
    outputs[L_OUTPUT].setChannels(channels);
    outputs[R_OUTPUT].setChannels(channels);
  }
};


template<typename T>
struct UpdatePartialsKnob : TrimbotWhite {
  T *module=nullptr;
  UpdatePartialsKnob() : TrimbotWhite() {}

  void onDragEnd(const DragEndEvent &e) override {
    SvgKnob::onDragEnd(e);
    if(e.button==GLFW_MOUSE_BUTTON_LEFT) {
      if(module)
        module->generatePartials();
    }
  }
};

template<typename T>
struct UpdateKnob : TrimbotWhite {
  T *module=nullptr;
  UpdateKnob() : TrimbotWhite() {
  }
  void onDragEnd(const DragEndEvent &e) override {
    SvgKnob::onDragEnd(e);
    if(e.button==GLFW_MOUSE_BUTTON_LEFT) {
      if(module)
        module->regenerate();
    }
  }
};

struct PadWidget : ModuleWidget {
  PadWidget(Pad *module) {
    setModule(module);
    setPanel(createPanel(asset::plugin(pluginInstance,"res/Pad.svg")));

    addChild(createWidget<ScrewSilver>(Vec(RACK_GRID_WIDTH,0)));
    addChild(createWidget<ScrewSilver>(Vec(box.size.x-2*RACK_GRID_WIDTH,0)));
    addChild(createWidget<ScrewSilver>(Vec(RACK_GRID_WIDTH,RACK_GRID_HEIGHT-RACK_GRID_WIDTH)));
    addChild(createWidget<ScrewSilver>(Vec(box.size.x-2*RACK_GRID_WIDTH,RACK_GRID_HEIGHT-RACK_GRID_WIDTH)));
    float xpos=1.9f;
    auto bwParam=createParam<UpdateKnob<Pad>>(mm2px(Vec(xpos,MHEIGHT-115.f)),module,Pad::BW_PARAM);
    bwParam->module=module;
    addParam(bwParam);
    auto scaleParam=createParam<UpdateKnob<Pad>>(mm2px(Vec(xpos,MHEIGHT-103.f)),module,Pad::SCALE_PARAM);
    scaleParam->module=module;
    addParam(scaleParam);
    auto seedParam=createParam<UpdatePartialsKnob<Pad>>(mm2px(Vec(xpos,MHEIGHT-91.f)),module,Pad::SEED_PARAM);
    seedParam->module=module;
    addParam(seedParam);
    auto mth=createParam<UpdatePartialsKnob<Pad>>(mm2px(Vec(xpos,MHEIGHT-79.f)),module,Pad::MTH_PARAM);
    mth->module=module;
    addParam(mth);

    addInput(createInput<SmallPort>(mm2px(Vec(xpos,MHEIGHT-43.f)),module,Pad::VOCT_INPUT));
    addOutput(createOutput<SmallPort>(mm2px(Vec(xpos,MHEIGHT-31.f)),module,Pad::L_OUTPUT));
    addOutput(createOutput<SmallPort>(mm2px(Vec(xpos,MHEIGHT-19.f)),module,Pad::R_OUTPUT));
  }
};


Model *modelPad=createModel<Pad,PadWidget>("Pad");