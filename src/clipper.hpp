//
// Created by docb on 2/26/23.
//

#ifndef DBRACKMODULES_CLIPPER_HPP
#define DBRACKMODULES_CLIPPER_HPP
#include "Waveshaping.hpp"
template<typename T>
struct Clipper {
  HardClipper<T> hc;
  SoftClipper<T> sc;
  T soft(T drive,T in) {
    T n=in*drive;
    return simd::ifelse(n>1.5f,1.f,simd::ifelse(n<-1.5f,-1.f,n-(4.f/27.f)*n*n*n));
  }

  T hard(T drive,T in) {
    T x=in*drive;
    return simd::ifelse(simd::fabs(x)>1,sgn(x),x);
  }

  T dsin(T drive,T in) {
    T n=in*drive;
    return simd::ifelse(n>2.4674f,1.f,simd::ifelse(n<-2.4674f,-1.f,simd::sin(n*0.63661977f)));
  }

  T overdrive(T drive, T in) {
    T n=in*drive;
    T ab=simd::fabs(n);

    return simd::ifelse(ab<0.333333f,2.f*n,simd::ifelse(ab<0.666666f,sgn(n)*0.33333f*(3.f-(2.f-3.f*ab)*(2.f-3.f*ab)),sgn(n)*1.f));
  }

  T dexp(T drive, T in) {
    T n=in*drive;
    return simd::sgn(n)*(1-simd::exp(-simd::abs(n)));
  }

  T dabs(T drive, T in) {
    T n=in*drive;
    return n/(1+simd::fabs(n));
  }

  T sqr(T drive, T in) {
    T n=in*drive;
    return n/simd::sqrt(1+n*n);
  }

  T hardAA(T drive, T in) {
    T n=in*drive;
    return hc.antialiasedHardClipN1(n);
  }

  T softAA(T drive, T in) {
    T n=in*drive;
    return sc.antialiasedSoftClipN1(n);
  }

  T process(T drive,T in,int mode) {
    switch(mode) {
      case 0: return soft(drive,in);
      case 1: return hard(drive,in);
      case 2: return dsin(drive,in);
      case 3: return overdrive(drive,in);
      case 4: return dexp(drive,in);
      case 5: return sqr(drive,in);
      case 6: return dabs(drive,in);
      case 7: return hardAA(drive,in);
      default: return softAA(drive,in);
    }
  }
};



#endif //DBRACKMODULES_CLIPPER_HPP
