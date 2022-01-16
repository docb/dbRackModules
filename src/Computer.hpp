//
// Created by docb on 12/24/21.
//

#ifndef DOCB_VCV_COMPUTER_HPP
#define DOCB_VCV_COMPUTER_HPP
#include "rnd.h"
using simd::float_4;

// =============================================================================
//    SIMPLEX NOISE
//    The simplex noise algorithm is based on code commited by Stefan Gustavson
//    to the public domain.
// =============================================================================

struct Grad {
  double x;
  double y;
  double z;

  double dot(double _x,double _y) {
    return x*_x+y*_y;
  }
};

struct SimplexNoise {
  int p[256];
  int perm[512];
  int permMod12[512];
  double F2,G2;
  Grad grad3[12]={{1, 1, 0},
                  {-1,1, 0},
                  {1, -1,0},
                  {-1,-1,0},
                  {1, 0, 1},
                  {-1,0, 1},
                  {1, 0, -1},
                  {-1,0, -1},
                  {0, 1, 1},
                  {0, -1,1},
                  {0, 1, -1},
                  {0, -1,-1}};

  SimplexNoise() {
    srand(12345678);
    for(int i=0;i<256;++i) {
      this->p[i]=floor((rand()%256+1));
    }
    for(int i=0;i<512;++i) {
      this->perm[i]=p[i&255];
      this->permMod12[i]=perm[i]%12;
    }
    F2=0.5*(sqrt(3.0)-1.0);
    G2=(3.0-sqrt(3.0))/6.0;
  }

  double noise(double xin,double yin) {
    double n0,n1,n2; // Noise contributions from the three corners
    // Skew the input space to determine which simplex cell we're in
    double s=(xin+yin)*F2; // Hairy factor for 2D
    int i=floor(xin+s);
    int j=floor(yin+s);
    double t=(i+j)*G2;
    double X0=i-t; // Unskew the cell origin back to (x,y) space
    double Y0=j-t;
    double x0=xin-X0; // The x,y distances from the cell origin
    double y0=yin-Y0;
    // For the 2D case, the simplex shape is an equilateral triangle.
    // Determine which simplex we are in.
    int i1,j1; // Offsets for second (middle) corner of simplex in (i,j) coords
    if(x0>y0) {
      i1=1;
      j1=0;
    } // lower triangle, XY order: (0,0)->(1,0)->(1,1)
    else {
      i1=0;
      j1=1;
    }      // upper triangle, YX order: (0,0)->(0,1)->(1,1)
    // A step of (1,0) in (i,j) means a step of (1-c,-c) in (x,y), and
    // a step of (0,1) in (i,j) means a step of (-c,1-c) in (x,y), where
    // c = (3-sqrt(3))/6
    double x1=x0-i1+G2; // Offsets for middle corner in (x,y) unskewed coords
    double y1=y0-j1+G2;
    double x2=x0-1.0+2.0*G2; // Offsets for last corner in (x,y) unskewed coords
    double y2=y0-1.0+2.0*G2;
    // Work out the hashed gradient indices of the three simplex corners
    int ii=i&255;
    int jj=j&255;
    int gi0=permMod12[ii+perm[jj]];
    int gi1=permMod12[ii+i1+perm[jj+j1]];
    int gi2=permMod12[ii+1+perm[jj+1]];
    // Calculate the contribution from the three corners
    double t0=0.5-x0*x0-y0*y0;
    if(t0<0)
      n0=0.0;
    else {
      t0*=t0;
      n0=t0*t0*grad3[gi0].dot(x0,y0);  // (x,y) of grad3 used for 2D gradient
    }
    double t1=0.5-x1*x1-y1*y1;
    if(t1<0)
      n1=0.0;
    else {
      t1*=t1;
      n1=t1*t1*grad3[gi1].dot(x1,y1);
    }
    double t2=0.5-x2*x2-y2*y2;
    if(t2<0)
      n2=0.0;
    else {
      t2*=t2;
      n2=t2*t2*grad3[gi2].dot(x2,y2);
    }
    // Add contributions from each corner to get the final noise value.
    // The result is scaled to return values in the interval [-1,1].
    return 70.0*(n0+n1+n2);
  }

};

#define PSIZE 256
#define PCOUNT 6

struct ValueNoise {
  float points[PCOUNT][PSIZE][PSIZE];
  uint64_t state;

  void dseed(uint64_t seed) {
    state=seed;
  }

  uint64_t rnd() {
    uint64_t s1=1103515245*state+12345;
    state=s1%0x80000000;
    return state;
  }

  ValueNoise() {
    for(int n=0;n<PCOUNT;n++) {
      dseed(n+10);
      for(int k=0;k<PSIZE;k++) {
        for(int i=0;i<PSIZE;i++) {
          if(i==0||i==PSIZE||k==0||k==PSIZE)
            points[n][k][i]=0;
          else {
            float rm=((float)rnd())/2147483647.f;
            switch(n) {
              case 1:
                points[n][k][i]=rm>0.5?0:1;
                break;
              case 2:
                points[n][k][i]=rm>0.7?0:1;
                break;
              case 3:
                points[n][k][i]=rm>0.9?0:1;
                break;
              case 4:
                points[n][k][i]=float(2*tan(atan(5)*(rm*2-1))*0.1);
                break;
              case 5:
                points[n][k][i]=float(sin(M_PI*(rm-0.5)*0.5)/(sin(1.5707963*0.5)));
                break;
              default:
                points[n][k][i]=rm;
            }
          }
        }
      }

    }
  }

  float getp(int n,int x,int y) {
    if(n<0)
      n=0;
    if(n>=PCOUNT)
      n=PCOUNT-1;
    if(x>=PSIZE)
      x=PSIZE-1;
    if(y>=PSIZE)
      y=PSIZE-1;
    if(x<0)
      x=0;
    if(y<0)
      y=0;
    return points[n][x][y];
  }

  inline float lerp(float lo,float hi,float t) {
    return lo*(1-t)+hi*t;
  }

  float interp(int n,float x,float y) {
    float xs=x*8;
    float ys=y*8;
    int xi=(int)floorf(xs);
    int yi=(int)floorf(ys);

    float tx=xs-(float)xi;
    float ty=ys-(float)yi;

    int rx0=PSIZE/2+xi;
    int rx1=rx0+1;
    int ry0=PSIZE/2+yi;
    int ry1=ry0+1;

    // random values at the corners of the cell using permutation table
    float c00=getp(n,ry0,rx0);
    float c10=getp(n,ry0,rx1);
    float c01=getp(n,ry1,rx0);
    float c11=getp(n,ry1,rx1);

    // linearly interpolate values along the x axis
    float nx0=lerp(c00,c10,tx);
    float nx1=lerp(c01,c11,tx);

    // linearly interpolate the nx0/nx1 along they y axis
    float ret=lerp(nx0,nx1,ty);
    return ret;
  }
};

template<typename T>
struct Computer {
  float table[6][65536]={};
  const float od2p=1/(2*M_PI);
  const float mpih=M_PI/2;
  SimplexNoise simplex;
  ValueNoise noise;

  Computer() {
    genTable(0,{1.f});//SIN
    genTable(1,{1.f,0.5f,1/3.f,1/4.f,1/5.f,1/6.f,1/7.f,1/8.f,1/9.f,1/10.f,1/11.f});//SAW
    genTable(2,{1.f,0.f,1/3.f,0.f,1/5.f,0.f,1/7.f,0.f,1/9.f,0.f,1/11.f}); //PULSE
    genTable(3,{1,0.5f,0.f,0.25f,0.f,1/6.f,0.f,1/8.f,0.f,1/10.f});//TRI
  }

  void genTable(int tblNr,std::initializer_list<float> list) {
    for(int i=0;i<65536;i++) {
      float val=0.f;
      int k=1;
      for(float elem:list) {
        if(elem!=0.f)
          val+=(float)(elem*sin(k*2*M_PI*i/65536.0));
        k++;
      }
      table[tblNr][i]=val;
    }
  }

  float_4 lookup(float_4 tphs,int tblNr) {
    float_4 sign=simd::ifelse(tphs>=0.f,1.f,-1.f);
    float_4 phs=tphs*od2p*65536.f;
    float v[4];
    for(int k=0;k<4;k++) {
      v[k]=sign[k]*table[tblNr][((int)sign[k]*(int)phs[k])&0xFFFF];
    }
    return {v[0],v[1],v[2],v[3]};
  }

  float lookup(float fphs,int tblNr) {
    int sign=fphs>=0?1:-1;
    int phs=(int)(fphs*od2p*65536);
    return (float)sign*table[tblNr][(sign*phs)&0xFFFF];
  }

  double fastPrecisePow(double a, const double b) {
    // calculate approximation with just the fraction of the exponent
    int exp = (int) b;
    union {
      double d;
      int x[2];
    } u = { a };
    u.x[1] = (int)((b - exp) * (u.x[1] - 1072632447) + 1072632447);
    u.x[0] = 0;

    // exponentiation by squaring with the exponent's integer part
    // double r = u.d makes everything much slower, not sure why
    double r = 1.0;
    while (exp) {
      if (exp & 1) {
        r *= a;
      }
      a *= a;
      exp >>= 1;
    }

    return r * u.d;
  }

  double fastPow(double a,double b) {
    union {
      double d;
      int x[2];
    }u={a};
    u.x[1]=(int)(b*(u.x[1]-1072632447)+1072632447);
    u.x[0]=0;
    return u.d;
  }

  float fpow(float a,float b) {
    return powf(a,b);
  }

  float_4 fpow(float_4 a,float_4 b) {
    float v[4];
    for(int k=0;k<4;k++) {
      v[k]=fastPow(a[k],b[k]);
    }
    return {v[0],v[1],v[2],v[3]};
  }

  T sinl(T v) {
    return lookup(v,0);
  }

  T cosl(T v) {
    return lookup(mpih-v,0);
  }

  T pls(T v) {
    return lookup(v,2);
  }

  T tri(T v) {
    return lookup(v,3);
  }

  T saw(T v) {
    return lookup(v,1);
  }

  T n(T v) {
    return simd::ifelse(v==0.f,0.000001f,v);
  }

  T sqr(T v) {
    return simd::ifelse(v<0.f,-simd::sqrt(-v),simd::sqrt(v));
  }

  enum Terrains {
    SINX,COSY,SINXPY,COSXMY,SINX2,COSY2,SINXCOSY,COSYSINX,SINSQR,SIMPLEX,VALUE_NOISE_UNI,VALUE_NOISE_DISCRETE_05,VALUE_NOISE_DISCRETE_07,VALUE_NOISE_DISCRETE_09,VALUE_NOISE_DIST1,VALUE_NOISE_DIST2,COS_DIV,TRIADD,SAWADD,PLSADD,TRI2,SAW2,PLS2,TANTER,SINSQRTANH,NUM_TERRAINS
  };
  float_4 vnoise(int n,float_4 x,float_4 y) {
    float v[4];
    for(int k=0;k<4;k++) {
      v[k]=noise.interp(n,x[k],y[k]);
    }
    return {v[0],v[1],v[2],v[3]};

  }

  float vnoise(int n,float x,float y) {
    return noise.interp(n,x,y);
  }

  float_4 snoise(float_4 x,float_4 y) {
    float v[4];
    for(int k=0;k<4;k++) {
      v[k]=(float)simplex.noise(x[k],y[k]);
    }
    return {v[0],v[1],v[2],v[3]};
  }

  float snoise(float x,float y) {
    return (float)simplex.noise(x,y);
  }

  T simplexNoise(T value,T x,T y) {
    return value*snoise(x,y);
  }

  T valueNoise(int n,T value,T x,T y) {
    return value*vnoise(n,x,y);
  }

  T sinxpy(T value,T x,T y) {
    return value*sinl(4*(x+y));
  }
  T cosxmy(T value,T x,T y) {
    return value*cosl(4*(x-y));
  }

  T cosdiv(T value,T x,T y) {
    return value*cosl(x/n(y));
  }

  T tanter(T value,T x,T y) {
    return value*simd::atan(((y)+simd::tan((x+y)-sinl(x+M_PI)-sinl(x*y/M_PI)*sinl((y*x+M_PI)))));
  }

  T sinxcosy(T value,T x,T y) {
    return value*sinl(2*x*cosl(y));
  }
  T cosysinx(T value,T x,T y) {
    return value*cosl(2*y*sinl(x));
  }

  T cosy(T value,T x,T y) {
    return value*cosl(4*y);
  }

  T sinx(T value,T x,T y) {
    return value*sinl(4*x);
  }

  T sin2(T value,T x,T y) {
    return value*sinl(x*x);
  } //C
  T cos2(T value,T x,T y) {
    return value*cosl(y*y);
  } //C

  T triadd(T value,T x,T y) {
    return value*tri(x*x+y*y);
  } //C
  T sawadd(T value,T x,T y) {
    return value*saw(x*x+y*y);
  } //C
  T plsadd(T value,T x,T y) {
    return value*pls(x*x+y*y);
  } //C
  T tri2(T value,T x,T y) {
    return value*tri(x*x)*tri(y*y);
  } //C
  T saw2(T value,T x,T y) {
    return value*saw(x*x)*saw(y*y);
  } //C
  T pls2(T value,T x,T y) {
    return value*pls(x*x)*pls(y*y);
  } //C

  T mtanh(T x) {
    T ex = simd::exp(2*x);
    return (ex-1)/(ex+1);
  }

  T genomFunc(const int *gen,int len,T x,T y) {
    T v=1;
    for(int k=0;k<len;k++) {
      int index=-1;
      if(gen[k]>=0&&gen[k]<NUM_TERRAINS) {
        index=gen[k];
      }
      switch(index) {
        case SINXPY:
          v=sinxpy(v,x,y);
          break;
        case COSXMY:
          v=cosxmy(v,x,y);
          break;
        case SIMPLEX:
          v=simplexNoise(v,x,y);
          break;
        case COSY2:
          v=cos2(v,x,y);
          break;
        case SINX2:
          v=sin2(v,x,y);
          break;
        case COSY:
          v=cosy(v,x,y);
          break;
        case SINX:
          v=sinx(v,x,y);
          break;
        case SINXCOSY:
          v=sinxcosy(v,x,y);
          break;
        case COSYSINX:
          v=cosysinx(v,x,y);
          break;
        case SINSQR:
          v=v*sinl(sqr(y+x))*x+sqr(sinl(x)*cosl(y));
          break;
        case SINSQRTANH:
          v=v*mtanh(sinl(sqr(y+x))*x+sqr(sinl(x)*cosl(y)));
          break;
        case VALUE_NOISE_DISCRETE_05:
          v=valueNoise(1,v,x,y);
          break;
        case VALUE_NOISE_DISCRETE_07:
          v=valueNoise(2,v,x,y);
          break;
        case VALUE_NOISE_DISCRETE_09:
          v=valueNoise(3,v,x,y);
          break;
        case VALUE_NOISE_DIST1:
          v=valueNoise(4,v,x,y);
          break;
        case VALUE_NOISE_DIST2:
          v=valueNoise(5,v,x,y);
          break;
        case VALUE_NOISE_UNI:
          v=valueNoise(0,v,x,y);
          break;
        case COS_DIV:
          v=cosdiv(v,x,y);
          break;
        case TANTER:
          v=tanter(v,x,y);
          break;
        case TRIADD:
          v=triadd(v,x,y);
          break;
        case SAWADD:
          v=sawadd(v,x,y);
          break;
        case PLSADD:
          v=plsadd(v,x,y);
          break;
        case TRI2:
          v=tri2(v,x,y);
          break;
        case SAW2:
          v=saw2(v,x,y);
          break;
        case PLS2:
          v=pls2(v,x,y);
          break;
      }
    }
    return v;
  }

  /************ CURVES ***************/
  void rotate_point(T cx,T cy,T angle,T &x,T &y) {
    T s=sinl(angle);
    T c=cosl(angle);
    x-=cx;
    y-=cy;
    T xnew=x*c-y*s;
    T ynew=x*s+y*c;
    x=xnew+cx;
    y=ynew+cy;
  }

  void ellipse(T t,T kx,T ky,T krx,T kry,T kparam,T &outX,T &outY) {
    T x=t+kparam*sinl(t);
    outX=kx+krx*sinl(x);
    outY=ky+kry*cosl(x);
  }

  /* the limacon curve parametrized by the kparam value
   see e.g. http://www.2dcurves.com/roulette/roulettel.html
   for kparam = 1 we have a cardioid
*/

  void limacon(T t,T kx,T ky,T krx,T kry,T kparam,T &outX,T &outY) {
    outX=kx+krx*sinl(t)*(cosl(t)+kparam);
    outY=ky+kry*cosl(t)*(cosl(t)+kparam);
  }

  /* a simple 8 */

  void lemniskateG(T t,T kx,T ky,T krx,T kry,T kparam,T &outX,T &outY) {
    T x=t+kparam*sinl(t);
    outX=kx+krx*cosl(x);
    outY=ky+kry*sinl(x)*cosl(x);
  }

  /* the cornoid curve
   see e.g. http://www.2dcurves.com/sextic/sexticco.html
*/
  void cornoid(T t,T kx,T ky,T krx,T kry,T kparam,T &outX,T &outY) {
    outX=kx+krx*cosl(t)*cosl(2*t);
    outY=ky+kry*sinl(t)*(kparam+cosl(2*t));
  }

  /* Chevas trisextix
   see e.g. http://www.2dcurves.com/sextic/sextict.html
*/
  void trisec(T t,T kx,T ky,T krx,T kry,T kparam,T &outX,T &outY) {
    outX=kx+krx*cosl(t)*(1+kparam*sinl(2*t));
    outY=ky+kry*sinl(t)*(1+kparam*sinl(2*t));
  }

  /* Scarabeus curve see e.g http://www.2dcurves.com/sextic/sexticsc.html
*/

  void scarabeus(T t,T kx,T ky,T krx,T kry,T kparam,T &outX,T &outY) {
    outX=kx+krx*cosl(t)*(kparam*sinl(2*t)+sinl(t));
    outY=ky+kry*sinl(t)*(kparam*sinl(2*t)+sinl(t));
  }

  /* folium see http://www.2dcurves.com/quartic/quarticfo.html */
  void folium(T t,T kx,T ky,T krx,T kry,T kparam,T &outX,T &outY) {
    T sint=sinl(t);
    T cost=cosl(t);
    outX=kx+krx*cost*cost*(sint*sint-kparam);
    outY=ky+kry*sint*cost*(sint*sint-kparam);
  }

  /* talbot see http://www.2dcurves.com/trig/trigta.html */
  void talbot(T t,T kx,T ky,T krx,T kry,T kparam,T &outX,T &outY) {
    T sint=sinl(t);
    T cost=cosl(t);
    outX=kx+krx*cost*(1+kparam*sint*sint);
    outY=ky+kry*sint*(1-kparam-kparam*cost*cost);
  }
  template<int A>
  void lissajous(T t,T kx,T ky,T krx,T kry,T kparam,T &outX,T &outY) {
    outX = kx+krx*sinl(t);
    outY = ky+kry*sinl(A*t + kparam);
  }
  template<int N, int P>
  void hypocycloid(T t,T kx,T ky,T krx,T kry,T kparam,T &outX,T &outY) {
    float a = float(N)/float(P);
    outX = kx+krx*((1.f-a)*cosl(a*t*float(P))+a*kparam*cosl((1-a)*t*float(P)));
    outY = ky+kry*((1.f-a)*sinl(a*t*float(P))-a*kparam*sinl((1-a)*t*float(P)));
  }

  enum Curves {
    ELLIPSE,LEMNISKATE,LIMACON,CORNOID,TRISEC,SCARABEUS,FOLIUM,TALBOT,LISSAJOUS,HYPOCYCLOID,NUM_CURVES
  };

  void crv(int curve,T t,T cx,T cy,T rx,T ry,T curveParam,T &x,T &y) {
    switch(curve) {
      case ELLIPSE:
        ellipse(t,cx,cy,rx,ry,curveParam,x,y);
        break;
      case LEMNISKATE:
        lemniskateG(t,cx,cy,rx,ry,curveParam,x,y);
        break;
      case LIMACON:
        limacon(t,cx,cy,rx,ry,curveParam,x,y);
        break;
      case CORNOID:
        cornoid(t,cx,cy,rx,ry,curveParam,x,y);
        break;
      case TRISEC:
        trisec(t,cx,cy,rx,ry,curveParam,x,y);
        break;
      case SCARABEUS:
        scarabeus(t,cx,cy,rx,ry,curveParam,x,y);
        break;
      case FOLIUM:
        folium(t,cx,cy,rx,ry,curveParam,x,y);
        break;
      case TALBOT:
        talbot(t,cx,cy,rx,ry,curveParam,x,y);
        break;
      case LISSAJOUS:
        lissajous<3>(t,cx,cy,rx,ry,curveParam,x,y);
        break;
      case HYPOCYCLOID:
        hypocycloid<2,5>(t,cx,cy,rx,ry,curveParam,x,y);
        break;
    }
  }

  void superformula(T t,T kx,T ky,T krx,T kry,T y,T z,T n1,T n2,T n3,T a,T b,T &outX,T &outY) {

    T t1=cosl(y*t/4);
    T t2=sinl(z*t/4);
    T r0=fpow(simd::fabs(t1/a),n2)+fpow(simd::fabs(t2/b),n3);
    T r=fpow(r0,-1/n1);
    outX=kx+krx*r*cosl(t);
    outY=ky+kry*r*sinl(t);
  }
};


#endif //DOCB_VCV_COMPUTER_HPP
