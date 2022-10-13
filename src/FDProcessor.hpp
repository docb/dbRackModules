
#ifndef DBCAEMODULES_FDPROCESSOR_HPP
#define DBCAEMODULES_FDPROCESSOR_HPP
#include "Gamma/DFT.h"
template<typename M, int S, int W, int T, int A>
struct FDProcessor {
  gam::STFT stft{S,S/4,0,(gam::WindowType)W,(gam::SpectralType)T,A};

  float process(float in, M& modulator) {
    if(stft(in)) {
      modulator.process(stft);
    }
    return stft();
  }


};
#endif //DBCAEMODULES_FDPROCESSOR_HPP
