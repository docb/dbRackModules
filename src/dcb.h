#ifndef __DCB
#define __DCB
#include "plugin.hpp"
#include <sstream>
#include <iomanip>
#include <utility>
#include "rnd.h"
#include <bitset>
struct SelectButton : Widget {
  int _value;
  std::string _label;
  std::basic_string<char> fontPath;
  SelectButton(int value, std::string  label) : _value(value),_label(std::move(label)) {
    fontPath = asset::plugin(pluginInstance, "res/FreeMonoBold.ttf");
  }
  void draw(const DrawArgs& args) override {
    std::shared_ptr<Font> font =  APP->window->loadFont(fontPath);
    int currentValue = 0;
    auto paramWidget = getAncestorOfType<ParamWidget>();
    assert(paramWidget);
    engine::ParamQuantity* pq = paramWidget->getParamQuantity();
    if (pq)
       currentValue = std::round(pq->getValue());

    if (currentValue == _value) {
      nvgFillColor(args.vg,nvgRGB(0x7e,0xa6,0xd3));
    } else {
      nvgFillColor(args.vg,nvgRGB(0x3c,0x4c,0x71));
    }
    nvgStrokeColor(args.vg,nvgRGB(0xc4,0xc9,0xc2));
    nvgBeginPath(args.vg);
    nvgRoundedRect(args.vg,0,0,box.size.x,box.size.y,3.f);
    nvgFill(args.vg);nvgStroke(args.vg);
    nvgFontSize(args.vg, box.size.y-2);
    nvgFontFaceId(args.vg, font->handle);
    NVGcolor textColor = nvgRGB(0xff, 0xff, 0xaa);
    nvgTextAlign(args.vg, NVG_ALIGN_CENTER | NVG_ALIGN_MIDDLE);
    nvgFillColor(args.vg, textColor);
    nvgText(args.vg, box.size.x/2.f, box.size.y/2.f, _label.c_str(), NULL);
  }

  void onDragHover(const event::DragHover& e) override {
    if (e.button == GLFW_MOUSE_BUTTON_LEFT) {
      e.consume(this);
    }
    Widget::onDragHover(e);
  }

  void onDragEnter(const event::DragEnter& e) override;
};
struct SelectParam : ParamWidget {

  void init(std::vector<std::string> labels) {
    const float margin = 0;
    float height = box.size.y - 2 * margin;
    unsigned int len = labels.size();
    for (unsigned int i = 0; i < len; i++) {
      auto selectButton = new SelectButton(i,labels[i]);
      selectButton->box.pos = Vec(0,height/len*i+margin);
      selectButton->box.size = Vec(box.size.x,height/len);
      addChild(selectButton);
    }
  }

  void draw(const DrawArgs& args) override {
    // Background
    nvgBeginPath(args.vg);
    nvgRect(args.vg, 0, 0, box.size.x, box.size.y);
    nvgFillColor(args.vg, nvgRGB(0, 0, 0));
    nvgFill(args.vg);

    ParamWidget::draw(args);
  }
};



struct SmallButton : SvgSwitch {
  SmallButton() {
    momentary = false;
    addFrame(APP->window->loadSvg(asset::plugin(pluginInstance, "res/SmallButton0.svg")));
    addFrame(APP->window->loadSvg(asset::plugin(pluginInstance, "res/SmallButton1.svg")));
    fb->removeChild(shadow);
    delete shadow;
  }
};
struct SmallButtonWithLabel : SvgSwitch {
  std::string label;
  std::basic_string<char> fontPath;
  SmallButtonWithLabel() {
    fontPath = asset::plugin(pluginInstance, "res/FreeMonoBold.ttf");
    momentary = false;
    addFrame(APP->window->loadSvg(asset::plugin(pluginInstance, "res/SmallButton0.svg")));
    addFrame(APP->window->loadSvg(asset::plugin(pluginInstance, "res/SmallButton1.svg")));
    fb->removeChild(shadow);
    delete shadow;
  }
  void setLabel(const std::string &lbl) {
    label = lbl;
  }
  void draw(const DrawArgs &args) override;

};
struct SmallRoundButton : SvgSwitch {
    SmallRoundButton() {
        momentary = false;
        addFrame(APP->window->loadSvg(asset::plugin(pluginInstance, "res/button_9px_off.svg")));
        addFrame(APP->window->loadSvg(asset::plugin(pluginInstance, "res/button_9px_active.svg")));
        fb->removeChild(shadow);
        delete shadow;
    }
};
template <typename TLightBase = RedLight>
struct LEDLightSliderFixed : LEDLightSlider<TLightBase> {
    LEDLightSliderFixed() {
        this->setHandleSvg(APP->window->loadSvg(asset::plugin(pluginInstance, "res/LEDSliderHandle.svg")));
    }
};

struct TrimbotWhite : SvgKnob {
  TrimbotWhite() {
    minAngle = -0.8 * M_PI;
    maxAngle = 0.8 * M_PI;
    setSvg(APP->window->loadSvg(asset::plugin(pluginInstance, "res/TrimpotWhite.svg")));
  }
};
struct TrimbotWhiteSnap : SvgKnob {
  TrimbotWhiteSnap() {
    snap = true;
    minAngle = -0.8 * M_PI;
    maxAngle = 0.8 * M_PI;
    setSvg(APP->window->loadSvg(asset::plugin(pluginInstance, "res/TrimpotWhite.svg")));
  }
};
struct TrimbotWhite9 : SvgKnob {
  TrimbotWhite9() {
    minAngle = -0.8 * M_PI;
    maxAngle = 0.8 * M_PI;
    setSvg(APP->window->loadSvg(asset::plugin(pluginInstance, "res/TrimpotWhite9mm.svg")));
  }
};
struct TrimbotWhite9Snap : SvgKnob {
  TrimbotWhite9Snap() {
    snap = true;
    minAngle = -0.8 * M_PI;
    maxAngle = 0.8 * M_PI;
    setSvg(APP->window->loadSvg(asset::plugin(pluginInstance, "res/TrimpotWhite9mm.svg")));
  }
};
struct SmallPort : app::SvgPort {
  SmallPort() {
    setSvg(APP->window->loadSvg(asset::plugin(pluginInstance, "res/SmallPort.svg")));
  }
};
template <typename TBase>
struct DBSmallLight : TSvgLight<TBase> {
  DBSmallLight() {
    this->setSvg(Svg::load(asset::plugin(pluginInstance, "res/SmallLight.svg")));
  }
};

template<int S>
struct Scale {
  std::string name;
  float values[S]={};
  std::string labels[S];

  Scale(json_t *scaleObj) {
    name=json_string_value(json_object_get(scaleObj,"name"));
    json_t *scale=json_object_get(scaleObj,"scale");
    size_t scaleLen=json_array_size(scale);
    printf("parsing %s len=%zu\n",name.c_str(),scaleLen);
    if(scaleLen!=S)
      throw Exception(string::f("Scale must have exact %d entries",S));
    for(size_t k=0;k<scaleLen;k++) {
      json_t *entry=json_array_get(scale,k);
      size_t elen=json_array_size(entry);
      if(elen!=2)
        throw Exception(string::f("Scale entry must be an array of length 2"));
      unsigned int n=json_integer_value(json_array_get(entry,0));
      unsigned int p=json_integer_value(json_array_get(entry,1));
      labels[k]=std::to_string(n) + "/" + std::to_string(p);
      values[k]=float(n)/float(p);
    }
  }
};
struct UpdateOnReleaseKnob : TrimbotWhite {
  bool *update=nullptr;
  bool contextMenu;
  UpdateOnReleaseKnob() : TrimbotWhite() {

  }

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
    if(update!=nullptr) *update=contextMenu;
  }

  void onDragEnd(const DragEndEvent &e) override {
    SvgKnob::onDragEnd(e);
    if(e.button==GLFW_MOUSE_BUTTON_LEFT) {
      if(update!=nullptr) *update=true;
    }

  }
};

template<typename M>
struct MKnob : TrimbotWhite {
  M *module=nullptr;
  void step() override {
    if(module && module->dirty) {
      ChangeEvent c;
      SvgKnob::onChange(c);
      module->dirty--;
    }
    SvgKnob::step();
  }
};
struct MLED : public SvgSwitch {
  MLED() {
    momentary=false;
    //latch=true;
    addFrame(Svg::load(asset::plugin(pluginInstance,"res/RButton0.svg")));
    addFrame(Svg::load(asset::plugin(pluginInstance,"res/RButton1.svg")));
  }
};
struct MLEDM : public SvgSwitch {
  MLEDM() {
    momentary=true;
    //latch=true;
    addFrame(Svg::load(asset::plugin(pluginInstance,"res/RButton0.svg")));
    addFrame(Svg::load(asset::plugin(pluginInstance,"res/RButton1.svg")));
  }
};


template<typename M>
struct MCopyButton : SmallButtonWithLabel {
  M *module;

  MCopyButton() : SmallButtonWithLabel() {
    momentary=true;
  }

  void onChange(const ChangeEvent &e) override {
    SvgSwitch::onChange(e);
    if(module) {
      if(module->params[M::COPY_PARAM].getValue()>0)
        module->copy();
    }
  }
};

template<typename M>
struct MPasteButton : SmallButtonWithLabel {
  M *module;

  MPasteButton() : SmallButtonWithLabel() {
    momentary=true;
  }

  void onChange(const ChangeEvent &e) override {
    SvgSwitch::onChange(e);
    if(module) {
      if(module->params[M::PASTE_PARAM].getValue()>0)
        module->paste();
    }

  }
};

struct UpButtonWidget : Widget {
  bool pressed=false;

  void draw(const DrawArgs &args) override {
    nvgFillColor(args.vg,pressed?nvgRGB(126,200,211):nvgRGB(126,166,211));
    nvgStrokeColor(args.vg,nvgRGB(196,201,104));
    nvgBeginPath(args.vg);
    nvgMoveTo(args.vg,2,box.size.y);
    nvgLineTo(args.vg,box.size.x/2.f,2);
    nvgLineTo(args.vg,box.size.x-2,box.size.y);
    nvgClosePath(args.vg);
    nvgFill(args.vg);
    nvgStroke(args.vg);
  }

  void onDragHover(const event::DragHover &e) override {
    if(e.button==GLFW_MOUSE_BUTTON_LEFT) {
      e.consume(this);
    }
    Widget::onDragHover(e);
  }

  void onButton(const ButtonEvent &e) override;


};

struct DownButtonWidget : Widget {
  bool pressed=false;

  void draw(const DrawArgs &args) override {

    nvgFillColor(args.vg,pressed?nvgRGB(126,200,211):nvgRGB(126,166,211));
    nvgStrokeColor(args.vg,nvgRGB(196,201,104));
    nvgBeginPath(args.vg);
    nvgMoveTo(args.vg,2,0);
    nvgLineTo(args.vg,box.size.x/2.f,box.size.y-2);
    nvgLineTo(args.vg,box.size.x-2,0);
    nvgClosePath(args.vg);
    nvgFill(args.vg);
    nvgStroke(args.vg);
  }

  void onDragHover(const event::DragHover &e) override {
    if(e.button==GLFW_MOUSE_BUTTON_LEFT) {
      e.consume(this);
    }
    Widget::onDragHover(e);
  }

  void onButton(const ButtonEvent &e) override;
};

struct NumberDisplayWidget : Widget {
  std::basic_string<char> fontPath;
  bool highLight = false;

  NumberDisplayWidget() : Widget() {
    fontPath=asset::plugin(pluginInstance,"res/FreeMonoBold.ttf");
  }

  void drawLayer(const DrawArgs &args,int layer) override {
    if(layer==1) {
      _draw(args);
    }
    Widget::drawLayer(args,layer);
  }

  void onDoubleClick(const DoubleClickEvent &e) override {
    INFO("double click - does not work");
    auto paramWidget=getAncestorOfType<ParamWidget>();
    assert(paramWidget);
    paramWidget->resetAction();
    e.consume(this);
  }

  void onButton(const ButtonEvent &e) override {
    INFO("display on button");
    e.unconsume();
  }

  void _draw(const DrawArgs &args) {
    std::shared_ptr<Font> font=APP->window->loadFont(fontPath);
    auto paramWidget=getAncestorOfType<ParamWidget>();
    assert(paramWidget);
    engine::ParamQuantity *pq=paramWidget->getParamQuantity();
    nvgBeginPath(args.vg);
    nvgRect(args.vg,0,0,box.size.x,box.size.y);
    nvgStrokeColor(args.vg,nvgRGB(128,128,128));
    nvgStroke(args.vg);

    if(pq) {
      std::stringstream stream;
      stream<<std::setfill('0')<<std::setw(2)<<int(pq->getValue());
      nvgFillColor(args.vg,highLight?nvgRGB(255,255,128):nvgRGB(0,255,0));
      nvgFontFaceId(args.vg,font->handle);
      nvgFontSize(args.vg,10);
      nvgTextAlign(args.vg,NVG_ALIGN_CENTER|NVG_ALIGN_MIDDLE);
      nvgText(args.vg,box.size.x/2,box.size.y/2,stream.str().c_str(),NULL);
    }
  }
};

struct SpinParamWidget : ParamWidget {
  bool pressed=false;
  int start;
  float dragY;
  UpButtonWidget *up;
  DownButtonWidget *down;
  NumberDisplayWidget *text;
  void init() {
    box.size.x=20;
    box.size.y=30;
    up=new UpButtonWidget();
    up->box.pos=Vec(0,0);
    up->box.size=Vec(20,10);
    addChild(up);
    text=new NumberDisplayWidget();
    text->box.pos=Vec(0,10);
    text->box.size=Vec(20,10);
    addChild(text);
    down=new DownButtonWidget();
    down->box.pos=Vec(0,20);
    down->box.size=Vec(20,10);
    addChild(down);
  }

  void onButton(const ButtonEvent &e) override {
    ParamWidget::onButton(e);
    if(e.action==GLFW_PRESS&&e.button==GLFW_MOUSE_BUTTON_LEFT) {
      engine::ParamQuantity *pq=getParamQuantity();
      start=pq->getValue();
      pressed=true;
    }
    if(e.action==GLFW_RELEASE&&e.button==GLFW_MOUSE_BUTTON_LEFT) {
      pressed=false;
    }
    e.consume(this);
  }

  void onDoubleClick(const DoubleClickEvent &e) override {

  }

  void onDragStart(const event::DragStart &e) override {
    if(pressed) {
      dragY=APP->scene->rack->getMousePos().y;
      text->highLight = true;
    }
  }

  void onDragEnd(const event::DragEnd& e) override {
    up->pressed = false;
    down->pressed = false;
    text->highLight = false;
  }

  void onDragMove(const event::DragMove &e) override {
    if(pressed) {
      float diff=dragY-APP->scene->rack->getMousePos().y;
      engine::ParamQuantity *pq=getParamQuantity();
      float newValue=float(start)+diff/4.0f;
      if(newValue>pq->getMaxValue()) pq->setValue(pq->getMaxValue());
      else if(newValue<pq->getMinValue()) pq->setValue(pq->getMinValue());
      else pq->setValue(newValue);
    }
  }

  void draw(const DrawArgs &args) override {
    // Background
    nvgBeginPath(args.vg);
    nvgRect(args.vg,0,0,box.size.x,box.size.y);
    nvgFillColor(args.vg,nvgRGB(0,0,0));
    nvgStrokeColor(args.vg,nvgRGB(128,128,128));
    nvgFill(args.vg);
    nvgStroke(args.vg);

    ParamWidget::draw(args);
  }

};


template<typename T>
struct DCBlocker {
  T x = 0.f;
  T y = 0.f;
  T process(T v) {
    y = v - x + y * 0.99f;
    x = v;
    return y;
  }
  void reset() {
    x = 0.f;
    y = 0.f;
  }
};
template<size_t S>
struct Averager {
  rack::dsp::RingBuffer<float,S> buffer;

  float process(float val) {
    buffer.push(val);
    float sum=0.f;
    for(unsigned int k=0;k<S;k++)
      sum+=buffer.data[k];
    return sum/(float)S;
  }
};

template<typename T>
struct DensQuantity : Quantity {
  T* module;

  DensQuantity(T* m) : module(m) {}

  void setValue(float value) override {
    value = clamp(value, getMinValue(), getMaxValue());
    if (module) {
      module->randomDens = value;
    }
  }

  float getValue() override {
    if (module) {
      return module->randomDens;
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
template<typename T>
struct DensSlider : ui::Slider {
  DensSlider(T* module) {
    quantity = new DensQuantity<T>(module);
    box.size.x = 200.0f;
  }
  virtual ~DensSlider() {
    delete quantity;
  }
};
template<typename T>
struct DensMenuItem : MenuItem {
  T* module;

  DensMenuItem(T* m) : module(m) {
    this->text = "Random";
    this->rightText = "???";
  }

  Menu* createChildMenu() override {
    Menu* menu = new Menu;
    menu->addChild(new DensSlider<T>(module));
    return menu;
  }
};

struct IntSelectItem : MenuItem {
  int *value;
  int min;
  int max;

  IntSelectItem(int *val,int _min,int _max) : value(val),min(_min),max(_max) {
  }

  Menu *createChildMenu() override {
    Menu *menu=new Menu;
    for(int c=min;c<=max;c++) {
      menu->addChild(createCheckMenuItem(string::f("%d",c),"",[=]() {
        return *value==c;
      },[=]() {
        *value=c;
      }));
    }
    return menu;
  }
};
struct MinMaxRange {
  float min;
  float max;
};
template<typename M>
struct RangeSelectItem : MenuItem {
  M *module;
  std::vector <MinMaxRange> ranges;

  RangeSelectItem(M *_module,std::vector <MinMaxRange> _ranges) : module(_module),ranges(std::move(_ranges)) {
  }

  Menu *createChildMenu() override {
    Menu *menu=new Menu;
    for(unsigned int k=0;k<ranges.size();k++) {
      menu->addChild(createCheckMenuItem(string::f("%d/%dV",(int)ranges[k].min,(int)ranges[k].max),"",[=]() {
        return module->min==ranges[k].min&&module->max==ranges[k].max;
      },[=]() {
        module->min=ranges[k].min;
        module->max=ranges[k].max;
        module->reconfig();
      }));
    }
    return menu;
  }
};

#define TWOPIF 6.2831853f
#define MHEIGHT 128.5f
#define TY(x) MHEIGHT-(x)-6.237

#endif
