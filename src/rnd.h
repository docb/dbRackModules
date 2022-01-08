#ifndef RND_H
#define RND_H

#include "cmath"
#include <ctime>

class RND {
public:

  explicit RND(unsigned long long rseed=0) {
    reset(rseed);
  }

  void reset() {
    state=seed;
  }

  void reset(unsigned long long rseed);

  unsigned long next() {
    state=(a*state+c)%m;
    unsigned long ret=state>>16;
    return ret;
  }

  double nextDouble() {
    return (double)next()/(double)(m>>16);
  }

  bool nextCoin(float thresh = 0.5f) {
    return nextDouble() > thresh;
  }

  int nextRange(int from, int to) {
    if(from==to) return from;
    if(from<to) {
      return from+int(round(nextDouble()*(to-from)));
    } else {
      return to+int(round(nextDouble()*(from-to)));
    }
  }

  double nextBeta(double _a,double b) {
    double r1,r2;
    while(true) {
      double r;
      do {
        r=nextDouble();
      } while(r==0.0);
      r1=pow(r,1/_a);
      do {
        r=nextDouble();
      } while(r==0.0);
      r2=r1+pow(r,1/b);
      if(r2>=1)
        break;
    }
    return r1/r2;
  }

  double nextWeibull(double dens) {
    if(dens<=1) return nextDouble();
    return pow(-(log(1.f-(nextDouble() * .63))), (dens));
  }

  double nextTri(int count) {
    if(count<2) return nextDouble();
    double z=0;
    for(int j=0;j<count;j++) {
      z+=nextDouble();
    }
    return z/count;
  }

  double nextMin(int count) {
    if(count<2) return nextDouble();
    double r=2;
    for(int j=0;j<count;j++) {
      double z=nextDouble();
      if(z<r)
        r=z;
    }
    return r;
  }

  double nextMax(int count) {
    if(count<2) return nextDouble();
    double r=0;
    for(int j=0;j<count;j++) {
      double z=nextDouble();
      if(z>r)
        r=z;
    }
    return r;
  }
  double nextCauchy(double _a) {
    if (_a > 1.0)
      _a = 1.0;
    else if (_a < 0.0001)
      _a = 0.0001;
    double rnd = nextDouble();
    return (1/_a)*tan(atan(10*_a)*(rnd))*0.1;
  }
  double nextExp(double _a) {
    if (_a > 1.0)
      _a = 1.0;
    else if (_a < 0.0001)
      _a = 0.0001;
    double _c=log(1.0-(0.999*_a));
    double r=nextDouble()*0.999*_a;
    return (log(1.0-r)/_c);
  }
private:
  unsigned long long state;
  unsigned long long seed;
  unsigned long long a=25214903917;
  unsigned long long c=11;
  unsigned long long m=0x0001000000000000ULL;
};

#endif // RND_H
