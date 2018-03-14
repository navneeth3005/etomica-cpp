#include <stdio.h>

#include "potential-master.h"
#include "integrator.h"
#include "potential.h"
#include "move-virial.h"
#include "box.h"
#include "meter-virial.h"
#include "data-sink.h"
#include "random.h"
#include "util.h"
#include "cluster.h"

int main(int argc, char** argv) {
  int order = 4;
  double temperature = 1.0;
  long steps = 1000000;
  int numAlpha = 4;
  double alphaSpan = 1;
  double alpha0 = .03;

  Random rand;
  printf("random seed: %d\n", rand.getSeed());

  PotentialLJ plj(1,1,TRUNC_NONE, 1.0/0.0);
  PotentialHS pHS(1.5);
  SpeciesList speciesList;
  speciesList.add(new SpeciesSimple(1,1));

  Box refBox(speciesList);
  refBox.setBoxSize(1,1,1);
  refBox.setNumMolecules(0, order);
  PotentialMasterVirial refPotentialMasterLJ(speciesList, refBox);
  refPotentialMasterLJ.setPairPotential(0, 0, &plj);
  PotentialMasterVirial refPotentialMasterHS(speciesList, refBox);
  refPotentialMasterHS.setPairPotential(0, 0, &pHS);
  IntegratorMC refIntegrator(refPotentialMasterHS, rand);
  ClusterVirial refClusterLJ(refPotentialMasterLJ, temperature, 0, false);
  ClusterChain refClusterHS(refPotentialMasterHS, temperature, 1.0, 0);
  MCMoveChainVirial refMove(refBox, refPotentialMasterHS, rand, 1.5);
  refIntegrator.addMove(&refMove, 1);
  refIntegrator.setTemperature(temperature);
  refIntegrator.reset();
  MeterVirialOverlap refMeter(refClusterHS, refClusterLJ, alpha0, alphaSpan, numAlpha);
  Average refAverage(numAlpha, 1, 0, false);
  DataPump refPumpVirial(refMeter, 1);
  refIntegrator.addListener(&refPumpVirial);

  Box targetBox(speciesList);
  targetBox.setBoxSize(1,1,1);
  targetBox.setNumMolecules(0, order);
  PotentialMasterVirial targetPotentialMasterLJ(speciesList, targetBox);
  targetPotentialMasterLJ.setPairPotential(0, 0, &plj);
  PotentialMasterVirial targetPotentialMasterHS(speciesList, targetBox);
  targetPotentialMasterHS.setPairPotential(0, 0, &pHS);
  IntegratorMC targetIntegrator(targetPotentialMasterLJ, rand);
  ClusterVirial targetClusterLJ(targetPotentialMasterLJ, temperature, 0, true);
  ClusterChain targetClusterHS(targetPotentialMasterHS, temperature, 1, 0);
  MCMoveDisplacementVirial targetMove(targetBox, targetPotentialMasterLJ, rand, 0.2, targetClusterLJ);
  targetIntegrator.addMove(&targetMove, 1);
  targetIntegrator.setTemperature(temperature);
  targetIntegrator.reset();
  MeterVirialOverlap targetMeter(targetClusterLJ, targetClusterHS, 1/alpha0, -alphaSpan, numAlpha);
  DataPump targetPumpVirial(targetMeter, 1);
  targetIntegrator.addListener(&targetPumpVirial);

  targetIntegrator.doSteps(steps/10);
  double t1 = getTime();
  printf("reference\n");
  refIntegrator.doSteps(steps);
  printf("target\n");
  targetIntegrator.doSteps(steps);
  double t2 = getTime();

  double** refStats = ((Average*)refPumpVirial.getDataSink(0))->getStatistics();
  double** targetStats = ((Average*)targetPumpVirial.getDataSink(0))->getStatistics();
  const double* alpha = refMeter.getAlpha();
  for (int i=0; i<numAlpha; i++) {
    printf("alpha: %e\n", alpha[i]);
    printf("  ref    avg: %f   err: %f\n", refStats[i][AVG_AVG], refStats[i][AVG_ERR]);
    printf("  target avg: %f   err: %f   cor: %f\n", targetStats[i][AVG_AVG], targetStats[i][AVG_ERR], targetStats[i][AVG_ACOR]);
    printf("  ratio  avg: %f\n", refStats[i][AVG_AVG]/targetStats[i][AVG_AVG]);
  }
  printf("time: %4.3f\n", t2-t1);
}
