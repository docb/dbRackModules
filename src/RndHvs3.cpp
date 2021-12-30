#include <thread>
#include <mutex>
#include "plugin.hpp"
#include "dcb.h"
#include "rnd.h"

struct RndHvs3 : Module {
  enum ParamIds {
    DENS_PARAM,SEED_PARAM,TRIG_PARAM,DIST_PARAM,NUM_PARAMS
  };
  enum InputIds {
    X_INPUT,Y_INPUT,Z_INPUT,DIST_INPUT,SEED_INPUT,DENS_INPUT,NUM_INPUTS
  };
  enum OutputIds {
    CV_POLY_1,CV_POLY_2,NUM_OUTPUTS
  };
  enum LightIds {
    NUM_LIGHTS
  };
  enum Dists {
    DISCRETE,WEIBULL,BETA,CAUCHY,EXP,NUM_DISTS
  };
  float cube[7][7][7][32];
  dsp::BooleanTrigger trigger;
  dsp::SchmittTrigger clockTrigger;
  RND rnd;
  std::mutex mutex;
  float lastDens,lastSeed;
  int lastDist;
  bool updateSeed=false;
  bool updateDens=false;

  RndHvs3() {
    config(NUM_PARAMS,NUM_INPUTS,NUM_OUTPUTS,NUM_LIGHTS);
    configParam(DENS_PARAM,0.001f,1.f,0.5f,"DENS");
    configParam(SEED_PARAM,0.f,1.f,0.5f,"SEED");
    configParam(TRIG_PARAM,0.f,1.f,0.0f,"TRIG");
    configSwitch(DIST_PARAM,0.f,NUM_DISTS,0.f,"Distribution",{"Discrete","Weibull","Beta","Cauchy","Exp"});
    configInput(DIST_INPUT,"Distribution (0-5V)");
    configInput(SEED_INPUT,"Seed (0-10V)");
    configInput(DENS_INPUT,"Density (0-10V)");
    configInput(X_INPUT,"X");
    configInput(Y_INPUT,"Y");
    configInput(Z_INPUT,"Z");
    configOutput(CV_POLY_1,"CV 0-15");
    configOutput(CV_POLY_2,"CV 16-31");
  }

  void onAdd(const AddEvent &e) override {
    lastDens=params[DENS_PARAM].getValue();
    lastSeed=params[SEED_PARAM].getValue();
    lastDist=(int)params[DIST_PARAM].getValue();
    fillCube(lastDist,lastDens,lastSeed);
  }

  void fillCubeSave(int dist,float dens,float seed) {
    if(mutex.try_lock()) {
      fillCube(dist,dens,seed);
      mutex.unlock();
    }
  }

  void fillCube(int dist,float dens,float seed) {
    if(dens==0.0f)
      return;
    unsigned int is=(unsigned int)(seed*UINT_MAX);
    rnd.reset(is);
    switch(dist) {
      case WEIBULL: {
        for(int i=0;i<7;i++) {
          for(int j=0;j<7;j++) {
            for(int k=0;k<7;k++) {
              for(int l=0;l<32;l++) {
                //float rm = (float)(((double)rand())/RAND_MAX);
                float rm=(float)rnd.nextDouble();
                cube[i][j][k][l]=powf(-(logf(1.f-(rm*.63f))),(1.0f/dens));
              }
            }
          }
        }
        break;
      }
      case BETA: {
        for(int i=0;i<7;i++) {
          for(int j=0;j<7;j++) {
            for(int k=0;k<7;k++) {
              for(int l=0;l<32;l++) {
                cube[i][j][k][l]=(float)rnd.nextBeta(dens,50.f);
              }
            }
          }
        }
        break;
      }
      case CAUCHY: {
        for(int i=0;i<7;i++) {
          for(int j=0;j<7;j++) {
            for(int k=0;k<7;k++) {
              for(int l=0;l<32;l++) {
                cube[i][j][k][l]=(float)rnd.nextCauchy(1.f-dens);
              }
            }
          }
        }
        break;
      }
      case EXP: {
        for(int i=0;i<7;i++) {
          for(int j=0;j<7;j++) {
            for(int k=0;k<7;k++) {
              for(int l=0;l<32;l++) {
                cube[i][j][k][l]=(float)rnd.nextExp(1-(dens*.05f));
              }
            }
          }
        }
        break;
      }
      case DISCRETE:
      default: {
        for(int i=0;i<7;i++) {
          for(int j=0;j<7;j++) {
            for(int k=0;k<7;k++) {
              for(int l=0;l<32;l++) {
                float rm=(float)rnd.nextDouble();
                cube[i][j][k][l]=rm<(dens/10.0);
              }
            }
          }
        }
      }
    }
  }

  void fillValues(float xin,float yin,float zin,float *values) {
    float x=xin/10.f*6;
    float y=yin/10.f*6;
    float z=zin/10.f*6;
    auto posX=(int32_t)x;
    auto posY=(int32_t)y;
    auto posZ=(int32_t)z;


    float fracX=x-posX;
    float fracY=y-posY;
    float fracZ=z-posZ;

    int32_t j;
    for(j=0;j<32;j++) {
      float val1=cube[posX][posY][posZ][j];
      float val2=cube[posX+1][posY][posZ][j];
      float val3=cube[posX][posY+1][posZ][j];
      float val4=cube[posX+1][posY+1][posZ][j];

      float valX1=(1-fracX)*val1+fracX*val2;
      float valX2=(1-fracX)*val3+fracX*val4;
      float valY1=(1-fracY)*valX1+fracY*valX2;

      float valY2;

      val1=cube[posX][posY][posZ+1][j];
      val2=cube[posX+1][posY][posZ+1][j];
      val3=cube[posX][posY+1][posZ+1][j];
      val4=cube[posX+1][posY+1][posZ+1][j];
      valX1=(1-fracX)*val1+fracX*val2;
      valX2=(1-fracX)*val3+fracX*val4;
      valY2=(1-fracY)*valX1+fracY*valX2;

      values[j]=(1-fracZ)*valY1+fracZ*valY2;

    }
  }

  bool changed(int dist,float dens,float seed) {
    return dist!=lastDist||dens!=lastDens||seed!=lastSeed;
  }
  bool mustUpdateCube() {
    bool mustUpdate = false;
    int dist;
    float seed;
    float dens;
    if(inputs[DIST_PARAM].isConnected()) {
      dist = clamp(inputs[DIST_PARAM].getVoltage(),0.f, float(NUM_DISTS)-0.01f);
      mustUpdate = (dist != lastDist);
    } else {
      dist = params[DIST_PARAM].getValue();
      mustUpdate = (dist != lastDist);
    }
    if(inputs[DENS_INPUT].isConnected()) {
      dens = clamp(inputs[DENS_INPUT].getVoltage(),0.f, 10.f)/10.f;
      mustUpdate = mustUpdate || (dens != lastDens);
    } else {
      if(updateDens) {
        dens = params[DENS_PARAM].getValue();
        mustUpdate = true;
      } else {
        dens = lastDens;
      }
    }
    if(inputs[SEED_INPUT].isConnected()) {
      seed = clamp(inputs[SEED_INPUT].getVoltage(),0.f, 10.f)/10.f;
      mustUpdate = mustUpdate || (seed != lastSeed);
    } else {
      if(updateSeed) {
        seed = params[SEED_PARAM].getValue();
        mustUpdate = true;
      } else {
        seed = lastSeed;
      }
    }
    if(mustUpdate) {
      lastDist = dist;
      lastSeed = seed;
      lastDens = dens;
      updateSeed = false;
      updateDens = false;
    }
    return mustUpdate;
  }
  void process(const ProcessArgs &args) override {

    if(mustUpdateCube()) {
      INFO("refilling cube %d %f %f",lastDist,lastDens,lastSeed);
      std::thread t1(&RndHvs3::fillCubeSave,this,lastDist,lastDens,lastSeed);
      t1.detach();
    }

    float x=simd::clamp(inputs[X_INPUT].getVoltage(),0.f,9.99999f);
    float y=simd::clamp(inputs[Y_INPUT].getVoltage(),0.f,9.99999f);
    float z=simd::clamp(inputs[Z_INPUT].getVoltage(),0.f,9.99999f);
    float values[32];
    fillValues(x,y,z,values);

    if(outputs[CV_POLY_1].isConnected()) {
      for(int k=0;k<16;k++) {
        outputs[CV_POLY_1].setVoltage(values[k]*10.f,k);
      }
    }
    if(outputs[CV_POLY_2].isConnected()) {
      for(int k=16;k<32;k++) {
        outputs[CV_POLY_2].setVoltage(values[k]*10.f,k-16);
      }
    }
    outputs[CV_POLY_1].setChannels(16);
    outputs[CV_POLY_2].setChannels(16);

  }
};


struct RndHvs3Widget : ModuleWidget {
  RndHvs3Widget(RndHvs3 *module) {
    setModule(module);
    setPanel(APP->window->loadSvg(asset::plugin(pluginInstance,"res/RndHvs3.svg")));

    addChild(createWidget<ScrewSilver>(Vec(RACK_GRID_WIDTH,0)));
    addChild(createWidget<ScrewSilver>(Vec(box.size.x-2*RACK_GRID_WIDTH,0)));
    addChild(createWidget<ScrewSilver>(Vec(RACK_GRID_WIDTH,RACK_GRID_HEIGHT-RACK_GRID_WIDTH)));
    addChild(createWidget<ScrewSilver>(Vec(box.size.x-2*RACK_GRID_WIDTH,RACK_GRID_HEIGHT-RACK_GRID_WIDTH)));

    auto selectParam=createParam<SelectParam>(Vec(10,68),module,RndHvs3::DIST_PARAM);
    selectParam->box.size=Vec(40,60);
    selectParam->init({"Discr","Weib","Beta","Cauchy","Exp"});
    addParam(selectParam);
    auto densParam=createParam<UpdateOnReleaseKnob>(Vec(10,154),module,RndHvs3::DENS_PARAM);
    if(module) {
      densParam->update=&module->updateDens;
    }
    addParam(densParam);
    auto seedParam=createParam<UpdateOnReleaseKnob>(Vec(10,196),module,RndHvs3::SEED_PARAM);
    if(module) {
      seedParam->update=&module->updateSeed;
    }
    addParam(seedParam);

    addInput(createInput<SmallPort>(Vec(60,88),module,RndHvs3::DIST_INPUT));
    addInput(createInput<SmallPort>(Vec(60,154),module,RndHvs3::DENS_INPUT));
    addInput(createInput<SmallPort>(Vec(60,195),module,RndHvs3::SEED_INPUT));

    addInput(createInput<SmallPort>(Vec(9,250),module,RndHvs3::X_INPUT));
    addInput(createInput<SmallPort>(Vec(34,250),module,RndHvs3::Y_INPUT));
    addInput(createInput<SmallPort>(Vec(59,250),module,RndHvs3::Z_INPUT));

    addOutput(createOutput<SmallPort>(Vec(16,320),module,RndHvs3::CV_POLY_1));
    addOutput(createOutput<SmallPort>(Vec(53,320),module,RndHvs3::CV_POLY_2));
  }
};


Model *modelRndHvs3=createModel<RndHvs3,RndHvs3Widget>("RndHvs3");
