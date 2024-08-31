#include "plugin.hpp"


Plugin* pluginInstance;

extern Model* modelGeneticTerrain;
extern Model* modelGeneticSuperTerrain;
extern Model* modelDCBlock;
extern Model* modelRndHvs3;
extern Model* modelJTScaler;
extern Model* modelAddSynth;
extern Model* modelRndH;
extern Model* modelRndC;
extern Model* modelGenScale;
extern Model* modelJTKeys;
extern Model* modelHopa;
extern Model* modelFrac;
extern Model* modelHexSeq;
extern Model* modelInterface;
extern Model* modelHexSeqP;
extern Model* modelMVerb;
extern Model* modelPShift;
extern Model* modelFLA;
extern Model* modelFLL;
extern Model* modelGendy;
extern Model* modelRtrig;
extern Model* modelSTrig;
extern Model* modelRSC;
extern Model* modelPad;
extern Model* modelRndG;
extern Model* modelPlotter;
extern Model* modelSuperLFO;
extern Model* modelHexSeqExp;
extern Model* modelPad2;
extern Model* modelPHSR;
extern Model* modelPhS;
extern Model* modelSPL;
extern Model* modelPhO;
extern Model* modelFaders;
extern Model* modelYC;
extern Model* modelPLC;
extern Model* modelMPad2;
extern Model* modelPHSR2;
extern Model* modelEVA;
extern Model* modelRatio;
extern Model* modelCSOSC;
extern Model* modelBWF;
extern Model* modelPRB;
extern Model* modelOsc2;
extern Model* modelRndH2;
extern Model* modelOFS;
extern Model* modelOFS3;
extern Model* modelOsc1;
extern Model* modelOsc3;
extern Model* modelOsc4;
extern Model* modelOsc5;
extern Model* modelL4P;
extern Model* modelSKF;
extern Model* modelSPF;
extern Model* modelUSVF;
extern Model* modelAP;
extern Model* modelWS;
extern Model* modelAUX;
extern Model* modelCHBY;
extern Model* modelCLP;
extern Model* modelLWF;
extern Model* modelDRM;
extern Model* modelCWS;
extern Model* modelSWF;
extern Model* modelOsc6;
extern Model* modelDrums;
extern Model* modelPPD;
extern Model* modelOsc7;
extern Model* modelOsc8;
extern Model* modelOsc9;
extern Model* modelFS6;
extern Model* modelPulsar;
extern Model* modelOscA1;
extern Model* modelOscS;
extern Model* modelOscP;
extern Model* modelX16;
extern Model* modelX8;
extern Model* modelX4;
extern Model* modelX6;

void init(Plugin* p) {
	pluginInstance = p;

  p->addModel(modelGeneticTerrain);
  p->addModel(modelGeneticSuperTerrain);
  p->addModel(modelDCBlock);
  p->addModel(modelRndHvs3);
  p->addModel(modelJTScaler);
  p->addModel(modelAddSynth);
  p->addModel(modelRndH);
  p->addModel(modelRndC);
  p->addModel(modelGenScale);
  p->addModel(modelJTKeys);
  p->addModel(modelHopa);
  p->addModel(modelFrac);
  p->addModel(modelHexSeq);
	p->addModel(modelInterface);
	p->addModel(modelHexSeqP);
	p->addModel(modelMVerb);
	p->addModel(modelPShift);
	p->addModel(modelFLA);
  p->addModel(modelFLL);
	p->addModel(modelGendy);
	p->addModel(modelRtrig);
	p->addModel(modelSTrig);
	p->addModel(modelRSC);
	p->addModel(modelPad);
	p->addModel(modelRndG);
	p->addModel(modelPlotter);
	p->addModel(modelSuperLFO);
	p->addModel(modelHexSeqExp);
  p->addModel(modelPad2);
	p->addModel(modelPHSR);
	p->addModel(modelPhS);
	p->addModel(modelSPL);
	p->addModel(modelPhO);
	p->addModel(modelFaders);
	p->addModel(modelYC);
  p->addModel(modelPLC);
	p->addModel(modelMPad2);
	p->addModel(modelPHSR2);
	p->addModel(modelEVA);
	p->addModel(modelCSOSC);
	p->addModel(modelRatio);
  p->addModel(modelBWF);
	p->addModel(modelPRB);
	p->addModel(modelOsc2);
  p->addModel(modelRndH2);
	p->addModel(modelOFS);
	p->addModel(modelOFS3);
	p->addModel(modelOsc1);
  p->addModel(modelOsc3);
  p->addModel(modelOsc4);
  p->addModel(modelOsc5);
  p->addModel(modelSKF);
  p->addModel(modelSPF);
  p->addModel(modelL4P);
  p->addModel(modelUSVF);
  p->addModel(modelAP);
  p->addModel(modelAUX);
  p->addModel(modelWS);
  p->addModel(modelCHBY);
  p->addModel(modelCLP);
  p->addModel(modelLWF);
  p->addModel(modelDRM);
  p->addModel(modelCWS);
  p->addModel(modelSWF);
  p->addModel(modelOsc6);
	p->addModel(modelDrums);
  p->addModel(modelPPD);
  p->addModel(modelOsc7);
  p->addModel(modelOsc8);
  p->addModel(modelOsc9);
  p->addModel(modelFS6);
  p->addModel(modelPulsar);
  p->addModel(modelOscA1);
  p->addModel(modelOscS);
  p->addModel(modelOscP);
	p->addModel(modelX16);
  p->addModel(modelX8);
  p->addModel(modelX4);
  p->addModel(modelX6);
}
