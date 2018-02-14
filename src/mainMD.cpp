#include <stdio.h>

#include "potential-master.h"
#include "integrator.h"
#include "potential.h"
#include "move.h"
#include "box.h"
#include "meter.h"
#include "average.h"
#include "sfmt_more.h"

int main(int argc, char** argv) {
  int numAtoms = 500;
  double temperature = 1.0;
  double density = 1.0;
  long steps = 1000;
  bool doData = false;
  bool doHMA = false;

  Random rand;
  printf("random seed: %d\n", rand.getSeed());

  PotentialLJ plj(TRUNC_SIMPLE, 3.0);
  Box box;
  double L = pow(numAtoms/density, 1.0/3.0);
  printf("box size: %f\n", L);
  box.setBoxSize(L,L,L);
  box.boxSizeUpdated();
  box.setNumAtoms(numAtoms);
  box.initCoordinates();
  box.enableVelocities();
  /*PotentialMasterCell potentialMaster(plj, box, 3.0, 2);
  potentialMaster.init();
  int* numCells = potentialMaster.getNumCells();
  printf("cells: %d %d %d\n", numCells[0], numCells[1], numCells[2]);*/
  PotentialMaster potentialMaster(plj, box);
  IntegratorMD integrator(potentialMaster, rand, box);
  integrator.setTimeStep(0.001);
  integrator.setTemperature(temperature);
  integrator.reset();
  PotentialCallbackHMA pcHMA(box, temperature, 9.550752245164025e+00);
  printf("u: %f\n", integrator.getPotentialEnergy());
  if (doData) integrator.doSteps(steps/10);
  MeterPotentialEnergy meterPE(integrator);
  DataPump pumpPE(meterPE, 1);
  MeterFullCompute meterFull(potentialMaster);
  PotentialCallbackPressure pcp(box, temperature);
  meterFull.addCallback(&pcp);
  if (doHMA) meterFull.addCallback(&pcHMA);
  DataPump pumpFull(meterFull, 4*numAtoms);
  MeterNumAtoms meterNA(box);
  DataPump pumpNA(meterNA, 10);
  if (doData) {
    integrator.addListener(&pumpPE);
    if (doHMA) integrator.addListener(&pumpFull);
  }

  integrator.doSteps(steps);
  if (doData) {
    double* statsPE = ((Average*)pumpPE.getDataSink())->getStatistics()[0];
    statsPE[AVG_AVG] /= numAtoms;
    statsPE[AVG_ERR] /= numAtoms;
    printf("u avg: %f  err: %f  cor: %f\n", statsPE[AVG_AVG], statsPE[AVG_ERR], statsPE[AVG_ACOR]);
    if (doHMA) {
      double** statsFull = ((Average*)pumpFull.getDataSink())->getStatistics();
      double* statsP = statsFull[0];
      printf("p avg: %f  err: %f  cor: %f\n", statsP[AVG_AVG], statsP[AVG_ERR], statsP[AVG_ACOR]);
      double* statsUHMA = statsFull[1];
      printf("uHMA avg: %f  err: %f  cor: %f\n", statsUHMA[AVG_AVG]/numAtoms, statsUHMA[AVG_ERR]/numAtoms, statsUHMA[AVG_ACOR]);
      double* statsPHMA = statsFull[2];
      printf("pHMA avg: %f  err: %f  cor: %f\n", statsPHMA[AVG_AVG], statsPHMA[AVG_ERR], statsPHMA[AVG_ACOR]);
    }
  }
}

