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
}
