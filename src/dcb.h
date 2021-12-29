#ifndef __DCB
#define __DCB
#include "plugin.hpp"
#include <sstream>
#include <iomanip>
#include <utility>

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
    uint len = labels.size();
    for (uint i = 0; i < len; i++) {
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
struct NumberDisplayWidget : TransparentWidget {
  float *value = nullptr;
  float size = 10;
  std::string fontPath;
  NumberDisplayWidget() {
    fontPath = asset::plugin(pluginInstance, "res/FreeMonoBold.ttf");
  };

  void draw(const DrawArgs &args) override {
    if (!value) {
      return;
    }
    std::shared_ptr<Font> font =  APP->window->loadFont(fontPath);

    nvgFontSize(args.vg, size);
    nvgFontFaceId(args.vg, font->handle);
    nvgTextLetterSpacing(args.vg, 0);

    std::stringstream to_display;
    to_display.precision(4);
    to_display << std::setw(4) << *value;

    Vec textPos = Vec(4.0f, size-1.f);
    NVGcolor textColor = nvgRGB(0x4e, 0xC4, 0x88);
    nvgFillColor(args.vg, textColor);
    nvgText(args.vg, textPos.x, textPos.y, to_display.str().c_str(), NULL);
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
      uint n=json_integer_value(json_array_get(entry,0));
      uint p=json_integer_value(json_array_get(entry,1));
      labels[k]=std::to_string(n) + "/" + std::to_string(p);
      values[k]=float(n)/float(p);
    }
  }
};
struct UpdateOnReleaseKnob : TrimbotWhite {
  //T *module;
  bool *update;
  UpdateOnReleaseKnob() : TrimbotWhite() {

  }

  void onDragEnd(const DragEndEvent &e) override {
    SvgKnob::onDragEnd(e);
    if(e.button==GLFW_MOUSE_BUTTON_LEFT) {
      INFO("update");
      *update=true;
    }

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
#define TWOPIF 6.2831853f
#define MHEIGHT 128.5f

#endif
