/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include <stdio.h>

#include "potential-master.h"
#include "alloc2d.h"
#include "vector.h"
#include "rotation-matrix.h"

/**
 * Computes energy with mapped averaging for rigid molecular system.  Also handles atoms.
 */
PotentialCallbackMoleculeHMA::PotentialCallbackMoleculeHMA(Box& b, SpeciesList& sl, double T, double Ph) : box(b), speciesList(sl), temperature(T), Pharm(Ph), returnAnh(false), computingLat(false) {
  callFinished = true;
  takesForces = true;
  int N = box.getTotalNumMolecules();
  latticePositions = (double**)malloc2D(N, 3, sizeof(double));
  latticeOrientations = (double**)malloc2D(N, 6, sizeof(double));
  // let's pretend we can't just copy the whole thing at once
  for (int i=0; i<N; i++) {
    int iSpecies, iMoleculeInSpecies, iFirstAtom, iLastAtom;
    box.getMoleculeInfo(i, iSpecies, iMoleculeInSpecies, iFirstAtom, iLastAtom);
    Species* species = speciesList.get(iSpecies);
    double* pos = species->getMoleculeCOM(box, iFirstAtom, iLastAtom);
    std::copy(pos, pos+3, latticePositions[i]);
    double o[6];
    species->getMoleculeOrientation(box, iFirstAtom, o, o+3);
    std::copy(o, o+6, latticeOrientations[i]);
  }
  data = (double*) malloc(4*sizeof(double));
}

PotentialCallbackMoleculeHMA::~PotentialCallbackMoleculeHMA() {
  free2D((void**)latticePositions);
  free2D((void**)latticeOrientations);
  free(data);
}

void PotentialCallbackMoleculeHMA::setReturnAnharmonic(bool ra, PotentialMaster* pm) {
  returnAnh = ra;
  if (ra) {
    computingLat = true;
    vector<PotentialCallback*> callbacks;
    callbacks.push_back(this);
    pm->computeAll(callbacks);
    computingLat = false;
    uLat = data[0];
    pLat = data[1];
    printf("pLat %f\n", pLat);
  }
}

int PotentialCallbackMoleculeHMA::getNumData() {return 4;}

void PotentialCallbackMoleculeHMA::allComputeFinished(double uTot, double virialTot, double** f) {
  const double* bs = box.getBoxSize();
  double vol = bs[0]*bs[1]*bs[2];
  if (computingLat) {
    data[0] = uTot;
    data[1] = -virialTot/(3*vol);
    return;
  }
  int N = box.getTotalNumMolecules();
  
  double dr0[3], dr[3];
  int iSpecies, iMoleculeInSpecies, iFirstAtom, iLastAtom;
  box.getMoleculeInfo(0, iSpecies, iMoleculeInSpecies, iFirstAtom, iLastAtom);
  Species* species = speciesList.get(iSpecies);
  double* ri = species->getMoleculeCOM(box, iFirstAtom, iLastAtom);
  for (int j=0; j<3; j++) {
    dr0[j] = ri[j] - latticePositions[0][j];
  }
  double fdotdrTot = 0, orientationSum = 0;
  double rotationDOF = 0;
  for (int i=0; i<N; i++) {
    box.getMoleculeInfo(i, iSpecies, iMoleculeInSpecies, iFirstAtom, iLastAtom);
    species = speciesList.get(iSpecies);
    double* ri = species->getMoleculeCOM(box, iFirstAtom, iLastAtom);
    for (int j=0; j<3; j++) {
      dr[j] = ri[j] - latticePositions[i][j] - dr0[j];
    }
    box.nearestImage(dr);
    double fi[3] = {0,0,0};
    double torque[3] = {0,0,0};
    double atomTorque[3] = {0,0,0};
    for (int iAtom=iFirstAtom; iAtom<=iLastAtom; iAtom++) {
      for (int j=0; j<3; j++) fi[j] += f[iAtom][j];
      if (iLastAtom>iFirstAtom) {
        double* riAtom = box.getAtomPosition(iAtom);
        double drAtom[3] = {riAtom[0]-ri[0], riAtom[1]-ri[1], riAtom[2]-ri[2]};
        box.nearestImage(drAtom);
        Vector::cross(drAtom, f[iAtom], atomTorque);
        for (int j=0; j<3; j++) torque[j] += atomTorque[j];
      }
    }
    for (int j=0; j<3; j++) {
      fdotdrTot += fi[j]*dr[j];
    }

    double o[9];
    species->getMoleculeOrientation(box, iFirstAtom, o, o+3);
    if (o[0]==0 && o[1]==0 && o[2] == 0) continue;
    double* lo = latticeOrientations[i];
    if (o[3]==0 && o[4]==0 && o[5] == 0) {
      rotationDOF += 2;
      // linear molecule
      double dot = o[0]*lo[0] + o[1]*lo[1] + o[2]*lo[2];
      if (fabs(dot) > 1) {
        dot = dot > 0 ? 1 : -1;
      }
      double axis[3];
      Vector::cross(o, lo, axis);
      double dudt = torque[0]*axis[0] + torque[1]*axis[1] + torque[2]*axis[2];
      orientationSum += (1-dot) / sqrt(1-dot*dot) * dudt;
    }
    else {
      rotationDOF += 3;
      // non-linear molecule
      double lo2[3];
      Vector::cross(lo, lo+3, lo2);
      RotationMatrix rotMatLat;
      rotMatLat.setRows(lo, lo+3, lo2);
      double o2[3];
      Vector::cross(o, o+3, o2);
      RotationMatrix rotMat;
      rotMat.setRows(o, o+3, o2);
      rotMat.transpose();
      rotMat.TE(rotMatLat);
      double axisRot[3];
      axisRot[0] = rotMat.matrix[2][1] - rotMat.matrix[1][2];
      axisRot[1] = rotMat.matrix[0][2] - rotMat.matrix[2][0];
      axisRot[2] = rotMat.matrix[1][0] - rotMat.matrix[0][1];
      double axisLength = sqrt(axisRot[0]*axisRot[0] + axisRot[1]*axisRot[1] + axisRot[2]*axisRot[2]);
      if (axisLength == 0) continue;
      // we'll miss any angle > 90; such angles indicate big trouble anyway
      double theta = asin(0.5 * axisLength);
      for (int j=0; j<3; j++) axisRot[j] /= axisLength;
      double dudt = -(torque[0]*axisRot[0] + torque[1]*axisRot[1] + torque[2]*axisRot[2]);
      if  (dudt == 0) continue;
      double denominator =  1 - cos(theta);
      if (denominator == 0) continue;
      orientationSum -= 1.5 * (theta - 0.5*axisLength) / denominator * dudt;
    }
  }
  double uHarm = 0.5*(3*(N-1) + rotationDOF)*temperature;
  double u0 = returnAnh ? (uHarm + uLat) : 0;
  data[0] = uTot - u0;
  double p0 = returnAnh ? (pLat + Pharm) : 0;
  data[1] = N*temperature/vol - virialTot/(3*vol) - p0;
  data[2] = (returnAnh ? -uLat : uHarm) + uTot + 0.5*fdotdrTot + orientationSum;
  double fV = (Pharm/temperature - N/vol)/(3*(N-1) + rotationDOF);
  //printf("%e %e %e\n", -virialTot/(3*vol), fV * (fdotdrTot + 2*orientationSum), fV);
  data[3] = (returnAnh ? -pLat : Pharm) - virialTot/(3*vol) + fV * (fdotdrTot + 2*orientationSum);
}

double* PotentialCallbackMoleculeHMA::getData() {
  return data;
}
