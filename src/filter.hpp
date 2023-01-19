
#ifndef DBRACKMODULES_FILTER_HPP
#define DBRACKMODULES_FILTER_HPP

#include <cfloat>
template<typename T>
struct Generic4Pole {
  T w[4]={};
  std::vector<float> a;
  std::vector<float> b;
  Generic4Pole(const std::vector<float> &_a,const std::vector<float> &_b) : a(_a),b(_b) {
  }
  T process(T in) {
    T y;
    T tmp=(in+FLT_MIN)-((a[1]*w[0]+a[2]*w[1])+(a[3]*w[2]+a[4]*w[3]));
    y=b[0]*tmp+((b[1]*w[0]+b[2]*w[1])+(b[3]*w[2]+b[4]*w[3]));
    w[3]=w[2];
    w[2]=w[1];
    w[1]=w[0];
    w[0]=tmp;
    return y;
  }
};
template<typename T>
struct Cheby1_24_BandFilter : Generic4Pole<T> {
  Cheby1_24_BandFilter() : Generic4Pole<T>({1.0000000000,-3.8585660101f,5.6015329300f,-3.6256505895f,0.8827598106f},{0.000004241295154f,0.000016965180615f,0.000025447770923f,0.000016965180615f,0.000004241295154f}) {}
};
template<typename T>
struct Cheby1_32_BandFilter : Generic4Pole<T> {
  Cheby1_32_BandFilter() : Generic4Pole<T>({1.0000000000,-3.8969977951f,5.7053982735f,-3.7190836654f,0.9107076415f},{0.000001362199230f,0.000005448796922f,0.000008173195382f,0.000005448796922f,0.000001362199230f}){}
};
template<typename T>
struct Cheby1_48_BandFilter : Generic4Pole<T> {
  Cheby1_48_BandFilter() : Generic4Pole<T>({1.0000000000,-3.9334075707f,5.8065675919f,-3.8127008493f,0.9395457323f},{2.731814697e-07f,1.092725879e-06f,1.639088818e-06f,1.092725879e-06f,2.731814697e-07f}){}
};

#endif //DBRACKMODULES_FILTER_HPP
