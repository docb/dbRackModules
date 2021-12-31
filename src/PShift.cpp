#include "dcb.h"
#include "Gamma/DFT.h"
// from the pitch shift example of the gamma library
struct PitchShift {
  gam::STFT stft{4096,4096/4,0,gam::HAMMING,gam::MAG_FREQ,3};

  float process(float in,float pshift) {
    if(stft(in)) {

      enum {
        PREV_MAG=0,TEMP_MAG,TEMP_FRQ
      };

      // Compute spectral flux (L^1 norm on positive changes)
      float flux=0;
      for(unsigned k=0;k<stft.numBins();++k) {
        float mcurr=stft.bin(k)[0];
        float mprev=stft.aux(PREV_MAG)[k];

        if(mcurr>mprev) {
          flux+=mcurr-mprev;
        }
      }

      //printf("%g\n", flux);
      //gam::printPlot(flux); printf("\n");

      // Store magnitudes for next frame
      stft.copyBinsToAux(0,PREV_MAG);

      // Given an onset, we would like the phases of the output frame
      // to match the input frame in order to preserve transients.
      if(flux>0.2) {
        stft.resetPhases();
      }

      // Initialize buffers to store pitch-shifted spectrum
      for(unsigned k=0;k<stft.numBins();++k) {
        stft.aux(TEMP_MAG)[k]=0.;
        stft.aux(TEMP_FRQ)[k]=k*stft.binFreq();
      }

      // Perform the pitch shift:
      // Here we contract or expand the bins. For overlapping bins,
      // we simply add the magnitudes and replace the frequency.
      // Reference:
      // http://oldsite.dspdimension.com/dspdimension.com/src/smbPitchShift.cpp
      if(pshift>0) {
        unsigned kmax=stft.numBins()/pshift;
        if(kmax>=stft.numBins())
          kmax=stft.numBins()-1;
        for(unsigned k=1;k<kmax;++k) {
          unsigned j=k*pshift;
          stft.aux(TEMP_MAG)[j]+=stft.bin(k)[0];
          stft.aux(TEMP_FRQ)[j]=stft.bin(k)[1]*pshift;
        }
      }

      // Copy pitch-shifted spectrum over to bins
      stft.copyAuxToBins(TEMP_MAG,0);
      stft.copyAuxToBins(TEMP_FRQ,1);
    }

    return stft();

  }
};

struct PShift : Module {
  enum ParamId {
    P_SHIFT,PARAMS_LEN
  };
  enum InputId {
    L_INPUT,R_INPUT,INPUTS_LEN
  };
  enum OutputId {
    L_OUTPUT,R_OUTPUT,OUTPUTS_LEN
  };
  enum LightId {
    LIGHTS_LEN
  };
  PitchShift L;
  PitchShift R;


  PShift() {
    config(PARAMS_LEN,INPUTS_LEN,OUTPUTS_LEN,LIGHTS_LEN);
    configParam(P_SHIFT,0.0f,4.f,0.f,"Shift");
    configOutput(L_OUTPUT,"Left");
    configOutput(R_OUTPUT,"Right");
    configInput(L_INPUT,"Left");
    configInput(R_INPUT,"Right");
    configBypass(R_INPUT,R_OUTPUT);
    configBypass(L_INPUT,L_OUTPUT);
  }

  void process(const ProcessArgs &args) override {
    if(inputs[L_INPUT].isConnected()) {
      float shift=params[P_SHIFT].getValue();
      float inL=inputs[L_INPUT].getVoltage();
      float inR=inL;
      if(inputs[R_INPUT].isConnected()) {
        inR=inputs[R_INPUT].getVoltage();
      }
      float outL,outR;
      if(shift>0.f) {
        outR=R.process(inR,shift);
        outL=L.process(inL,shift);
      } else {
        outL=inL;
        outR=inR;
      }
      outputs[L_OUTPUT].setVoltage(outL);
      outputs[R_OUTPUT].setVoltage(outR);
    }
  }
};


struct PShiftWidget : ModuleWidget {
  PShiftWidget(PShift *module) {
    setModule(module);
    setPanel(createPanel(asset::plugin(pluginInstance,"res/PShift.svg")));

    addChild(createWidget<ScrewSilver>(Vec(RACK_GRID_WIDTH,0)));
    addChild(createWidget<ScrewSilver>(Vec(box.size.x-2*RACK_GRID_WIDTH,0)));
    addChild(createWidget<ScrewSilver>(Vec(RACK_GRID_WIDTH,RACK_GRID_HEIGHT-RACK_GRID_WIDTH)));
    addChild(createWidget<ScrewSilver>(Vec(box.size.x-2*RACK_GRID_WIDTH,RACK_GRID_HEIGHT-RACK_GRID_WIDTH)));

    addParam(createParam<TrimbotWhite>(mm2px(Vec(2,MHEIGHT-79.f)),module,PShift::P_SHIFT));

    addInput(createInput<SmallPort>(mm2px(Vec(1.9f,MHEIGHT-55.f)),module,PShift::L_INPUT));
    addInput(createInput<SmallPort>(mm2px(Vec(1.9f,MHEIGHT-43.f)),module,PShift::R_INPUT));
    addOutput(createOutput<SmallPort>(mm2px(Vec(1.9f,MHEIGHT-31.f)),module,PShift::L_OUTPUT));
    addOutput(createOutput<SmallPort>(mm2px(Vec(1.9f,MHEIGHT-19.f)),module,PShift::R_OUTPUT));

  }
};


Model *modelPShift=createModel<PShift,PShiftWidget>("PShift");