//
// Created by docb on 11/9/21.
//

#ifndef DOCB_VCV_HEXFIELD_H
#define DOCB_VCV_HEXFIELD_H
#include "rnd.h"
#include "textfield.hpp"
#include <bitset>
#include "hexutil.h"
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
  NVGcolor backgroundColor1;
  NVGcolor backgroundColorFocus1;
  NVGcolor dirtyColor;
  NVGcolor emptyColor;
  M *module;
  W *moduleWidget;
  bool dirty=false;
  bool rndInit = false;
  RND rnd;


  HexField() : MTextField() {
    fontPath = asset::plugin(pluginInstance,"res/FreeMonoBold.ttf");
    defaultTextColor=nvgRGB(0x20,0x44,0x20);
    dirtyColor=nvgRGB(0xAA,0x20,0x20);
    emptyColor=nvgRGB(0x88,0x88,0x88);
    textColor=defaultTextColor;
    backgroundColor=nvgRGB(0xcc,0xcc,0xcc);
    backgroundColorFocus=nvgRGB(0xcc,0xff,0xcc);
    backgroundColor1=nvgRGB(0xbb,0xbb,0xbb);
    backgroundColorFocus1=nvgRGB(0xaa,0xdd,0xaa);
    box.size=mm2px(Vec(45.5,6.f));
    font_size=14;
    charWidth = 16.5f;
    charWidth2 = 16.25f;
    letter_spacing=0.f;
    textOffset=Vec(2.f,2.f);
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
      bool ret=checkChar(c);
      if(!ret)
        return false;
    }
    return true;
  }

  static bool checkChar(int c) {
    return (c>=48&&c<58)||(c>=65&&c<=70)||(c>=97&&c<=102)||c=='*';
  }

  void onSelectText(const event::SelectText &e) override {
    if(onWidgetSelect) {
      onWidgetSelect = false;
      e.consume(nullptr);
    } else {
      //INFO("on select text %d %d %d\n",e.codepoint,cursor,selection);
      if((MTextField::text.size()<maxTextLength||cursor!=selection)&&checkChar(e.codepoint)) {
        std::string newText(1, (char) toupper(e.codepoint));
        insertText(newText);
        e.consume(this);
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
        std::transform(newText.begin(), newText.end(), newText.begin(), ::toupper);
        insertText(newText);
        if(isFocused) {
          dirty=true;
        } /*else {
          module->setHex(nr,text);
          dirty=false;
        }*/
      }
    }
  }
  void pasteClipboard(bool menu) override {
    pasteCheckedString();
  }

  unsigned int hexToInt(const std::string &hex) {
    unsigned int x;
    std::stringstream ss;
    ss<<std::hex<<hex;
    ss>>x;
    return x;
  }

  void onSelectKey(const event::SelectKey &e) override {
    bool act=e.action==GLFW_PRESS||e.action==GLFW_REPEAT;
    if(act&&(e.keyName == "v" && (e.mods & RACK_MOD_MASK) == RACK_MOD_CTRL)) {
      pasteCheckedString();
    } else if(act&&(e.keyName=="p")) {
      setText(getRandomHex(rnd,module->randomDens,module->randomLengthFrom,module->randomLengthTo));
      if((e.mods&RACK_MOD_MASK)==GLFW_MOD_SHIFT) {
        module->setHex(nr,text);
        dirty=false;
      }
    } else if(act&&(e.keyName=="r")) {
      if(text.size()>0) {
        std::replace( text.begin(), text.end(), '*', '0');
        unsigned long long x=toLong();
        unsigned long long mask=((1ULL&x)<<(text.size()*4-1));
        x>>=1;
        x|=mask;
        std::stringstream stream;
        stream<<std::uppercase<<std::setfill('0')<<std::setw(text.size())<<std::hex<<(x);
        setText(stream.str());
        if((e.mods&RACK_MOD_MASK)==GLFW_MOD_SHIFT) {
          module->setHex(nr,text);
          dirty=false;
        }
      }
    } else if(act&&(e.keyName=="l")) {
      if(text.size()>0) {
        std::replace( text.begin(), text.end(), '*', '0');
        unsigned long long x=toLong();
        int ps=text.size()*4-1;
        unsigned long long mask=(x&(1<<ps))>>ps;
        long long m=text.size()==16?-1:((1LL<<text.size()*4)-1LL);
        auto um=(unsigned long long)m;
        x=(x<<1);
        x|=mask;
        x&=um;
        std::stringstream stream;
        stream<<std::uppercase<<std::setfill('0')<<std::setw(text.size())<<std::hex<<(x);
        std::string str=stream.str().substr(0,text.size());
        setText(str);
        if((e.mods&RACK_MOD_MASK)==GLFW_MOD_SHIFT) {
          module->setHex(nr,text);
          dirty=false;
        }
      }
    } else if(act&&(e.keyName=="h")) {
      if(text.size()>0 && text.size()<=8) {
        std::vector<std::string> conv={"00","02","08","0A","20","22","28","2A","80","82","88","8A","A0","A2","A8","AA"};
        std::string newText;
        for(unsigned k=0;k<text.size();k++) {
          std::string ch=text.substr(k,1);
          if(ch=="*") newText.append("**");
          else {
            unsigned int i=hexToInt(ch);
            newText.append(conv[i]);
          }
        }
        setText(newText);
        if((e.mods&RACK_MOD_MASK)==GLFW_MOD_SHIFT) {
          module->setHex(nr,text);
          dirty=false;
        }
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
    //INFO("%d %d %f %f %f %f\n", cursor, selection, e.pos.x,e.pos.y,box.size.x,box.size.y);
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

  void drawBG(NVGcontext *vg) {
    for(int k=0;k<4;k++) {
      nvgBeginPath(vg);
      nvgRect(vg,k*(box.size.x*.25f-1),0,box.size.x*0.25,box.size.y);
      nvgFillColor(vg,isFocused?(k%2==0?backgroundColorFocus1:backgroundColorFocus):(k%2==1?backgroundColor:backgroundColor1));
      nvgFill(vg);
    }
  }

  void draw(const DrawArgs &args) override {
    std::shared_ptr <Font> font = APP->window->loadFont(fontPath);
    if(module && module->isDirty(nr)) {
      text = module->getHex(nr);
      cursor = 0;
      selection = 0;
      module->setDirty(nr,false);
      dirty = false;
    }
    auto vg=args.vg;
    nvgScissor(vg,0,0,box.size.x,box.size.y);

    drawBG(vg);

    if(font->handle>=0) {

      nvgFillColor(vg,dirty?dirtyColor:textColor);
      nvgFontFaceId(vg,font->handle);
      nvgFontSize(vg,font_size);
      nvgTextLetterSpacing(vg,letter_spacing);
      nvgTextAlign(vg,NVG_ALIGN_LEFT|NVG_ALIGN_TOP);
      if(module!=nullptr && text.size()==0 && module->getHex(nr).size()>0 && !isFocused) {
        nvgFillColor(vg,emptyColor);
        nvgText(vg,textOffset.x,textOffset.y,module->getHex(nr).c_str(),NULL);
      } else {
        nvgText(vg,textOffset.x,textOffset.y,text.c_str(),NULL);
      }
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
  }

  void copyClipboard(bool menu) override {
    if(menu) {
      if(cursor!=selection) {
        glfwSetClipboardString(APP->window->win,getSelectedText().c_str());
      } else {
        glfwSetClipboardString(APP->window->win,getText().c_str());
      }
    } else {
      if(cursor!=selection) {
        glfwSetClipboardString(APP->window->win,getSelectedText().c_str());
      }
    }
  }
  void cutClipboard(bool menu) override {
    copyClipboard(menu);
    if(cursor!=selection) {
      insertText("");
    } else {
      if(menu) setText("");
    }
  }

};
#endif //DOCB_VCV_HEXFIELD_H
