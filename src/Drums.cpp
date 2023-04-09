#include "dcb.h"
#include "DrumSampler.hpp"
BINARY(src_data_bd_bd01_raw);
BINARY(src_data_bd_bd02_raw);
BINARY(src_data_bd_bd03_raw);
BINARY(src_data_bd_bd04_raw);
BINARY(src_data_bd_bd05_raw);
BINARY(src_data_bd_bd06_raw);
BINARY(src_data_bd_bd07_raw);
BINARY(src_data_bd_bd08_raw);
BINARY(src_data_bd_bd09_raw);
BINARY(src_data_bd_bd10_raw);
BINARY(src_data_bd_bd11_raw);
BINARY(src_data_bd_bd12_raw);
BINARY(src_data_bd_bd13_raw);
BINARY(src_data_bd_bd14_raw);
BINARY(src_data_bd_bd15_raw);
BINARY(src_data_bd_bd16_raw);
BINARY(src_data_sn_sn01_raw);
BINARY(src_data_sn_sn02_raw);
BINARY(src_data_sn_sn03_raw);
BINARY(src_data_sn_sn04_raw);
BINARY(src_data_sn_sn05_raw);
BINARY(src_data_sn_sn06_raw);
BINARY(src_data_sn_sn07_raw);
BINARY(src_data_sn_sn08_raw);
BINARY(src_data_sn_sn09_raw);
BINARY(src_data_sn_sn10_raw);
BINARY(src_data_sn_sn11_raw);
BINARY(src_data_sn_sn12_raw);
BINARY(src_data_sn_sn13_raw);
BINARY(src_data_sn_sn14_raw);
BINARY(src_data_sn_sn15_raw);
BINARY(src_data_sn_sn16_raw);
BINARY(src_data_cl_cl01_raw);
BINARY(src_data_cl_cl02_raw);
BINARY(src_data_cl_cl03_raw);
BINARY(src_data_cl_cl04_raw);
BINARY(src_data_cl_cl05_raw);
BINARY(src_data_cl_cl06_raw);
BINARY(src_data_cl_cl07_raw);
BINARY(src_data_cl_cl08_raw);
BINARY(src_data_cl_cl09_raw);
BINARY(src_data_cl_cl10_raw);
BINARY(src_data_cl_cl11_raw);
BINARY(src_data_cl_cl12_raw);
BINARY(src_data_cl_cl13_raw);
BINARY(src_data_cl_cl14_raw);
BINARY(src_data_cl_cl15_raw);
BINARY(src_data_cl_cl16_raw);
BINARY(src_data_oh_oh01_raw);
BINARY(src_data_oh_oh02_raw);
BINARY(src_data_oh_oh03_raw);
BINARY(src_data_oh_oh04_raw);
BINARY(src_data_oh_oh05_raw);
BINARY(src_data_oh_oh06_raw);
BINARY(src_data_oh_oh07_raw);
BINARY(src_data_oh_oh08_raw);
BINARY(src_data_oh_oh09_raw);
BINARY(src_data_oh_oh10_raw);
BINARY(src_data_oh_oh11_raw);
BINARY(src_data_oh_oh12_raw);
BINARY(src_data_oh_oh13_raw);
BINARY(src_data_oh_oh14_raw);
BINARY(src_data_oh_oh15_raw);
BINARY(src_data_oh_oh16_raw);
BINARY(src_data_perc_perc01_raw);
BINARY(src_data_perc_perc02_raw);
BINARY(src_data_perc_perc03_raw);
BINARY(src_data_perc_perc04_raw);
BINARY(src_data_perc_perc05_raw);
BINARY(src_data_perc_perc06_raw);
BINARY(src_data_perc_perc07_raw);
BINARY(src_data_perc_perc08_raw);
BINARY(src_data_perc_perc09_raw);
BINARY(src_data_perc_perc10_raw);
BINARY(src_data_perc_perc11_raw);
BINARY(src_data_perc_perc12_raw);
BINARY(src_data_perc_perc13_raw);
BINARY(src_data_perc_perc14_raw);
BINARY(src_data_perc_perc15_raw);
BINARY(src_data_perc_perc16_raw);
BINARY(src_data_clap_clap01_raw);
BINARY(src_data_clap_clap02_raw);
BINARY(src_data_clap_clap03_raw);
BINARY(src_data_clap_clap04_raw);
BINARY(src_data_clap_clap05_raw);
BINARY(src_data_clap_clap06_raw);
BINARY(src_data_clap_clap07_raw);
BINARY(src_data_clap_clap08_raw);
BINARY(src_data_clap_clap09_raw);
BINARY(src_data_clap_clap10_raw);
BINARY(src_data_clap_clap11_raw);
BINARY(src_data_clap_clap12_raw);
BINARY(src_data_clap_clap13_raw);
BINARY(src_data_clap_clap14_raw);
BINARY(src_data_clap_clap15_raw);
BINARY(src_data_clap_clap16_raw);
struct Drums : Module {
	enum ParamId {
		TYPE_PARAM,SMP_PARAM,PITCH_PARAM,DECAY_PARAM,DECAY_CV_PARAM,GAIN_PARAM,PARAMS_LEN
	};
	enum InputId {
		TRIG_INPUT,DECAY_INPUT,GAIN_INPUT,INPUTS_LEN
	};
	enum OutputId {
		OUTPUT,OUTPUTS_LEN
	};
	enum LightId {
		LIGHTS_LEN
	};
  SamplePlayer samplePlayers[6];
  dsp::SchmittTrigger sampleTrigger;
  const float attackTime = 1.5e-3;
  const float minDecayTime = 4.5e-3;
  const float maxDecayTime = 4.f;
  ADEnvelope env;

	Drums() {
		config(PARAMS_LEN, INPUTS_LEN, OUTPUTS_LEN, LIGHTS_LEN);
    configParam(SMP_PARAM,0,15,0,"Sample selection");
    getParamQuantity(SMP_PARAM)->snapEnabled=true;
    configSwitch(TYPE_PARAM,0,5,0,"Type",{"BD","SN","CL","OH","PERC","CLAP"});
    configParam(PITCH_PARAM,-1,1,0,"Pitch");
    configParam(DECAY_PARAM,0,1,1,"Decay");
    configParam(DECAY_CV_PARAM,0,1,0,"Decay CV");
    configParam(GAIN_PARAM,0,4,1,"Gain CV");
    configInput(DECAY_INPUT,"Decay");
    configInput(GAIN_INPUT,"Gain");
    configInput(TRIG_INPUT,"Trig");
    configOutput(OUTPUT,"CV");
    env.attackTime = attackTime;
    env.attackShape = 0.5f;
    env.decayShape = 2.0f;
    INFO("add bd");
    samplePlayers[0].addRawSample((float*)BINARY_START(src_data_bd_bd01_raw),BINARY_SIZE(src_data_bd_bd01_raw));
    samplePlayers[0].addRawSample((float*)BINARY_START(src_data_bd_bd02_raw),BINARY_SIZE(src_data_bd_bd02_raw));
    samplePlayers[0].addRawSample((float*)BINARY_START(src_data_bd_bd03_raw),BINARY_SIZE(src_data_bd_bd03_raw));
    samplePlayers[0].addRawSample((float*)BINARY_START(src_data_bd_bd04_raw),BINARY_SIZE(src_data_bd_bd04_raw));
    samplePlayers[0].addRawSample((float*)BINARY_START(src_data_bd_bd05_raw),BINARY_SIZE(src_data_bd_bd05_raw));
    samplePlayers[0].addRawSample((float*)BINARY_START(src_data_bd_bd06_raw),BINARY_SIZE(src_data_bd_bd06_raw));
    samplePlayers[0].addRawSample((float*)BINARY_START(src_data_bd_bd07_raw),BINARY_SIZE(src_data_bd_bd07_raw));
    samplePlayers[0].addRawSample((float*)BINARY_START(src_data_bd_bd08_raw),BINARY_SIZE(src_data_bd_bd08_raw));
    samplePlayers[0].addRawSample((float*)BINARY_START(src_data_bd_bd09_raw),BINARY_SIZE(src_data_bd_bd09_raw));
    samplePlayers[0].addRawSample((float*)BINARY_START(src_data_bd_bd10_raw),BINARY_SIZE(src_data_bd_bd10_raw));
    samplePlayers[0].addRawSample((float*)BINARY_START(src_data_bd_bd11_raw),BINARY_SIZE(src_data_bd_bd11_raw));
    samplePlayers[0].addRawSample((float*)BINARY_START(src_data_bd_bd12_raw),BINARY_SIZE(src_data_bd_bd12_raw));
    samplePlayers[0].addRawSample((float*)BINARY_START(src_data_bd_bd13_raw),BINARY_SIZE(src_data_bd_bd13_raw));
    samplePlayers[0].addRawSample((float*)BINARY_START(src_data_bd_bd14_raw),BINARY_SIZE(src_data_bd_bd14_raw));
    samplePlayers[0].addRawSample((float*)BINARY_START(src_data_bd_bd15_raw),BINARY_SIZE(src_data_bd_bd15_raw));
    samplePlayers[0].addRawSample((float*)BINARY_START(src_data_bd_bd16_raw),BINARY_SIZE(src_data_bd_bd16_raw));
    INFO("add sn");
    samplePlayers[1].addRawSample((float*)BINARY_START(src_data_sn_sn01_raw),BINARY_SIZE(src_data_sn_sn01_raw));
    samplePlayers[1].addRawSample((float*)BINARY_START(src_data_sn_sn02_raw),BINARY_SIZE(src_data_sn_sn02_raw));
    samplePlayers[1].addRawSample((float*)BINARY_START(src_data_sn_sn03_raw),BINARY_SIZE(src_data_sn_sn03_raw));
    samplePlayers[1].addRawSample((float*)BINARY_START(src_data_sn_sn04_raw),BINARY_SIZE(src_data_sn_sn04_raw));
    samplePlayers[1].addRawSample((float*)BINARY_START(src_data_sn_sn05_raw),BINARY_SIZE(src_data_sn_sn05_raw));
    samplePlayers[1].addRawSample((float*)BINARY_START(src_data_sn_sn06_raw),BINARY_SIZE(src_data_sn_sn06_raw));
    samplePlayers[1].addRawSample((float*)BINARY_START(src_data_sn_sn07_raw),BINARY_SIZE(src_data_sn_sn07_raw));
    samplePlayers[1].addRawSample((float*)BINARY_START(src_data_sn_sn08_raw),BINARY_SIZE(src_data_sn_sn08_raw));
    samplePlayers[1].addRawSample((float*)BINARY_START(src_data_sn_sn09_raw),BINARY_SIZE(src_data_sn_sn09_raw));
    samplePlayers[1].addRawSample((float*)BINARY_START(src_data_sn_sn10_raw),BINARY_SIZE(src_data_sn_sn10_raw));
    samplePlayers[1].addRawSample((float*)BINARY_START(src_data_sn_sn11_raw),BINARY_SIZE(src_data_sn_sn11_raw));
    samplePlayers[1].addRawSample((float*)BINARY_START(src_data_sn_sn12_raw),BINARY_SIZE(src_data_sn_sn12_raw));
    samplePlayers[1].addRawSample((float*)BINARY_START(src_data_sn_sn13_raw),BINARY_SIZE(src_data_sn_sn13_raw));
    samplePlayers[1].addRawSample((float*)BINARY_START(src_data_sn_sn14_raw),BINARY_SIZE(src_data_sn_sn14_raw));
    samplePlayers[1].addRawSample((float*)BINARY_START(src_data_sn_sn15_raw),BINARY_SIZE(src_data_sn_sn15_raw));
    samplePlayers[1].addRawSample((float*)BINARY_START(src_data_sn_sn16_raw),BINARY_SIZE(src_data_sn_sn16_raw));
    INFO("add cl");
    samplePlayers[2].addRawSample((float*)BINARY_START(src_data_cl_cl01_raw),BINARY_SIZE(src_data_cl_cl01_raw));
    samplePlayers[2].addRawSample((float*)BINARY_START(src_data_cl_cl02_raw),BINARY_SIZE(src_data_cl_cl02_raw));
    samplePlayers[2].addRawSample((float*)BINARY_START(src_data_cl_cl03_raw),BINARY_SIZE(src_data_cl_cl03_raw));
    samplePlayers[2].addRawSample((float*)BINARY_START(src_data_cl_cl04_raw),BINARY_SIZE(src_data_cl_cl04_raw));
    samplePlayers[2].addRawSample((float*)BINARY_START(src_data_cl_cl05_raw),BINARY_SIZE(src_data_cl_cl05_raw));
    samplePlayers[2].addRawSample((float*)BINARY_START(src_data_cl_cl06_raw),BINARY_SIZE(src_data_cl_cl06_raw));
    samplePlayers[2].addRawSample((float*)BINARY_START(src_data_cl_cl07_raw),BINARY_SIZE(src_data_cl_cl07_raw));
    samplePlayers[2].addRawSample((float*)BINARY_START(src_data_cl_cl08_raw),BINARY_SIZE(src_data_cl_cl08_raw));
    samplePlayers[2].addRawSample((float*)BINARY_START(src_data_cl_cl09_raw),BINARY_SIZE(src_data_cl_cl09_raw));
    samplePlayers[2].addRawSample((float*)BINARY_START(src_data_cl_cl10_raw),BINARY_SIZE(src_data_cl_cl10_raw));
    samplePlayers[2].addRawSample((float*)BINARY_START(src_data_cl_cl11_raw),BINARY_SIZE(src_data_cl_cl11_raw));
    samplePlayers[2].addRawSample((float*)BINARY_START(src_data_cl_cl12_raw),BINARY_SIZE(src_data_cl_cl12_raw));
    samplePlayers[2].addRawSample((float*)BINARY_START(src_data_cl_cl13_raw),BINARY_SIZE(src_data_cl_cl13_raw));
    samplePlayers[2].addRawSample((float*)BINARY_START(src_data_cl_cl14_raw),BINARY_SIZE(src_data_cl_cl14_raw));
    samplePlayers[2].addRawSample((float*)BINARY_START(src_data_cl_cl15_raw),BINARY_SIZE(src_data_cl_cl15_raw));
    samplePlayers[2].addRawSample((float*)BINARY_START(src_data_cl_cl16_raw),BINARY_SIZE(src_data_cl_cl16_raw));
    INFO("add oh");
    samplePlayers[3].addRawSample((float*)BINARY_START(src_data_oh_oh01_raw),BINARY_SIZE(src_data_oh_oh01_raw));
    samplePlayers[3].addRawSample((float*)BINARY_START(src_data_oh_oh02_raw),BINARY_SIZE(src_data_oh_oh02_raw));
    samplePlayers[3].addRawSample((float*)BINARY_START(src_data_oh_oh03_raw),BINARY_SIZE(src_data_oh_oh03_raw));
    samplePlayers[3].addRawSample((float*)BINARY_START(src_data_oh_oh04_raw),BINARY_SIZE(src_data_oh_oh04_raw));
    samplePlayers[3].addRawSample((float*)BINARY_START(src_data_oh_oh05_raw),BINARY_SIZE(src_data_oh_oh05_raw));
    samplePlayers[3].addRawSample((float*)BINARY_START(src_data_oh_oh06_raw),BINARY_SIZE(src_data_oh_oh06_raw));
    samplePlayers[3].addRawSample((float*)BINARY_START(src_data_oh_oh07_raw),BINARY_SIZE(src_data_oh_oh07_raw));
    samplePlayers[3].addRawSample((float*)BINARY_START(src_data_oh_oh08_raw),BINARY_SIZE(src_data_oh_oh08_raw));
    samplePlayers[3].addRawSample((float*)BINARY_START(src_data_oh_oh09_raw),BINARY_SIZE(src_data_oh_oh09_raw));
    samplePlayers[3].addRawSample((float*)BINARY_START(src_data_oh_oh10_raw),BINARY_SIZE(src_data_oh_oh10_raw));
    samplePlayers[3].addRawSample((float*)BINARY_START(src_data_oh_oh11_raw),BINARY_SIZE(src_data_oh_oh11_raw));
    samplePlayers[3].addRawSample((float*)BINARY_START(src_data_oh_oh12_raw),BINARY_SIZE(src_data_oh_oh12_raw));
    samplePlayers[3].addRawSample((float*)BINARY_START(src_data_oh_oh13_raw),BINARY_SIZE(src_data_oh_oh13_raw));
    samplePlayers[3].addRawSample((float*)BINARY_START(src_data_oh_oh14_raw),BINARY_SIZE(src_data_oh_oh14_raw));
    samplePlayers[3].addRawSample((float*)BINARY_START(src_data_oh_oh15_raw),BINARY_SIZE(src_data_oh_oh15_raw));
    samplePlayers[3].addRawSample((float*)BINARY_START(src_data_oh_oh16_raw),BINARY_SIZE(src_data_oh_oh16_raw));
    INFO("add perc");
    samplePlayers[4].addRawSample((float*)BINARY_START(src_data_perc_perc01_raw),BINARY_SIZE(src_data_perc_perc01_raw));
    samplePlayers[4].addRawSample((float*)BINARY_START(src_data_perc_perc02_raw),BINARY_SIZE(src_data_perc_perc02_raw));
    samplePlayers[4].addRawSample((float*)BINARY_START(src_data_perc_perc03_raw),BINARY_SIZE(src_data_perc_perc03_raw));
    samplePlayers[4].addRawSample((float*)BINARY_START(src_data_perc_perc04_raw),BINARY_SIZE(src_data_perc_perc04_raw));
    samplePlayers[4].addRawSample((float*)BINARY_START(src_data_perc_perc05_raw),BINARY_SIZE(src_data_perc_perc05_raw));
    samplePlayers[4].addRawSample((float*)BINARY_START(src_data_perc_perc06_raw),BINARY_SIZE(src_data_perc_perc06_raw));
    samplePlayers[4].addRawSample((float*)BINARY_START(src_data_perc_perc07_raw),BINARY_SIZE(src_data_perc_perc07_raw));
    samplePlayers[4].addRawSample((float*)BINARY_START(src_data_perc_perc08_raw),BINARY_SIZE(src_data_perc_perc08_raw));
    samplePlayers[4].addRawSample((float*)BINARY_START(src_data_perc_perc09_raw),BINARY_SIZE(src_data_perc_perc09_raw));
    samplePlayers[4].addRawSample((float*)BINARY_START(src_data_perc_perc10_raw),BINARY_SIZE(src_data_perc_perc10_raw));
    samplePlayers[4].addRawSample((float*)BINARY_START(src_data_perc_perc11_raw),BINARY_SIZE(src_data_perc_perc11_raw));
    samplePlayers[4].addRawSample((float*)BINARY_START(src_data_perc_perc12_raw),BINARY_SIZE(src_data_perc_perc12_raw));
    samplePlayers[4].addRawSample((float*)BINARY_START(src_data_perc_perc13_raw),BINARY_SIZE(src_data_perc_perc13_raw));
    samplePlayers[4].addRawSample((float*)BINARY_START(src_data_perc_perc14_raw),BINARY_SIZE(src_data_perc_perc14_raw));
    samplePlayers[4].addRawSample((float*)BINARY_START(src_data_perc_perc15_raw),BINARY_SIZE(src_data_perc_perc15_raw));
    samplePlayers[4].addRawSample((float*)BINARY_START(src_data_perc_perc16_raw),BINARY_SIZE(src_data_perc_perc16_raw));
    INFO("add clap");
    samplePlayers[5].addRawSample((float*)BINARY_START(src_data_clap_clap01_raw),BINARY_SIZE(src_data_clap_clap01_raw));
    samplePlayers[5].addRawSample((float*)BINARY_START(src_data_clap_clap02_raw),BINARY_SIZE(src_data_clap_clap02_raw));
    samplePlayers[5].addRawSample((float*)BINARY_START(src_data_clap_clap03_raw),BINARY_SIZE(src_data_clap_clap03_raw));
    samplePlayers[5].addRawSample((float*)BINARY_START(src_data_clap_clap04_raw),BINARY_SIZE(src_data_clap_clap04_raw));
    samplePlayers[5].addRawSample((float*)BINARY_START(src_data_clap_clap05_raw),BINARY_SIZE(src_data_clap_clap05_raw));
    samplePlayers[5].addRawSample((float*)BINARY_START(src_data_clap_clap06_raw),BINARY_SIZE(src_data_clap_clap06_raw));
    samplePlayers[5].addRawSample((float*)BINARY_START(src_data_clap_clap07_raw),BINARY_SIZE(src_data_clap_clap07_raw));
    samplePlayers[5].addRawSample((float*)BINARY_START(src_data_clap_clap08_raw),BINARY_SIZE(src_data_clap_clap08_raw));
    samplePlayers[5].addRawSample((float*)BINARY_START(src_data_clap_clap09_raw),BINARY_SIZE(src_data_clap_clap09_raw));
    samplePlayers[5].addRawSample((float*)BINARY_START(src_data_clap_clap10_raw),BINARY_SIZE(src_data_clap_clap10_raw));
    samplePlayers[5].addRawSample((float*)BINARY_START(src_data_clap_clap11_raw),BINARY_SIZE(src_data_clap_clap11_raw));
    samplePlayers[5].addRawSample((float*)BINARY_START(src_data_clap_clap12_raw),BINARY_SIZE(src_data_clap_clap12_raw));
    samplePlayers[5].addRawSample((float*)BINARY_START(src_data_clap_clap13_raw),BINARY_SIZE(src_data_clap_clap13_raw));
    samplePlayers[5].addRawSample((float*)BINARY_START(src_data_clap_clap14_raw),BINARY_SIZE(src_data_clap_clap14_raw));
    samplePlayers[5].addRawSample((float*)BINARY_START(src_data_clap_clap15_raw),BINARY_SIZE(src_data_clap_clap15_raw));
    samplePlayers[5].addRawSample((float*)BINARY_START(src_data_clap_clap16_raw),BINARY_SIZE(src_data_clap_clap16_raw));

	}
  float getPitch() {
    float voct=params[PITCH_PARAM].getValue();
    return voct;
  }

	void process(const ProcessArgs& args) override {
    int type=params[TYPE_PARAM].getValue();
    samplePlayers[type].setCurrentSample(params[SMP_PARAM].getValue());
    if(sampleTrigger.process(inputs[TRIG_INPUT].getVoltage())) {
      samplePlayers[type].trigger();
      env.trigger();
    }
    float out=samplePlayers[type].getOutput();
    float decay=params[DECAY_PARAM].getValue()+inputs[DECAY_INPUT].getVoltage()*params[DECAY_CV_PARAM].getValue()*0.1f;
    env.decayTime = rescale(std::pow(clamp(decay, 0.f, 1.0f), 2.f), 0.f, 1.f, minDecayTime, maxDecayTime);
    env.process(args.sampleTime);
    float gain=params[GAIN_PARAM].getValue();
    if(inputs[GAIN_INPUT].isConnected()) {
      gain*=(inputs[GAIN_INPUT].getVoltage()*0.1f);
    }
    out*=env.env*gain;
    outputs[OUTPUT].setVoltage(out);
    samplePlayers[type].step(args,getPitch());
  }

  int getType() {
    return (int)params[TYPE_PARAM].getValue();
  }

};

struct DrumTextWidget : Widget {
  Drums *module;
  std::basic_string<char> fontPath;
  std::vector<std::string> names={"BD","SN","CL","OH","PERC","CLAP"};
  DrumTextWidget(Drums *_module,Vec _pos,Vec _size) : module(_module)  {
    box.pos=_pos;
    box.size=_size;
    fontPath=asset::plugin(pluginInstance,"res/FreeMonoBold.ttf");
  }

  void drawLayer(const DrawArgs &args,int layer) override {
    if(layer==1) {
      _draw(args);
    }
    Widget::drawLayer(args,layer);
  }


  void _draw(const DrawArgs &args) {
    std::shared_ptr<Font> font=APP->window->loadFont(fontPath);
    std::string label=module?names[module->getType()]:"BD";
    nvgFillColor(args.vg,nvgRGB(255,255,128));
    nvgFontFaceId(args.vg,font->handle);
    nvgFontSize(args.vg,10);
    nvgTextAlign(args.vg,NVG_ALIGN_CENTER|NVG_ALIGN_MIDDLE);
    nvgText(args.vg,box.size.x/2,box.size.y/2,label.c_str(),NULL);
  }
};

struct DrumsWidget : ModuleWidget {
	DrumsWidget(Drums* module) {
		setModule(module);
		setPanel(createPanel(asset::plugin(pluginInstance, "res/Drums.svg")));

    if(module) {
      auto titleWidget=new DrumTextWidget(module,mm2px(Vec(1,8)),mm2px(Vec(9,3)));
      addChild(titleWidget);
    }

    float x=1.9;
    float y=20;
    addParam(createParam<TrimbotWhite>(mm2px(Vec(x,y)),module,Drums::TYPE_PARAM));
    y+=12;
    addParam(createParam<TrimbotWhite>(mm2px(Vec(x,y)),module,Drums::SMP_PARAM));
    y+=12;
    addParam(createParam<TrimbotWhite>(mm2px(Vec(x,y)),module,Drums::PITCH_PARAM));
    y+=12;
    addParam(createParam<TrimbotWhite>(mm2px(Vec(x,y)),module,Drums::DECAY_PARAM));
    y+=8;
    addInput(createInput<SmallPort>(mm2px(Vec(x,y)),module,Drums::DECAY_INPUT));
    y+=8;
    addParam(createParam<TrimbotWhite>(mm2px(Vec(x,y)),module,Drums::DECAY_CV_PARAM));
    y+=12;
    addParam(createParam<TrimbotWhite>(mm2px(Vec(x,y)),module,Drums::GAIN_PARAM));
    y+=8;
    addInput(createInput<SmallPort>(mm2px(Vec(x,y)),module,Drums::GAIN_INPUT));
    y=104;
    addInput(createInput<SmallPort>(mm2px(Vec(x,y)),module,Drums::TRIG_INPUT));
    y+=12;
    addOutput(createOutput<SmallPort>(mm2px(Vec(x,y)),module,Drums::OUTPUT));
  }
};


Model* modelDrums = createModel<Drums, DrumsWidget>("Drums");