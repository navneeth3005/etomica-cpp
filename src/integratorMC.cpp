#include "integrator.h"

IntegratorMC::IntegratorMC(PotentialMaster& p, Random& r) : Integrator(p), random(r), pMoveSum(0), lastMove(NULL) {
}

IntegratorMC::~IntegratorMC(){}

void IntegratorMC::addMove(MCMove* move, double prob) {
  moves.push_back(move);
  moveProbabilities.push_back(prob);
  pMoveSum += prob;
}

void IntegratorMC::setTuning(bool doTune) {
  for (vector<MCMove*>::iterator it = moves.begin(); it!=moves.end(); it++) {
    (*it)->tunable = doTune;
  }
}

void IntegratorMC::doStep() {
  stepCount++;
  MCMove* m = nullptr;
  int nm = moves.size();
  if (nm>1) {
    double r = random.nextDouble32()*pMoveSum;
    double s = 0;
    for (int i=0; i<nm; i++) {
      s += moveProbabilities[i];
      if (s >= r) {
        m = moves[i];
        break;
      }
    }
  }
  else {
    m = moves[0];
  }
  lastMove = m;
  bool success = m->doTrial();
  double chi = success ? m->getChi(temperature) : 0;
  if (chi==0 || (chi<1 && chi<random.nextDouble())) {
    //printf("chi %e 0\n", chi);
    m->rejectNotify();
  }
  else {
    m->acceptNotify();
    double du = m->energyChange();
    energy += du;
    //printf("chi %e 1 %f\n", chi, du);
  }
#ifdef DEBUG
  if (fabs(energy-potentialMaster.uTotalFromAtoms()) > 1e-6) {
    printf("uAtoms! %d: %e %e %e\n", stepCount, energy, potentialMaster.uTotalFromAtoms(), energy-potentialMaster.uTotalFromAtoms());
    exit(0);
  }
  if (stepCount%100==0) {
    double oldEnergy = energy;
    reset();
    if (fabs(oldEnergy-energy) > 1e-7) printf("%d: %e %e %e\n", stepCount, oldEnergy, energy, oldEnergy-energy);
  }
#endif
  for (vector<IntegratorListener*>::iterator it = listeners.begin(); it!=listeners.end(); it++) {
    (*it)->stepFinished();
  }
}

MCMove* IntegratorMC::getLastMove() {
  return lastMove;
}
