#include "dcb.h"

#define HISTORY_SIZE (1<<21)

template<typename M>
struct DelayChannel {
  M *module;
  dsp::DoubleRingBuffer<float,HISTORY_SIZE> buffer;
  dsp::DoubleRingBuffer<float,16> outBuffer;
  dsp::SampleRateConverter<1> src;
  int sendId,returnId;
  float fadeInFx=0.0f;
  float fadeOutDry=1.0f;
  const float fadeSpeed=0.001f;

  DelayChannel(M *_module,int _sendId,int _returnId) : module(_module),sendId(_sendId),returnId(_returnId) {
  }

  float process(float in,float delay,float mix,float sr) {
    float index=delay*sr;

    if(!buffer.full()) {
      buffer.push(in);
    }
    float consume=index-buffer.size();

    if(outBuffer.empty()) {
      double ratio=1.0;
      if(consume<=-16)
        ratio=0.5;
      else if(consume>=16)
        ratio=2.0;

      float inSR=sr;
      float outSR=ratio*inSR;

      int inFrames=fmin(buffer.size(),16);
      int outFrames=outBuffer.capacity();
      src.setRates(inSR,outSR);
      src.process((const dsp::Frame<1> *)buffer.startData(),&inFrames,(dsp::Frame<1> *)outBuffer.endData(),&outFrames);
      buffer.startIncr(inFrames);
      outBuffer.endIncr(outFrames);
    }

    float out;
    float wet=0.0f;
    if(!outBuffer.empty()) {
      wet=outBuffer.shift();
    }

    if(module->outputs[sendId].isConnected()&&module->inputs[returnId].isConnected()) {
      module->outputs[sendId].setVoltage(wet);
      wet=module->inputs[returnId].getVoltage();
    }
    out=crossfade(in,wet,mix);
    fadeInFx+=fadeSpeed;
    if(fadeInFx>1.0f) {
      fadeInFx=1.0f;
    }
    fadeOutDry-=fadeSpeed;
    if(fadeOutDry<0.0f) {
      fadeOutDry=0.0f;
    }
    return (in*fadeOutDry)+(out*fadeInFx);
  }

};

struct PPD : Module {
  enum ParamId {
    BPM_PARAM,NOTE_PARAM,PT_PARAM,FEEDBACK_PARAM,MIX_PARAM,PARAMS_LEN
  };
  enum InputId {
    INPUT,BPM_INPUT,FEEDBACK_INPUT,MIX_INPUT,RETURN_L_INPUT,RETURN_R_INPUT,INPUTS_LEN
  };
  enum OutputId {
    L_OUTPUT,R_OUTPUT,SEND_L_OUTPUT,SEND_R_OUTPUT,OUTPUTS_LEN
  };
  enum LightId {
    LIGHTS_LEN
  };
  float currentBpm=120.f;
  dsp::ClockDivider paramDivider;
  float msecs[6][3];
  float lastBpm=0;
  float wetR=0.f;
  DelayChannel<PPD> l{this,SEND_L_OUTPUT,RETURN_L_INPUT};
  DelayChannel<PPD> r{this,SEND_R_OUTPUT,RETURN_R_INPUT};


  PPD() {
    config(PARAMS_LEN,INPUTS_LEN,OUTPUTS_LEN,LIGHTS_LEN);
    configParam(BPM_PARAM,30,240,120,"BPM");
    configInput(BPM_INPUT,"BPM");
    configSwitch(NOTE_PARAM,0,5,3,"Note",{"1","1/2","1/4","1/8","1/16","1/32"});
    getParamQuantity(NOTE_PARAM)->snapEnabled=true;
    configSwitch(PT_PARAM,0,2,0,"PT",{"none","dotted","triplet"});
    configParam(FEEDBACK_PARAM,0,1,0.5f,"Feedback");
    configInput(FEEDBACK_INPUT,"Feedback");
    configParam(MIX_PARAM,0,1,0.5f,"Dry/Wet");
    configInput(MIX_INPUT,"Dry/Wet");
    configOutput(SEND_L_OUTPUT,"Left Send");
    configOutput(SEND_R_OUTPUT,"Right Send");
    configInput(RETURN_L_INPUT,"Left Return");
    configInput(RETURN_R_INPUT,"Right Return");
    configOutput(L_OUTPUT,"Left");
    configOutput(R_OUTPUT,"Right");
    configInput(INPUT,"IN");
    paramDivider.setDivision(16);
  }

  float getBpm() {
    return currentBpm;
  }

  void computeMsecs(float bpm) {
    float mSecsPerBeat=60000.f/bpm;
    float secsPerBeat=60/bpm;
    msecs[0][0]=msecs[0][1]=msecs[0][2]=secsPerBeat*4;
    msecs[1][1]=mSecsPerBeat*3;
    msecs[1][0]=mSecsPerBeat*2;
    msecs[1][2]=msecs[1][0]*0.666667f;
    msecs[2][1]=mSecsPerBeat*1.5f;
    msecs[2][0]=mSecsPerBeat;
    msecs[2][2]=msecs[1][0]/3.f;
    msecs[3][1]=mSecsPerBeat*0.75f;
    msecs[3][0]=mSecsPerBeat/2;
    msecs[3][2]=mSecsPerBeat/3;
    msecs[4][0]=mSecsPerBeat/4;
    msecs[4][1]=msecs[4][0]*1.5f;
    msecs[4][2]=mSecsPerBeat/6;
    msecs[5][0]=mSecsPerBeat/8;
    msecs[5][1]=msecs[4][0]*1.5f;
    msecs[5][2]=msecs[4][0]*0.666667f;
    lastBpm=bpm;
  }

  void process(const ProcessArgs &args) override {
    if(paramDivider.process()) {
      if(inputs[BPM_INPUT].isConnected()) {
        float freq=powf(2,clamp(inputs[BPM_INPUT].getVoltage(),-2.f,1.f));
        getParamQuantity(BPM_PARAM)->setImmediateValue(freq*120.f);
      }
      currentBpm=params[BPM_PARAM].getValue();
      if(inputs[FEEDBACK_INPUT].isConnected()) {
        getParamQuantity(FEEDBACK_PARAM)->setImmediateValue(inputs[FEEDBACK_INPUT].getVoltage()*0.1);
      }
      if(inputs[MIX_INPUT].isConnected()) {
        getParamQuantity(MIX_PARAM)->setImmediateValue(inputs[MIX_INPUT].getVoltage()*0.1);
      }
    }

    if(lastBpm!=currentBpm) {
      computeMsecs(currentBpm);
    }

    int noteIdx=params[NOTE_PARAM].getValue();
    int ptIdx=params[PT_PARAM].getValue();
    float rate=msecs[noteIdx][ptIdx];
    rate=rescale(rate,0.0f,10000.0f,0.0f,10.0f);
    float in=inputs[INPUT].getVoltage();
    float feedback=clamp(params[FEEDBACK_PARAM].getValue()+inputs[FEEDBACK_INPUT].getVoltage()/10.0f,0.0f,1.0f);
    float dry=in+(wetR*feedback);
    float delay=clamp(rate,0.0f,10.0f);
    float mix=params[MIX_PARAM].getValue();
    float wetL=l.process(dry,delay,mix,args.sampleRate);
    wetR=r.process(wetL,delay,mix,args.sampleRate);
    outputs[L_OUTPUT].setVoltage(wetL);
    outputs[R_OUTPUT].setVoltage(wetR);
  }
};

struct BpmDisplay : OpaqueWidget {
  std::basic_string<char> fontPath;
  PPD *module;

  BpmDisplay(PPD *_module) : OpaqueWidget(),module(_module) {
    fontPath=asset::plugin(pluginInstance,"res/FreeMonoBold.ttf");
  }

  void drawLayer(const DrawArgs &args,int layer) override {
    if(layer==1) {
      _draw(args);
    }
    Widget::drawLayer(args,layer);
  }

  void _draw(const DrawArgs &args) {
    std::string text="120.00";
    if(module) {
      text=rack::string::f("%3.2f",module->getBpm());
    }
    std::shared_ptr <Font> font=APP->window->loadFont(fontPath);
    nvgFillColor(args.vg,nvgRGB(255,255,128));
    nvgFontFaceId(args.vg,font->handle);
    nvgFontSize(args.vg,16);
    nvgTextAlign(args.vg,NVG_ALIGN_CENTER|NVG_ALIGN_MIDDLE);
    nvgText(args.vg,box.size.x/2,box.size.y/2,text.c_str(),nullptr);
  }
};

struct PPDWidget : ModuleWidget {
  PPDWidget(PPD *module) {
    setModule(module);
    setPanel(createPanel(asset::plugin(pluginInstance,"res/PPD.svg")));

    addChild(createWidget<ScrewSilver>(Vec(0,0)));
    addChild(createWidget<ScrewSilver>(Vec(box.size.x-15,0)));
    addChild(createWidget<ScrewSilver>(Vec(0,365)));
    addChild(createWidget<ScrewSilver>(Vec(box.size.x-15,365)));

    float x1=4;
    float x2=15.5;
    addParam(createParam<TrimbotWhite9>(mm2px(Vec(x1,8)),module,PPD::BPM_PARAM));
    addInput(createInput<SmallPort>(mm2px(Vec(x2,9)),module,PPD::BPM_INPUT));
    auto bpmDisplay=new BpmDisplay(module);
    bpmDisplay->box.pos=mm2px(Vec(4,20));
    bpmDisplay->box.size=mm2px(Vec(17,4));
    addChild(bpmDisplay);
    addParam(createParam<TrimbotWhite9>(mm2px(Vec(x1,32)),module,PPD::NOTE_PARAM));
    auto selectParam=createParam<SelectParam>(mm2px(Vec(x2,33)),module,PPD::PT_PARAM);
    selectParam->box.size=mm2px(Vec(7,9));
    selectParam->init({"nn","dot","trip"});
    addParam(selectParam);
    addParam(createParam<TrimbotWhite9>(mm2px(Vec(x1,48)),module,PPD::FEEDBACK_PARAM));
    addInput(createInput<SmallPort>(mm2px(Vec(x2,49)),module,PPD::FEEDBACK_INPUT));
    addParam(createParam<TrimbotWhite9>(mm2px(Vec(x1,64)),module,PPD::MIX_PARAM));
    addInput(createInput<SmallPort>(mm2px(Vec(x2,65)),module,PPD::MIX_INPUT));

    x1=3.5;
    float y=80;
    addInput(createInput<SmallPort>(mm2px(Vec(9.5,y)),module,PPD::INPUT));
    y+=12;
    addOutput(createOutput<SmallPort>(mm2px(Vec(x1,y)),module,PPD::SEND_L_OUTPUT));
    addOutput(createOutput<SmallPort>(mm2px(Vec(x2,y)),module,PPD::SEND_R_OUTPUT));
    y+=12;
    addInput(createInput<SmallPort>(mm2px(Vec(x1,y)),module,PPD::RETURN_L_INPUT));
    addInput(createInput<SmallPort>(mm2px(Vec(x2,y)),module,PPD::RETURN_R_INPUT));
    y+=12;
    addOutput(createOutput<SmallPort>(mm2px(Vec(x1,y)),module,PPD::L_OUTPUT));
    addOutput(createOutput<SmallPort>(mm2px(Vec(x2,y)),module,PPD::R_OUTPUT));

  }
};


Model *modelPPD=createModel<PPD,PPDWidget>("PPD");