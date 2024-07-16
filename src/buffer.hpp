//
// Created by docb on 7/15/24.
//

#ifndef DBRACKMODULES_BUFFER_HPP
#define DBRACKMODULES_BUFFER_HPP
template<typename T>
struct RBufferBase {
  virtual bool in_empty()=0;
  virtual bool in_full()=0;
  virtual T in_shift()=0;
  virtual void in_push(T val)=0;
  virtual bool out_empty()=0;
  virtual bool out_full()=0;
  virtual T out_shift()=0;
  virtual void out_push(T val)=0;
};
template<typename T, int S>
struct RBuffer : RBufferBase<T> {
  rack::dsp::RingBuffer<T,S> outBuffer;
  rack::dsp::RingBuffer<T,S> inBuffer;
  bool in_empty() override {
    return inBuffer.empty();
  };
  bool in_full() override {
    return inBuffer.full();
  };
  T in_shift() override {
    return inBuffer.shift();
  }
  void in_push(T val) override {
    inBuffer.push(val);
  }
  bool out_empty() override {
    return outBuffer.empty();
  };
  bool out_full() override {
    return outBuffer.full();
  };
  T out_shift() override {
    return outBuffer.shift();
  }
  void out_push(T val) override {
    outBuffer.push(val);
  }
};
template<typename T>
struct RBufferMgr {
  int bufferSizeIndex=3;
  RBufferMgr() {
    buffer=&b256;
  }
  RBufferBase<T> *buffer;
  RBuffer<T,32> b32;
  RBuffer<T,64> b64;
  RBuffer<T,128> b128;
  RBuffer<T,256> b256;
  RBuffer<T,512> b512;
  RBuffer<T,1024> b1024;
  void setBufferSize(int s) {
    bufferSizeIndex=s;
    switch(s) {
      case 0:
        buffer=&b32;break;
      case 1:
        buffer=&b64;break;
      case 2:
        buffer=&b128;break;
      case 3:
        buffer=&b256;break;
      case 4:
        buffer=&b512;break;
      case 5:
        buffer=&b1024;break;
      default: break;
    }
  }
};

#endif //DBRACKMODULES_BUFFER_HPP
