//
// Created by docb on 4/8/23.
//

#ifndef DBRACKMODULES_DRUMSAMPLER_HPP
#define DBRACKMODULES_DRUMSAMPLER_HPP

struct Sample {
  bool loading;
  bool loaded=false;
  unsigned int sr;
  std::vector<float> buffer;

  Sample() {
    buffer.clear();
    loading=false;
    sr=0;
  }

  ~Sample() {
    buffer.clear();
  }

  void load(float *values,size_t len) {
    loading=true;
    loaded=false;

    sr=48000;
    buffer.clear();

    for(unsigned int i=0;i<len;i++) {
      buffer.push_back(values[i]);
    }

    loading=false;
    loaded=true;
  }

  size_t size() {
    return buffer.size();
  }

  float get(unsigned index) {
    if(index>=buffer.size())
      return 0.f;
    return buffer[index];
  }

  float operator[](size_t idx) {
    return get(idx);
  }

};

struct SamplePlayer {
  std::vector <Sample> samples;
  float pos=0.0f;
  unsigned int samplePosition=0;
  bool playing=false;
  unsigned int currentSample=0;

  void addRawSample(float *data,size_t length) {
    Sample sample;
    sample.load(data,length/4);
    if(sample.loaded) {
      samples.push_back(sample);
    }
  }

  float getOutput() {
    samplePosition=int(pos);
    if((currentSample>=samples.size())||!playing||(samplePosition>=samples[currentSample].size())||!samples[currentSample].loaded)
      return 0.f;
    //linear interpolation
    if((samplePosition+1)<samples[currentSample].size()) {
      float tmp;
      float frac=modff(pos,&tmp);
      float out1,out2;
      out1=samples[currentSample].get(samplePosition);
      out2=samples[currentSample].get(samplePosition+1);
      out1=out1+((out2-out1)*frac);
      return out1;
    }
    return samples[currentSample].get(samplePosition);
  }

  void trigger() {
    pos=0;
    playing=true;
  }

  void stop() {
    playing=false;
  }

  void step(const Module::ProcessArgs &args,float voct) {
    if((currentSample<samples.size())&&playing&&samples[currentSample].loaded) {
      float sr=samples[currentSample].sr;
      if(args.sampleRate!=sr)
        voct+=log2f(sr*args.sampleTime);
      pos=pos+powf(2,voct);
      if(pos>=samples[currentSample].size())
        stop();
    }
  }

  void setCurrentSample(float in) {
    currentSample=in;
  }

  void reset() {
    playing=false;
    samples.clear();
  }
};

struct ADEnvelope {
  enum Stage {
    STAGE_OFF,STAGE_ATTACK,STAGE_DECAY
  };
  float envLinear=0.f;
  Stage stage=STAGE_OFF;
  float env=0.f;
  float attackTime=0.1;
  float decayTime=0.1;
  float attackShape=1.0;
  float decayShape=1.0;

  ADEnvelope() = default;

  void process(const float &sampleTime) {

    if(stage==STAGE_OFF) {
      env=envLinear=0.0f;
    } else if(stage==STAGE_ATTACK) {
      envLinear+=sampleTime/attackTime;
      env=std::pow(envLinear,attackShape);
    } else if(stage==STAGE_DECAY) {
      envLinear-=sampleTime/decayTime;
      env=std::pow(envLinear,decayShape);
    }

    if(envLinear>=1.0f) {
      stage=STAGE_DECAY;
      env=envLinear=1.0f;
    } else if(envLinear<=0.0f) {
      stage=STAGE_OFF;
      env=envLinear=0.0f;
    }
  }

  void trigger() {
    stage=ADEnvelope::STAGE_ATTACK;
    envLinear=std::pow(env,1.0f/attackShape);
  }

};

#endif //DBRACKMODULES_DRUMSAMPLER_HPP
