//
// Created by docb on 1/19/23.
//

#ifndef DBRACKMODULES_OFS_HPP
#define DBRACKMODULES_OFS_HPP
using simd::float_4;
struct OfsW {
  int outputId;
  int inputId;
  int offsetParamId;
  int offsetCVParamId;
  int offsetInputId;
  int scaleParamId;
  int scaleCVParamId;
  int scaleInputId;
  OfsW(int _outputId,int _inputId,int _offsetParamId,int _offsetCVParamId,int _offsetInputId,int _scaleParamId,int _scaleCVParamId,int _scaleInputId) : outputId(_outputId),inputId(_inputId),offsetParamId(_offsetParamId),offsetCVParamId(_offsetCVParamId),offsetInputId(_offsetInputId),scaleParamId(_scaleParamId),scaleCVParamId(_scaleCVParamId),scaleInputId(_scaleInputId) {}
  void process(Module *m,bool offsetThenScale) {
    if(m->outputs[outputId].isConnected()) {
      if(m->inputs[inputId].isConnected()) {
        int channels=m->inputs[inputId].getChannels();
        for(int c=0;c<channels;c+=4) {
          float_4 in=m->inputs[inputId].getVoltageSimd<float_4>(c);
          float_4 offset=m->params[offsetParamId].getValue()+m->inputs[offsetInputId].getPolyVoltageSimd<float_4>(c)*m->params[offsetCVParamId].getValue();
          float_4 scale=m->params[scaleParamId].getValue()+m->inputs[scaleInputId].getPolyVoltageSimd<float_4>(c)*m->params[scaleCVParamId].getValue();
          if(offsetThenScale) {
            m->outputs[outputId].setVoltageSimd((in+offset)*scale,c);
          } else {
            m->outputs[outputId].setVoltageSimd(in*scale+offset,c);
          }
          m->outputs[outputId].setChannels(channels);
        }
      } else {
        m->outputs[outputId].setVoltage(m->params[offsetParamId].getValue()+m->inputs[offsetInputId].getVoltage()*m->params[offsetCVParamId].getValue());
        m->outputs[outputId].setChannels(1);
      }
    }
  }
};

#endif //DBRACKMODULES_OFS_HPP
