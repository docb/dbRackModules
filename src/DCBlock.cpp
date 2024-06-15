#include "dcb.h"

using simd::float_4;
template<size_t S>
struct Buffers {
  rack::dsp::RingBuffer<float_4,S*2> delayBuffer[2][4]={};
  rack::dsp::RingBuffer<float_4,S> iirBuffer[2][4][4]={};
  float_4 ydels[2][4][4]={};
};

struct DelayBuffer {
  Buffers<128> buffer128;
  Buffers<64> buffer64;
  Buffers<32> buffer32;
  Buffers<16> buffer16;
  Buffers<8> buffer8;
  Buffers<4> buffer4;

  void pushDel(int order,int nr,int chn,float_4 v) {
    switch(order) {
      case 0:
        buffer4.delayBuffer[nr][chn].push(v);
        break;
      case 1:
        buffer8.delayBuffer[nr][chn].push(v);
        break;
      case 2:
        buffer16.delayBuffer[nr][chn].push(v);
        break;
      case 3:
        buffer32.delayBuffer[nr][chn].push(v);
        break;
      case 4:
        buffer64.delayBuffer[nr][chn].push(v);
        break;
      default:
        buffer128.delayBuffer[nr][chn].push(v);
        break;
    }
  }

  float_4 shiftDel(int order,int nr,int chn) {
    switch(order) {
      case 0:
        return buffer4.delayBuffer[nr][chn].shift();
      case 1:
        return buffer8.delayBuffer[nr][chn].shift();
      case 2:
        return buffer16.delayBuffer[nr][chn].shift();
      case 3:
        return buffer32.delayBuffer[nr][chn].shift();
      case 4:
        return buffer64.delayBuffer[nr][chn].shift();
      default:
        return buffer128.delayBuffer[nr][chn].shift();
    }
  }

  void pushIIR(int order,int nr,int chn,int iirNr,float_4 v) {
    switch(order) {
      case 0:
        buffer4.iirBuffer[nr][chn][iirNr].push(v);
        break;
      case 1:
        buffer8.iirBuffer[nr][chn][iirNr].push(v);
        break;
      case 2:
        buffer16.iirBuffer[nr][chn][iirNr].push(v);
        break;
      case 3:
        buffer32.iirBuffer[nr][chn][iirNr].push(v);
        break;
      case 4:
        buffer64.iirBuffer[nr][chn][iirNr].push(v);
        break;
      default:
        buffer128.iirBuffer[nr][chn][iirNr].push(v);
        break;
    }
  }

  float_4 shiftIIR(int order,int nr,int chn,int iirNr) {
    switch(order) {
      case 0:
        return buffer4.iirBuffer[nr][chn][iirNr].shift();
      case 1:
        return buffer8.iirBuffer[nr][chn][iirNr].shift();
      case 2:
        return buffer16.iirBuffer[nr][chn][iirNr].shift();
      case 3:
        return buffer32.iirBuffer[nr][chn][iirNr].shift();
      case 4:
        return buffer64.iirBuffer[nr][chn][iirNr].shift();
      default:
        return buffer128.iirBuffer[nr][chn][iirNr].shift();
    }
  }

  float_4 getYDel(int order,int nr,int chn,int iirNr) {
    switch(order) {
      case 0:
        return buffer4.ydels[nr][chn][iirNr];
      case 1:
        return buffer8.ydels[nr][chn][iirNr];
      case 2:
        return buffer16.ydels[nr][chn][iirNr];
      case 3:
        return buffer32.ydels[nr][chn][iirNr];
      case 4:
        return buffer64.ydels[nr][chn][iirNr];
      default:
        return buffer128.ydels[nr][chn][iirNr];
    }
  }

  void setYDel(int order,int nr,int chn,int iirNr,float_4 v) {
    switch(order) {
      case 0:
        buffer4.ydels[nr][chn][iirNr]=v;
        break;
      case 1:
        buffer8.ydels[nr][chn][iirNr]=v;
        break;
      case 2:
        buffer16.ydels[nr][chn][iirNr]=v;
        break;
      case 3:
        buffer32.ydels[nr][chn][iirNr]=v;
        break;
      case 4:
        buffer64.ydels[nr][chn][iirNr]=v;
        break;
      default:
        buffer128.ydels[nr][chn][iirNr]=v;
        break;
    }
  }

  static float getScale(int order) {
    switch(order) {
      case 0:
        return 1/4.f;
      case 1:
        return 1/8.f;
      case 2:
        return 1/16.f;
      case 3:
        return 1/32.f;
      case 4:
        return 1/64.f;
      default:
        return 1/128.f;
    }
  }
};

struct DCBlock : Module {
  enum ParamIds {
    ORDER_PARAM,NUM_PARAMS
  };
  enum InputIds {
    V_INPUT_L,V_INPUT_R,NUM_INPUTS
  };
  enum OutputIds {
    V_OUTPUT_L,V_OUTPUT_R,NUM_OUTPUTS
  };
  enum LightIds {
    NUM_LIGHTS
  };

  DelayBuffer delayBuffer;
  int lastOrder=5;

  DCBlock() {
    config(NUM_PARAMS,NUM_INPUTS,NUM_OUTPUTS,NUM_LIGHTS);
    configSwitch(ORDER_PARAM,0.f,5.f,5.f,"Order",{"4","8","16","32","64","128"});
    configBypass(V_INPUT_L,V_OUTPUT_L);
    configBypass(V_INPUT_R,V_OUTPUT_R);
    configOutput(V_OUTPUT_L,"Out L");
    configOutput(V_OUTPUT_R,"Out R");
    configOutput(V_INPUT_L,"In L");
    configOutput(V_INPUT_R,"Out R");
  }

  void processChannel(int nr,int output,int chn,int order) {

    float_4 in=inputs[nr].getVoltageSimd<float_4>(chn);
    int f4chn=chn/4;
    float_4 del=delayBuffer.shiftDel(order,nr,f4chn);
    float_4 x1=in;
    delayBuffer.pushDel(order,nr,f4chn,in);
    for(int j=0;j<4;j++) {
      float_4 x2=delayBuffer.shiftIIR(order,nr,f4chn,j);
      delayBuffer.pushIIR(order,nr,f4chn,j,x1);
      delayBuffer.setYDel(order,nr,f4chn,j,x1-x2+delayBuffer.getYDel(order,nr,f4chn,j));
      x1=delayBuffer.getYDel(order,nr,f4chn,j)*float_4(DelayBuffer::getScale(order));
    }
    outputs[output].setVoltageSimd<float_4>(del-x1,chn);
  }

  void process(const ProcessArgs &args) override {
    int order=(int)params[ORDER_PARAM].getValue();
    if(inputs[V_INPUT_L].isConnected()) {
      int channels=std::max(inputs[V_INPUT_L].getChannels(),1);
      for(int k=0;k<channels;k+=4) {
        processChannel(V_INPUT_L,V_OUTPUT_L,k,order);
      }
      outputs[V_OUTPUT_L].setChannels(channels);
    }
    if(inputs[V_INPUT_R].isConnected()) {
      int channels=std::max(inputs[V_INPUT_R].getChannels(),1);
      for(int k=0;k<channels;k+=4) {
        processChannel(V_INPUT_R,V_OUTPUT_R,k,order);
      }
      outputs[V_OUTPUT_R].setChannels(channels);
    }
  }
};


struct DCBlockWidget : ModuleWidget {
  explicit DCBlockWidget(DCBlock *module) {
    setModule(module);
    setPanel(APP->window->loadSvg(asset::plugin(pluginInstance,"res/DCBlock.svg")));
    addParam(createParam<TrimbotWhiteSnap>(mm2px(Vec(2,38)),module,DCBlock::ORDER_PARAM));

    addInput(createInput<SmallPort>(mm2px(Vec(1.9f,80)),module,DCBlock::V_INPUT_L));
    addInput(createInput<SmallPort>(mm2px(Vec(1.9f,92)),module,DCBlock::V_INPUT_R));
    addOutput(createOutput<SmallPort>(mm2px(Vec(1.9f,104)),module,DCBlock::V_OUTPUT_L));
    addOutput(createOutput<SmallPort>(mm2px(Vec(1.9f,116)),module,DCBlock::V_OUTPUT_R));
  }
};


Model *modelDCBlock=createModel<DCBlock,DCBlockWidget>("DCBlock");
