#include "dcb.h"
#include "rnd.h"
#include <thread>
#include <mutex>

#define LENGTH 262144
/**
 *  padsynth algorithm from Paul Nasca
 */
template<size_t S>
struct PadTable {
  float *table[2];
  int currentTable=0;
  dsp::RealFFT _realFFT;
  unsigned long counter=0;
  unsigned long period;

  //float fade=0;
  RND rnd;
  PadTable() : _realFFT(S) {
    table[0]=new float[S];
    table[1]=new float[S];
    memset(table[0],0.f,S);
    memset(table[1],0.f,S);
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

  void generate(const std::vector<float> &partials,float sampleRate,float fundFreq,float partialBW,float bwScale,float fade) {
    if(counter>0) return;
    auto input=new float[S*2];
    auto work=new float[S*2];
    memset(input,0,S*2*sizeof(float));
    input[0]=0.f;
    input[1]=0.f;
    for(unsigned int k=0;k<partials.size();k++) {
      if(partials[k]>0.f) {
        float partialHz=fundFreq*float(k+1);
        float freqIdx=partialHz/((float)sampleRate);
        float bandwidthHz=(std::pow(2.0f,partialBW/1200.0f)-1.0f)*fundFreq*std::pow(float(k+1),bwScale);
        float bandwidthSamples=bandwidthHz/(2.0f*sampleRate);
        for(unsigned i=0;i<S;i++) {
          float fttIdx=((float)i)/((float)S*2);
          float profileIdx=fttIdx-freqIdx;
          float profileSample=profile(profileIdx,bandwidthSamples);
          input[i*2]+=(profileSample*partials[k]);
        }
      }
    }
    for(unsigned int k=0;k<S;k++) {
      auto randomPhase=float(rnd.nextDouble()*M_PI*2);
      input[k*2]=(input[k*2]*cosf(randomPhase));
      input[k*2+1]=(input[k*2]*sinf(randomPhase));
    };
    int idx=(currentTable+1)%2;
    pffft_transform_ordered(_realFFT.setup, input, table[idx], work, PFFFT_BACKWARD);
    _realFFT.scale(table[idx]);

    period=counter=uint32_t(fade*sampleRate);

    currentTable=idx;
    delete[] input;
    delete[] work;
  }

  float lookup(double phs) {
    return table[currentTable][int(phs*float(S))&(S-1)];
  }

  float lookupS(double phs,float mix) {
    float future=table[currentTable][int(phs*float(S))&(S-1)];
    int lastTable=currentTable==0?1:0;
    float last=table[lastTable][int(phs*float(S))&(S-1)];
    return future*(1-mix)+last*mix;
  }
};

struct Pad : Module {
  enum ParamId {
    BW_PARAM,SCALE_PARAM,SEED_PARAM,MTH_PARAM,FUND_PARAM,AMP_PARAM,PARAMS_LEN
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
    HARMONIC_MIN,EVEN_MIN,ODD_MIN,HARMONIC_WB,EVEN_WB,ODD_WB,VOICE,STATIC,NUM_METHODS
  };

  RND rnd;
  PadTable<LENGTH> pt;
  std::vector<float> partials;
  float sr=48000.f;
  float bw=10.f;
  float scl=1.f;

  float fade=0;

  void generatePartials() {
    partials.clear();
    //partials.push_back(0.f);
    partials.push_back(params[AMP_PARAM].getValue());
    float seed=params[SEED_PARAM].getValue();
    int method=(int)params[MTH_PARAM].getValue();
    auto is=(unsigned long long)(seed*(float)UINT_MAX);
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
      case STATIC: {
        bool even=false;
        if(seed>0.5) {
          seed=1.5f-seed;
          even=true;
        }
        seed*=2;
        for(int k=2;k<80;k++) {
          float amp=1.f/float(k);
          if(k%2==int(even)) {
            amp*=seed;
          }
          partials.push_back(amp);
        }
        break;
      }
      case HARMONIC_MIN:
      default:
        for(int k=2;k<80;k++) {
          partials.push_back(rnd.nextMin(k/4));
        }
        break;

    }
  }

  void regenerate() {
    float bwth=params[BW_PARAM].getValue();
    float scale=params[SCALE_PARAM].getValue();
    pt.generate(partials,APP->engine->getSampleRate(),fund,bwth,scale,fade);
  }

  double phs[16]={};
  float fund=32.7f;
  std::mutex mutex;

  Pad() {
    config(PARAMS_LEN,INPUTS_LEN,OUTPUTS_LEN,LIGHTS_LEN);
    configParam(BW_PARAM,0.5f,60,10,"Bandwidth");
    configParam(SCALE_PARAM,0.5f,4.f,1,"Bandwidth Scale");
    configParam(SEED_PARAM,0.f,1.f,0.5,"Seed");
    configParam(AMP_PARAM,0.f,1.f,1,"Fundamental Amp");
    configParam(FUND_PARAM,4.f,9.f,5.03f,"Frequency"," Hz",2,1);
    configSwitch(MTH_PARAM,0.f,NUM_METHODS-1,0,"Method",{"Harmonic Min","Even Min","Odd Min","Harmonic Weibull","Even Weibull","Odd Weibull","Voice","Static"});
    getParamQuantity(MTH_PARAM)->snapEnabled = true;
    configInput(VOCT_INPUT,"V/Oct");
    configOutput(L_OUTPUT,"Left");
    configOutput(R_OUTPUT,"Right");
  }

  void onAdd(const AddEvent &e) override {
    generatePartials();
  }

  void setFadeTime(float value) {
    fade=value;
  }

  float getFadeTime() {
    return fade;
  }

  void process(const ProcessArgs &args) override {
    int channels=inputs[VOCT_INPUT].getChannels();
    float mix=0;
    if(pt.counter>0) {
      mix=float(pt.counter)/float(pt.period);
      pt.counter--;
    }
    for(int k=0;k<channels;k++) {
      float voct=inputs[VOCT_INPUT].getVoltage(k);
      float freq=261.626f*powf(2.0f,voct-1);
      double oscFreq=double(freq*args.sampleRate)/double(LENGTH*double(fund));
      double dPhase=oscFreq*args.sampleTime;
      phs[k]+=dPhase;
      phs[k]-=floor(phs[k]);

      float outL=pt.lookupS(phs[k],mix);
      float outR=pt.lookupS(phs[k]+0.5,mix);
      outputs[L_OUTPUT].setVoltage(outL/2.5f,k);
      outputs[R_OUTPUT].setVoltage(outR/2.5f,k);
    }
    outputs[L_OUTPUT].setChannels(channels);
    outputs[R_OUTPUT].setChannels(channels);
  }

  void regenerateSave(float bw,float scale,float sr,float fund,float fade) {
    if(mutex.try_lock()) {
      generatePartials();
      pt.generate(partials,sr,fund,bw,scale,fade);
      mutex.unlock();
    }
  }

  void fromJson(json_t *rootJ) override {
    Module::fromJson(rootJ);
    bw=params[BW_PARAM].getValue();
    scl=params[SCALE_PARAM].getValue();
    sr=APP->engine->getSampleRate();
    std::thread t1(&Pad::regenerateSave,this,bw,scl,sr,fund,fade);
    t1.detach();
  }
};


template<typename T>
struct UpdatePartialsKnob : TrimbotWhite {
  T *module=nullptr;
  bool contextMenu;

  UpdatePartialsKnob() : TrimbotWhite() {}
  void onButton(const ButtonEvent& e) override {
    if (e.action == GLFW_PRESS && e.button == GLFW_MOUSE_BUTTON_RIGHT && (e.mods & RACK_MOD_MASK) == 0) {
      contextMenu=true;
    } else {
      contextMenu=false;
    }
    Knob::onButton(e);
  }

  void onChange(const ChangeEvent& e) override {
    SvgKnob::onChange(e);
    if(module && contextMenu) {
      module->generatePartials();
      module->regenerate();
    }
    contextMenu=false;
  }
  void onDragEnd(const DragEndEvent &e) override {
    SvgKnob::onDragEnd(e);
    if(e.button==GLFW_MOUSE_BUTTON_LEFT) {
      if(module) {
        module->generatePartials();
        module->regenerate();
      }
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
template<typename T>
struct FadeTimeQuantity : Quantity {
  T* module;

  FadeTimeQuantity(T* m) : module(m) {}

  void setValue(float value) override {
    value = clamp(value, getMinValue(), getMaxValue());
    if (module) {
      module->setFadeTime(value);
    }
  }

  float getValue() override {
    if (module) {
      return module->getFadeTime();
    }
    return 0;
  }

  float getMinValue() override { return 0.0f; }
  float getMaxValue() override { return 10.0f; }
  float getDefaultValue() override { return 0.f; }
  //float getDisplayValue() override { return getValue() *100.f; }
  //void setDisplayValue(float displayValue) override { setValue(displayValue/100.f); }
  std::string getLabel() override { return "Fade Time"; }
  //std::string getUnit() override { return "%"; }
};
template<typename T>
struct FadeTimeSlider : ui::Slider {
  FadeTimeSlider(T* module) {
    quantity = new FadeTimeQuantity<T>(module);
    box.size.x = 200.0f;
  }
  virtual ~FadeTimeSlider() {
    delete quantity;
  }
};
template<typename T>
struct FadeTimeMenuItem : MenuItem {
  T* module;

  FadeTimeMenuItem(T* m) : module(m) {
    this->text = "Fade";
    this->rightText = "▸";
  }

  Menu* createChildMenu() override {
    Menu* menu = new Menu;
    menu->addChild(new FadeTimeSlider<T>(module));
    return menu;
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
    auto amp=createParam<UpdatePartialsKnob<Pad>>(mm2px(Vec(xpos,MHEIGHT-67.f)),module,Pad::AMP_PARAM);
    amp->module=module;
    addParam(amp);
    addInput(createInput<SmallPort>(mm2px(Vec(xpos,MHEIGHT-43.f)),module,Pad::VOCT_INPUT));
    addOutput(createOutput<SmallPort>(mm2px(Vec(xpos,MHEIGHT-31.f)),module,Pad::L_OUTPUT));
    addOutput(createOutput<SmallPort>(mm2px(Vec(xpos,MHEIGHT-19.f)),module,Pad::R_OUTPUT));
  }

  void appendContextMenu(Menu* menu) override {
    Pad *module=dynamic_cast<Pad *>(this->module);
    assert(module);

    menu->addChild(new MenuSeparator);

    menu->addChild(new FadeTimeMenuItem<Pad>(module));
  }

};


Model *modelPad=createModel<Pad,PadWidget>("Pad");