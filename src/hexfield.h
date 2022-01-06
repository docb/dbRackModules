//
// Created by docb on 11/9/21.
//

#ifndef DOCB_VCV_HEXFIELD_H
#define DOCB_VCV_HEXFIELD_H
#include "rnd.h"
#include "textfield.hpp"
#include <bitset>

template<typename M,typename W>
struct HexField;

template<typename M,typename W>
struct DensQuantity : Quantity {
  HexField<M,W>* hexField;

  DensQuantity(HexField<M,W>* m) : hexField(m) {}

  void setValue(float value) override {
    value = clamp(value, getMinValue(), getMaxValue());
    if (hexField) {
      hexField->dens = value;
    }
  }

  float getValue() override {
    if (hexField) {
      return hexField->dens;
    }
    return 0.5f;
  }

  float getMinValue() override { return 0.0f; }
  float getMaxValue() override { return 1.0f; }
  float getDefaultValue() override { return 0.5f; }
  float getDisplayValue() override { return getValue() *100.f; }
  void setDisplayValue(float displayValue) override { setValue(displayValue/100.f); }
  std::string getLabel() override { return "Random density"; }
  std::string getUnit() override { return "%"; }
};
template<typename M,typename W>
struct DensSlider : ui::Slider {
  DensSlider(HexField<M,W>* hexField) {
    quantity = new DensQuantity<M,W>(hexField);
    box.size.x = 200.0f;
  }
  virtual ~DensSlider() {
    delete quantity;
  }
};
template<typename M,typename W>
struct DensMenuItem : MenuItem {
  HexField<M,W>* hexField;

  DensMenuItem(HexField<M,W>* hexField1) : hexField(hexField1) {
    this->text = "Random";
    this->rightText = "▸";
  }

  Menu* createChildMenu() override {
    Menu* menu = new Menu;
    menu->addChild(new DensSlider<M,W>(hexField));
    return menu;
  }
};

/***
 * An editable Textfield which only accepts hex strings
 * some stuff i learned from https://github.com/mgunyho/Little-Utils
 */
template<typename M,typename W>
struct HexField : MTextField {
  unsigned maxTextLength=16;
  bool isFocused=false;
  std::basic_string<char> fontPath;
  float font_size;
  float charWidth,charWidth2;
  float letter_spacing;
  Vec textOffset;
  NVGcolor defaultTextColor;
  NVGcolor textColor; // This can be used to temporarily override text color
  NVGcolor backgroundColor;
  NVGcolor backgroundColorFocus;
  NVGcolor dirtyColor;
  M *module;
  W *moduleWidget;
  bool dirty=false;
  bool rndInit = false;
  RND rnd;
  float dens;

  HexField() : MTextField() {
    fontPath = asset::plugin(pluginInstance,"res/FreeMonoBold.ttf");
    defaultTextColor=nvgRGB(0x20,0x44,0x20);
    dirtyColor=nvgRGB(0xAA,0x20,0x20);
    textColor=defaultTextColor;
    backgroundColor=nvgRGB(0xcc,0xcc,0xcc);
    backgroundColorFocus=nvgRGB(0xcc,0xff,0xcc);
    box.size=mm2px(Vec(45.5,6.f));
    font_size=14;
    charWidth = 16.5f;
    charWidth2 = 16.25f;
    letter_spacing=0.f;
    textOffset=Vec(2.f,2.f);
    dens = 0.2f;
  }

  int nr;
  bool onWidgetSelect = false;

  void setNr(int k) {
    nr=k;
  }

  void setModule(M *_module) {
    module=_module;
  }
  void setModuleWidget(W *_moduleWidget) {
    moduleWidget=_moduleWidget;
  }

  static bool checkText(std::string newText) {
    std::string::iterator it;
    for(it=newText.begin();it!=newText.end();++it) {
      char c=*it;
      bool ret=(c>=48&&c<58)||(c>=65&&c<=70)||(c>=97&&c<=102);
      if(!ret)
        return false;
    }
    return true;
  }

  static bool checkChar(int c) {
    return (c>=48&&c<58)||(c>=65&&c<=70)||(c>=97&&c<=102);
  }

  void onSelectText(const event::SelectText &e) override {
    if(onWidgetSelect) {
      onWidgetSelect = false;
      e.consume(nullptr);
    } else {
      INFO("on select text %d %d %d\n",e.codepoint,cursor,selection);
      if((MTextField::text.size()<maxTextLength||cursor!=selection)&&checkChar(e.codepoint)) {
        MTextField::onSelectText(e);
      } else {
        e.consume(nullptr);
      }
    }
  }

  unsigned long long toLong() {
    unsigned long long x;
    std::stringstream ss;
    ss << std::hex << text;
    ss >> x;
    return x;
  }

  void pasteCheckedString() {
    unsigned int pasteLength=maxTextLength-MTextField::text.size()+abs(selection-cursor);
    if(pasteLength>0) {
      std::string newText(glfwGetClipboardString(APP->window->win));
      if(checkText(newText)) {
        if(newText.size()>pasteLength)
          newText.erase(pasteLength);
        insertText(newText);
        if(isFocused) {
          dirty=true;
        } else {
          module->setHex(nr,text);
          dirty=false;
        }
      }
    }
  }
  void pasteClipboard() override {
    pasteCheckedString();
  }
  void onSelectKey(const event::SelectKey &e) override {
    bool act=e.action==GLFW_PRESS||e.action==GLFW_REPEAT;
    if(act&&(e.keyName == "v" && (e.mods & RACK_MOD_MASK) == RACK_MOD_CTRL)) {
      pasteCheckedString();
    } else if(act&&(e.keyName=="p")) {
      if(!rndInit) {
        rnd.reset(0);
        rndInit = true;
      }
      unsigned long val;
      std::stringstream rstream;
      for(int k=0;k<32;k++) {
        if(rnd.nextCoin(1-dens)) {
          rstream << "1";
        } else {
          rstream << "0";
        }
      }
      val = std::bitset<32>(rstream.str()).to_ulong();
      //unsigned long val = rnd.next();
      std::stringstream stream;
      stream<<std::uppercase<<std::setfill('0')<<std::setw(8)<<std::hex<<(val&0xFFFFFFFF);
      setText(stream.str());
      if((e.mods&RACK_MOD_MASK)==GLFW_MOD_SHIFT) {
        module->setHex(nr,text);
        dirty=false;
      }
    } else if(act&&(e.keyName=="r")) {
      unsigned long long x = toLong();
      unsigned long long mask = ((1ULL&x) << (text.size()*4-1));
      x >>= 1; x|=mask;
      std::stringstream stream;
      stream<<std::uppercase<<std::setfill('0')<<std::setw(text.size())<<std::hex<<(x);
      setText(stream.str());
      if((e.mods&RACK_MOD_MASK)==GLFW_MOD_SHIFT) {
        module->setHex(nr,text);
        dirty=false;
      }
    } else if(act&&(e.keyName=="l")) {
      unsigned long long x = toLong();
      int ps = text.size()*4-1;
      unsigned long long mask = (x&(1<<ps)) >> ps;
      long long m = text.size()==16?-1:((1LL<<text.size()*4)-1LL);
      auto um = (unsigned long long)m;
      x = (x << 1); x|=mask; x&=um;
      std::stringstream stream;
      stream<<std::uppercase<<std::setfill('0')<<std::setw(text.size())<<std::hex<<(x);
      std::string str = stream.str().substr(0,text.size());
      setText(str);
      if((e.mods&RACK_MOD_MASK)==GLFW_MOD_SHIFT) {
        module->setHex(nr,text);
        dirty=false;
      }
    } else if(act&&(e.mods&RACK_MOD_MASK)==GLFW_MOD_SHIFT&&e.key==GLFW_KEY_HOME) {
      cursor=0;
    } else if(act&&(e.mods&RACK_MOD_MASK)==GLFW_MOD_SHIFT&&e.key==GLFW_KEY_END) {
      cursor=MTextField::text.size();
    } else if(act&&(e.key==GLFW_KEY_DOWN)) {
      module->setHex(nr,text);
      dirty=false;
      moduleWidget->moveFocusDown(nr);
    } else if(act&&(e.key==GLFW_KEY_TAB)) {
      moduleWidget->moveFocusDown(nr);
    } else if(act&&(e.key==GLFW_KEY_UP)) {
      module->setHex(nr,text);
      dirty=false;
      moduleWidget->moveFocusUp(nr);
    } else if(act&&(e.mods&RACK_MOD_MASK)==GLFW_MOD_SHIFT&&e.key==GLFW_KEY_TAB) {
      moduleWidget->moveFocusUp(nr);
    } else if(act&&e.key==GLFW_KEY_ESCAPE) {
      event::Deselect eDeselect;
      onDeselect(eDeselect);
      APP->event->selectedWidget=nullptr;
      setText(module->getHex(nr));
      dirty=false;
    } else {
      MTextField::onSelectKey(e);
    }
    if(!e.isConsumed())
      e.consume(dynamic_cast<MTextField *>(this));
  }

  virtual void onChange(const event::Change &e) override {
    dirty=true;
  }

  int getTextPosition(Vec mousePos) override {
    float pct = mousePos.x / box.size.x;
    return std::min((int)(pct*charWidth), (int)text.size());
  }

  void onButton(const event::Button &e) override {
    MTextField::onButton(e);
    INFO("%d %d %f %f %f %f\n", cursor, selection, e.pos.x,e.pos.y,box.size.x,box.size.y);
  }

  void onAction(const event::Action &e) override {
    event::Deselect eDeselect;
    onDeselect(eDeselect);
    APP->event->selectedWidget=NULL;
    e.consume(NULL);
    module->setHex(nr,text);
    dirty=false;
  }

  void onHover(const event::Hover &e) override {
    MTextField::onHover(e);
  }

  void onHoverScroll(const event::HoverScroll &e) override {
    MTextField::onHoverScroll(e);
  }

  void onSelect(const event::Select &e) override {
    isFocused=true;
    e.consume(dynamic_cast<MTextField *>(this));
  }

  void onDeselect(const event::Deselect &e) override {
    isFocused=false;
    e.consume(NULL);
  }

  void step() override {
    MTextField::step();
  }

  void draw(const DrawArgs &args) override {
    std::shared_ptr <Font> font = APP->window->loadFont(fontPath);
    if(module && module->isDirty(nr)) {
      text = module->getHex(nr);
      module->setDirty(nr,false);
    }
    auto vg=args.vg;
    nvgScissor(vg,0,0,box.size.x,box.size.y);

    nvgBeginPath(vg);
    nvgRoundedRect(vg,0,0,box.size.x,box.size.y,3.0);
    nvgFillColor(vg,isFocused?backgroundColorFocus:backgroundColor);
    nvgFill(vg);

    if(font->handle>=0) {

      nvgFillColor(vg,dirty?dirtyColor:textColor);
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

      nvgBeginPath(vg);
      nvgFillColor(vg,highlightColor);

      nvgRect(vg,textOffset.x+begin*.5f*charWidth2-1,textOffset.y,(len>0?charWidth2*len*0.5f:1)+1,box.size.y*0.8f);
      nvgFill(vg);

    }
    nvgResetScissor(vg);

  }

  void createContextMenu() override {
    MTextField::createContextMenu();
    menu->addChild(new DensMenuItem<M,W>(this));
  }
  void copyClipboard() override {
    if (cursor == selection && isFocused)
      return;
    if(!isFocused)
      glfwSetClipboardString(APP->window->win, getText().c_str());
    else
      glfwSetClipboardString(APP->window->win, getSelectedText().c_str());
  }
};
#endif //DOCB_VCV_HEXFIELD_H
