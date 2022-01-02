#include "dcb.h"

#define ROWS 12



struct Interface : Module {
  enum ParamId {
    PARAMS_LEN
  };
  enum InputId {
    INS,
    INPUTS_LEN = INS + ROWS
  };
  enum OutputId {
    OUTS,
    OUTPUTS_LEN = OUTS + ROWS
  };
  enum LightId {
    LIGHTS_LEN
  };

  std::string labels[ROWS];
  bool dirty[ROWS] = {};
  Interface() {
    config(PARAMS_LEN,INPUTS_LEN,OUTPUTS_LEN,LIGHTS_LEN);
  }

  void process(const ProcessArgs &args) override {
    for (int i = 0; i < ROWS; i++) {
      if (inputs[i].isConnected()) {
        int channels = std::min(inputs[i].getChannels(),1);
        for(int k=0;k<channels;k++) {
          outputs[i].setVoltage(inputs[i].getVoltage(k),k);
        }
        outputs[i].setChannels(channels);
      }
    }
  }

  json_t *dataToJson() override {
    json_t *data=json_object();
    json_t *labelList=json_array();
    for(int k=0;k<ROWS;k++) {
      json_array_append_new(labelList,json_string(labels[k].c_str()));
    }
    json_object_set_new(data,"labels",labelList);
    return data;
  }

  void dataFromJson(json_t *rootJ) override {
    json_t *data=json_object_get(rootJ,"labels");
    for(int k=0;k<ROWS;k++) {
      json_t *hexStr=json_array_get(data,k);
      labels[k]=json_string_value(hexStr);
      dirty[k] = true;
    }

  }

};

/***
 * An editable Textfield
 * some stuff i learned from https://github.com/mgunyho/Little-Utils
 */
struct LabelField : TextField {
  unsigned maxTextLength=16;
  bool isFocused=false;
  std::string _fontPath;
  float font_size;
  float letter_spacing;
  Vec textOffset;
  NVGcolor defaultTextColor;
  NVGcolor textColor; // This can be used to temporarily override text color
  NVGcolor backgroundColor;
  Interface *module;

  LabelField() : TextField() {
    _fontPath = asset::plugin(pluginInstance,"res/FreeMonoBold.ttf");

    defaultTextColor=nvgRGB(0x20,0x44,0x20);
    textColor=defaultTextColor;
    backgroundColor=nvgRGB(0xcc,0xcc,0xcc);
    box.size=mm2px(Vec(45.5,6.f));
    font_size=14;
    letter_spacing=0.f;
    textOffset=Vec(2.f,2.f);
  }

  int nr;

  void setNr(int k) {
    nr=k;
  }


  void onSelectText(const event::SelectText &e) override {
    //printf("on select text %d %d %d\n",e.codepoint, cursor, selection);
    if(TextField::text.size()<maxTextLength||cursor!=selection) {
      TextField::onSelectText(e);
    } else {
      e.consume(NULL);
    }
  }

  void onSelectKey(const event::SelectKey &e) override {
    //if(!(e.action == GLFW_REPEAT)) printf("on select key\n");
    bool act=e.action==GLFW_PRESS||e.action==GLFW_REPEAT;
    if(act&&e.key==GLFW_KEY_V&&(e.mods&RACK_MOD_MASK)==RACK_MOD_CTRL) {
      // prevent pasting too long text
      unsigned int pasteLength=maxTextLength-TextField::text.size()+abs(selection-cursor);
      if(pasteLength>0) {
        std::string newText(glfwGetClipboardString(APP->window->win));
        if(newText.size()>pasteLength)
          newText.erase(pasteLength);
        insertText(newText);
      }

    } else if(act&&(e.mods&RACK_MOD_MASK)==GLFW_MOD_SHIFT&&e.key==GLFW_KEY_HOME) {
      cursor=0;
    } else if(act&&(e.mods&RACK_MOD_MASK)==GLFW_MOD_SHIFT&&e.key==GLFW_KEY_END) {
      cursor=TextField::text.size();
    } else if(act&&e.key==GLFW_KEY_ESCAPE) {
      event::Deselect eDeselect;
      onDeselect(eDeselect);
      APP->event->selectedWidget=NULL;
    } else {
      TextField::onSelectKey(e);
    }
    if(!e.isConsumed())
      e.consume(dynamic_cast<TextField *>(this));
  }
  int getTextPosition(Vec mousePos) override {
    float pct = mousePos.x / box.size.x;
    return std::min((int)(pct*16.5f), (int)text.size());
  }
  void onButton(const event::Button &e) override {
    TextField::onButton(e);
  }

  void onAction(const event::Action &e) override {
    event::Deselect eDeselect;
    onDeselect(eDeselect);
    APP->event->selectedWidget=NULL;
    e.consume(NULL);
  }

  void onHover(const event::Hover &e) override {
    TextField::onHover(e);
  }

  void onHoverScroll(const event::HoverScroll &e) override {
    TextField::onHoverScroll(e);
  }

  virtual void onChange(const event::Change &e) override {
    module->labels[nr] = text;
  }

  void onSelect(const event::Select &e) override {
    isFocused=true;
    e.consume(dynamic_cast<TextField *>(this));
  }

  void onDeselect(const event::Deselect &e) override {
    isFocused=false;
    e.consume(NULL);
  }

  void step() override {
    TextField::step();
  }

  void draw(const DrawArgs &args) override {
    std::shared_ptr <Font> font= APP->window->loadFont(_fontPath);
    if(module && module->dirty[nr]) {
      text = module->labels[nr];
      module->dirty[nr] = false;
    }
    auto vg=args.vg;
    nvgScissor(vg,0,0,box.size.x,box.size.y);

    nvgBeginPath(vg);
    nvgRoundedRect(vg,0,0,box.size.x,box.size.y,3.0);
    nvgFillColor(vg,backgroundColor);
    nvgFill(vg);

    if(font->handle>=0) {

      nvgFillColor(vg,textColor);
      nvgFontFaceId(vg,font->handle);
      nvgFontSize(vg,font_size);
      nvgTextLetterSpacing(vg,letter_spacing);
      nvgTextAlign(vg,NVG_ALIGN_LEFT|NVG_ALIGN_TOP);
      nvgText(vg,textOffset.x,textOffset.y,text.c_str(),NULL);
    }
    if(isFocused) {
      NVGcolor highlightColor=nvgRGB(0x0,0x90,0xd8);
      highlightColor.a=0.5;

      int begin=std::min(cursor,selection);
      int end=std::max(cursor,selection);
      int len=end-begin;

      float char_width=16.25f;
      nvgBeginPath(vg);
      nvgFillColor(vg,highlightColor);

      //nvgRect(vg,textOffset.x+begin*.5f*char_width-1,textOffset.y,(len>0?char_width*len*0.5f:1)+1,box.size.y*0.8f);
      nvgRect(vg,textOffset.x+begin*.5f*char_width-1,textOffset.y,(len>0?char_width*len*0.5f:1)+1,box.size.y*0.8f);
      nvgFill(vg);

    }
    nvgResetScissor(vg);

  }

};

struct InterfaceWidget : ModuleWidget {
  InterfaceWidget(Interface *module) {
    setModule(module);
    setPanel(createPanel(asset::plugin(pluginInstance,"res/Interface.svg")));

    addChild(createWidget<ScrewSilver>(Vec(RACK_GRID_WIDTH,0)));
    addChild(createWidget<ScrewSilver>(Vec(box.size.x-2*RACK_GRID_WIDTH,0)));
    addChild(createWidget<ScrewSilver>(Vec(RACK_GRID_WIDTH,RACK_GRID_HEIGHT-RACK_GRID_WIDTH)));
    addChild(createWidget<ScrewSilver>(Vec(box.size.x-2*RACK_GRID_WIDTH,RACK_GRID_HEIGHT-RACK_GRID_WIDTH)));

    for(int k=0;k<ROWS;k++) {
      auto *textField=createWidget<LabelField>(mm2px(Vec(7.5f,MHEIGHT-((k<6?108.5f:105.5f)-((float)k*8.3f)))));
      textField->nr=k;
      textField->multiline=false;
      textField->module = module;
      addChild(textField);
      if(k<6) {
        addInput(createInput<SmallPort>(mm2px(Vec(53.5f,MHEIGHT-(108.5f-(k*8.3f)))),module,Interface::INS+k));
        addOutput(createOutput<SmallPort>(mm2px(Vec(1.f,MHEIGHT-(108.5f-(k*8.3f)))),module,Interface::OUTS+k));
      } else {
        addInput(createInput<SmallPort>(mm2px(Vec(1.f,MHEIGHT-(105.5f-(k*8.3f)))),module,Interface::INS+k));
        addOutput(createOutput<SmallPort>(mm2px(Vec(53.5f,MHEIGHT-(105.5f-(k*8.3f)))),module,Interface::OUTS+k));
      }
    }
    // mm2px(Vec(15.24, 72.0))
    addChild(createWidget<Widget>(mm2px(Vec(-0.0,14.585))));
  }
};


Model *modelInterface=createModel<Interface,InterfaceWidget>("Interface");