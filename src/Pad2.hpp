//
// Created by docb on 6/21/22.
//

#ifndef DBRACKMODULES_PAD2_HPP
#define DBRACKMODULES_PAD2_HPP
#include "rnd.h"
/**
 *  padsynth algorithm from Paul Nasca
 */
#define SMOOTH_PERIOD 1024
template<size_t S>
struct Pad2Table {
  float *table[2];
  float *rndPhs;
  float currentSeed;
  int currentTable=0;
  dsp::RealFFT _realFFT;
  RND rnd;
  int counter=0;
  double lastDuration;

  Pad2Table() : _realFFT(S) {
    table[0]=new float[S];
    table[1]=new float[S];
    rndPhs=new float[S];
    reset(0.5);
  }

  ~Pad2Table() {
    delete[] table[0];
    delete[] table[1];
    delete[] rndPhs;
  }

  void reset(float pSeed) {
    rnd.reset((unsigned long long)(floor((double)pSeed*(double)ULONG_MAX)));
    for(size_t k=0;k<S;k++) {
      rndPhs[k]=float(rnd.nextDouble()*M_PI*2);
    }
    currentSeed=pSeed;
  }

  float profile(float fi,float bwi) {
    float x=fi/bwi;
    x*=x;
    if(x>14.71280603)
      return 0.0;//this avoids computing the e^(-x^2) where it's results are very close to zero
    return expf(-x)/bwi;
  };

  void generate(const std::vector<float> &partials,float sampleRate,float fundFreq,float partialBW,float bwScale,float pSeed) {
    auto input=new float[S*2];
    auto work=new float[S*2];
    memset(input,0,S*2*sizeof(float));
    input[0]=0.f;
    input[1]=0.f;
    for(unsigned int k=0;k<partials.size();k++) {
      float partialHz=fundFreq*float(k+1);
      if(partials[k]>0.f /*&& partialHz<sampleRate/2.f*/) {
        float freqIdx=partialHz/((float)sampleRate);
        float bandwidthHz=(std::pow(2.0f,partialBW/1200.0f)-1.0f)*fundFreq*std::pow(float(k+1),bwScale);
        float bandwidthSamples=bandwidthHz/(2.0f*sampleRate);
        for(unsigned i=0;i<S;i++) {
          float fttIdx=((float)i)/((float)S*2);
          float profileIdx=fttIdx-freqIdx;
          float profileSample=profile(profileIdx,bandwidthSamples);
          input[i*2]+=(profileSample*partials[k]);
        }
      }
    }
    if(currentSeed!=pSeed)
      reset(pSeed);
    for(unsigned k=0;k<S;k++) {
      auto randomPhase=rndPhs[k];
      input[k*2]=(input[k*2]*cosf(randomPhase));
      input[k*2+1]=(input[k*2]*sinf(randomPhase));
    };

    int idx=(currentTable+1)%2;
    pffft_transform_ordered(_realFFT.setup,input,table[idx],work,PFFFT_BACKWARD);
    _realFFT.scale(table[idx]);
    currentTable=idx;
    counter=SMOOTH_PERIOD;
    delete[] input;
    delete[] work;
  }

  float lookup(double phs) {
    if(counter>0) {
      float mix=float(counter)/float(SMOOTH_PERIOD);
      float future=table[currentTable][int(phs*float(S))&(S-1)];
      int lastTable=currentTable==0?1:0;
      float last=table[lastTable][int(phs*float(S))&(S-1)];
      counter--;
      return future*(1-mix)+last*mix;
    }
    return table[currentTable][int(phs*float(S))&(S-1)];
  }


};
#endif //DBRACKMODULES_PAD2_HPP
