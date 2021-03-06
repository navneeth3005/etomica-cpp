/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "move-virial.h"

MCMoveChainVirial::MCMoveChainVirial(Box& b, PotentialMaster& p, Random& r, double s) : MCMove(b,p,r,1), sigma(s) {
  tunable = false;
}

MCMoveChainVirial::~MCMoveChainVirial() {}

bool MCMoveChainVirial::doTrial() {
  int na = box.getNumAtoms();
  if (na<=1) {
    fprintf(stderr, "Gotta give me more than 1 atom!\n");
    abort();
  }
  double* rPrev = box.getAtomPosition(0);
  for (int iAtom=1; iAtom<na; iAtom++) {
    double* r = box.getAtomPosition(iAtom);
    random.inSphere(r);
    r[0] = r[0]*sigma + rPrev[0];
    r[1] = r[1]*sigma + rPrev[1];
    r[2] = r[2]*sigma + rPrev[2];
    rPrev = r;
  }
  numTrials++;
  numAccepted++;
  return true;
}
