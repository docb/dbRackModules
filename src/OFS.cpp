#include "dcb.h"
#include "OFS.hpp"

struct OFS : Module {
	enum ParamId {
		OFFSET_PARAM,OFFSET_CV_PARAM,SCALE_PARAM,SCALE_CV_PARAM,PARAMS_LEN
	};
	enum InputId {
		CV_INPUT,OFFSET_INPUT,SCALE_INPUT, CLK_INPUT, SCL_INPUT,INPUTS_LEN
	};
	enum OutputId {
		CV_OUTPUT,TRIG_OUTPUT,OUTPUTS_LEN
	};
	enum LightId {
		LIGHTS_LEN
	};
  bool offsetThenScale=false;
	float_4 hold[4];
	float_4 offsetHold[4];
	float_4 scaleHold[4];
  float_4 last[4];
	dsp::TSchmittTrigger<float_4> clkTrigger;
	OFS() {
		config(PARAMS_LEN, INPUTS_LEN, OUTPUTS_LEN, LIGHTS_LEN);
    configParam(OFFSET_PARAM,-10,10,0,"Offset");
    configParam(SCALE_PARAM,-10,10,1,"Scale");
    configParam(OFFSET_CV_PARAM,0,1,0,"Offset CV"," %",0,100);
    configParam(SCALE_CV_PARAM,0,1,0,"Scale CV"," %",0,100);
    configInput(CV_INPUT,"CV");
    configInput(SCALE_INPUT,"Scale");
    configInput(OFFSET_INPUT,"Offset");
    configInput(SCL_INPUT,"QNT");
    configOutput(CV_OUTPUT,"CV");
		configOutput(TRIG_OUTPUT,"Trg");

	}

	void process(const ProcessArgs& args) override {
		if(outputs[CV_OUTPUT].isConnected()) {
			if(inputs[CV_INPUT].isConnected()) {
				bool sample=inputs[CLK_INPUT].isConnected();
				int channels=inputs[CV_INPUT].getChannels();
				for(int c=0;c<channels;c+=4) {
					float_4 in=inputs[CV_INPUT].getVoltageSimd<float_4>(c);
					float_4 offset=params[OFFSET_PARAM].getValue()+inputs[OFFSET_INPUT].getPolyVoltageSimd<float_4>(c)*params[OFFSET_CV_PARAM].getValue();
					float_4 scale=params[SCALE_PARAM].getValue()+inputs[SCALE_INPUT].getPolyVoltageSimd<float_4>(c)*params[SCALE_CV_PARAM].getValue();
					if(sample) {
						float_4 triggered = clkTrigger.process(inputs[CLK_INPUT].getPolyVoltageSimd<float_4>(c));
						hold[c]=simd::ifelse(triggered,in,hold[c]);
						offsetHold[c]=simd::ifelse(triggered,offset,offsetHold[c]);
						scaleHold[c]=simd::ifelse(triggered,scale,scaleHold[c]);
					} else {
						hold[c]=in;
						offsetHold[c]=offset;
						scaleHold[c]=scale;
					}
					float filteredScale[16];
					int N = 0;
					if(inputs[SCL_INPUT].isConnected()) {
						int scaleChannels = inputs[SCL_INPUT].getChannels();
						float scaleBuf[16];
						for (int i = 0; i < scaleChannels; ++i) {
							scaleBuf[i] = inputs[SCL_INPUT].getPolyVoltage(i);
						}

						// Sorting max 16 floats takes mere nanoseconds
						std::sort(scaleBuf, scaleBuf + scaleChannels);

						// 3. FILTER THE SCALE (Within 1 Octave of Base Note)
						float baseVolt = scaleBuf[0];
						float baseOctave = std::floor(baseVolt);

						for (int i = 0; i < scaleChannels; ++i) {
							// Take all inputs smaller than lowest voltage + 1.0V
							if (scaleBuf[i] < baseVolt + 1.0f) {
								// Store the relative pitch of the note (strips the absolute octave)
								filteredScale[N] = scaleBuf[i] - baseOctave;
								N++;
							}
						}
					}
					float_4 out=offsetThenScale?(hold[c]+offsetHold[c])*scaleHold[c]:hold[c]*scaleHold[c]+offsetHold[c];
					if(N>0) {
						float_4 octave = simd::floor(out);
						float_4 fraction = out - octave;
						float_4 k_f = fraction * static_cast<float>(N);
						k_f = rack::simd::clamp(k_f, 0.0f, static_cast<float>(N - 1));
						simd::int32_4 k_i = simd::int32_4(k_f);
						int32_t k_arr[4];
						k_i.store(k_arr);

						// 3. Fast scalar gather (array lookup)
						float lookup_arr[4] = {
							filteredScale[k_arr[0]],
							filteredScale[k_arr[1]],
							filteredScale[k_arr[2]],
							filteredScale[k_arr[3]]
					  };

						float_4 scaleValues = float_4::load(lookup_arr);
						out = octave + scaleValues;
					}
					outputs[CV_OUTPUT].setVoltageSimd(out,c);
					float_4 mask=out != last[c];
					outputs[TRIG_OUTPUT].setVoltageSimd(simd::ifelse(mask,10.f,0.f),c);
					last[c]=out;
				}
				outputs[CV_OUTPUT].setChannels(channels);
				outputs[TRIG_OUTPUT].setChannels(channels);
			} else {
				outputs[CV_OUTPUT].setVoltage(params[OFFSET_PARAM].getValue()+inputs[OFFSET_INPUT].getVoltage()*params[OFFSET_CV_PARAM].getValue());
				outputs[CV_OUTPUT].setChannels(1);
			}
		}
	}
  void dataFromJson(json_t *root) override {
    json_t *jOffsetThenScale=json_object_get(root,"offsetThenScale");
    if(jOffsetThenScale) {
      offsetThenScale=json_boolean_value(jOffsetThenScale);
    }
  }

  json_t *dataToJson() override {
    json_t *root=json_object();
    json_object_set_new(root,"offsetThenScale",json_boolean(offsetThenScale));
    return root;
  }
};


struct OFSWidget : ModuleWidget {
	OFSWidget(OFS* module) {
		setModule(module);
		setPanel(createPanel(asset::plugin(pluginInstance, "res/OFS.svg")));
    float y=10;
    float x=1.9;
    addParam(createParam<TrimbotWhite>(mm2px(Vec(x,y)),module,OFS::OFFSET_PARAM));
    y+=8;
    addInput(createInput<SmallPort>(mm2px(Vec(x,y)),module,OFS::OFFSET_INPUT));
    y+=8;
    addParam(createParam<TrimbotWhite>(mm2px(Vec(x,y)),module,OFS::OFFSET_CV_PARAM));
    y=40;
    addParam(createParam<TrimbotWhite>(mm2px(Vec(x,y)),module,OFS::SCALE_PARAM));
    y+=8;
    addInput(createInput<SmallPort>(mm2px(Vec(x,y)),module,OFS::SCALE_INPUT));
    y+=8;
    addParam(createParam<TrimbotWhite>(mm2px(Vec(x,y)),module,OFS::SCALE_CV_PARAM));
		addInput(createInput<SmallPort>(mm2px(Vec(x,68)),module,OFS::CLK_INPUT));

		addInput(createInput<SmallPort>(mm2px(Vec(x,80)),module,OFS::SCL_INPUT));
    y=92;
		addOutput(createOutput<SmallPort>(mm2px(Vec(x,y)),module,OFS::TRIG_OUTPUT));
		y+=12;
		addInput(createInput<SmallPort>(mm2px(Vec(x,y)),module,OFS::CV_INPUT));
    y+=12;
    addOutput(createOutput<SmallPort>(mm2px(Vec(x,y)),module,OFS::CV_OUTPUT));
	}
  void appendContextMenu(Menu* menu) override {
    OFS *module=dynamic_cast<OFS *>(this->module);
    assert(module);

    menu->addChild(new MenuSeparator);

    menu->addChild(createBoolPtrMenuItem("Offset Then Scale","",&module->offsetThenScale));
  }
};


Model* modelOFS = createModel<OFS, OFSWidget>("OFS");