#include "dcb.h"

void SmallButtonWithLabel::draw(const Widget::DrawArgs &args) {
  std::shared_ptr<Font> font = APP->window->loadFont(fontPath);
  SvgSwitch::draw(args);
  if(label.length()>0) {
    nvgFontSize(args.vg, 8);
    nvgFontFaceId(args.vg, font->handle);
    NVGcolor textColor = nvgRGB(0xff, 0xff, 0xaa);
    nvgTextAlign(args.vg, NVG_ALIGN_CENTER);
    nvgFillColor(args.vg, textColor);
    nvgText(args.vg, 11, 7, label.c_str(), NULL);
  }
}

void SelectButton::onDragEnter(const event::DragEnter &e) {
  if (e.button == GLFW_MOUSE_BUTTON_LEFT) {
    auto origin = dynamic_cast<SelectParam*>(e.origin);
    if (origin) {
      auto paramWidget = getAncestorOfType<ParamWidget>();
      assert(paramWidget);
      engine::ParamQuantity* pq = paramWidget->getParamQuantity();
      if (pq) {
        pq->setValue(_value);
      }
    }
  }
  Widget::onDragEnter(e);
}
void UpButtonWidget::onButton(const ButtonEvent& e) {

  if(e.action == GLFW_PRESS && e.button==GLFW_MOUSE_BUTTON_LEFT) {
    pressed = true;
    auto paramWidget=getAncestorOfType<ParamWidget>();
    assert(paramWidget);
    engine::ParamQuantity *pq=paramWidget->getParamQuantity();
    if(pq) {
      if(pq->getValue()<pq->getMaxValue()) {
        pq->setValue(pq->getValue()+1);
        ChangeEvent c;
        paramWidget->onChange(c);
      }
    } else {
      INFO("no pq");
    }
    e.consume(this);
  }
  if(e.action == GLFW_RELEASE && e.button==GLFW_MOUSE_BUTTON_LEFT) {
    pressed = false;
  }
}

void DownButtonWidget::onButton(const ButtonEvent& e) {
  if(e.action == GLFW_PRESS && e.button==GLFW_MOUSE_BUTTON_LEFT) {
    pressed = true;
    auto paramWidget=getAncestorOfType<ParamWidget>();
    assert(paramWidget);
    engine::ParamQuantity *pq=paramWidget->getParamQuantity();
    if(pq) {
      if(pq->getValue()>pq->getMinValue()) {
        pq->setValue(pq->getValue()-1);
        ChangeEvent c;
        paramWidget->onChange(c);
      }
    }
    e.consume(this);
  }
  if(e.action == GLFW_RELEASE && e.button==GLFW_MOUSE_BUTTON_LEFT) {
    pressed = false;
  }
}