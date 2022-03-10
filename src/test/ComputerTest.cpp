//
// Created by docb on 3/9/22.
//
#include <time.h>
#include "simd/functions.hpp"
using namespace rack;
#define TWOPIF 6.2831853f

#include "../Computer.hpp"
using rack::simd::float_4;
static double seconds()
{
  struct timespec ts;
  int x = clock_gettime(CLOCK_THREAD_CPUTIME_ID, &ts);
  assert(x == 0);
  (void) x;

  return double(ts.tv_sec) + double(ts.tv_nsec) / (1000.0 * 1000.0 * 1000.0);
}
void run(Computer<float>& computer,int terrain, int curve) {
  int genParam[4] = {terrain,-1,-1,-1};
  float cx = 0.f;
  float cy = 0.f;
  float rot = 0.5f;
  float rx = 0.5f;
  float ry = 0.5f;
  float curveParam = 1.5f;
  float xc,yc;
  float phase = 0.f;
  float dPhase = {0.00001f};
  float phs=TWOPIF;
  double secs = seconds();
  for(int k=0;k<100000;k++) {
    computer.crv(curve,phase,cx,cy,rx,ry,curveParam,xc,yc);
    computer.rotate_point(cx,cy,rot,xc,yc);
    computer.genomFunc(genParam,4,xc,yc);
    phase=simd::fmod(phase+dPhase,phs);
  }
  double diff = seconds()-secs;
  printf("terrain %d curve %d %lf\n",terrain,curve,diff);
}

void run(Computer<float_4>& computer,int terrain, int curve) {
  int genParam[4] = {terrain,-1,-1,-1};
  float_4 cx = 0.f;
  float_4 cy = 0.f;
  float_4 rot = 0.5f;
  float_4 rx = 0.5f;
  float_4 ry = 0.5f;
  float_4 curveParam = 1.5f;
  float_4 xc,yc;
  float_4 phase = 0.f;
  float_4 dPhase = {0.00001f,0.00002f,0.00003f,0.00004f};
  float phs=TWOPIF;
  double secs = seconds();
  for(int k=0;k<100000;k++) {
    computer.crv(curve,phase,cx,cy,rx,ry,curveParam,xc,yc);
    computer.rotate_point(cx,cy,rot,xc,yc);
    computer.genomFunc(genParam,4,xc,yc);
    phase=simd::fmod(phase+dPhase,phs);
  }
  double diff = seconds()-secs;
  printf("terrain %d curve %d %lf\n",terrain,curve,diff);
}

int main(int args,char **argv) {
  Computer<float> computer;
  for(int k=0;k<Computer<float_4>::NUM_TERRAINS;k++) {
    for(int j=0;j<Computer<float_4>::NUM_CURVES;j++) {
      run(computer,k,j);
    }
  }
}