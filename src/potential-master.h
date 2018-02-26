#pragma once

#include <vector>
#include <math.h>
#include <cstddef>
#include <set>
#include <algorithm>
#include "box.h"
#include "potential.h"

using namespace std;

class PotentialCallback {
  public:
    bool callPair;
    bool callFinished;
    bool takesForces;

    PotentialCallback();
    virtual ~PotentialCallback() {}
    virtual void reset() {}
    virtual void pairCompute(int iAtom, int jAtom, double* dr, double u, double du, double d2u) {}
    virtual void allComputeFinished(double uTot, double virialTot, double** f) {}
    virtual int getNumData() {return 0;}
    virtual double* getData() {return nullptr;}
};

class PotentialCallbackPressure : public PotentialCallback {
  protected:
    Box& box;
    double temperature;
    double data[1];
  public:
    PotentialCallbackPressure(Box& box, double temperature);
    ~PotentialCallbackPressure() {}
    virtual void allComputeFinished(double uTot, double virialTot, double** f);
    virtual int getNumData();
    virtual double* getData();
};
    
class PotentialCallbackHMA : public PotentialCallback {
  protected:
    Box& box;
    double temperature;
    double Pharm;
    double data[2];
    double** latticePositions;
  public:
    PotentialCallbackHMA(Box& box, double temperature, double Pharm);
    ~PotentialCallbackHMA() {}
    virtual void allComputeFinished(double uTot, double virialTot, double** f);
    virtual int getNumData();
    virtual double* getData();
};

class PotentialMaster {
  protected:
    const SpeciesList& speciesList;
    Potential*** pairPotentials;
    double** pairCutoffs;
    Box& box;
    vector<double> uAtom;
    vector<double> duAtom;
    vector<int> uAtomsChanged;
    set<int> uAtomsChangedSet;
    double** force;
    vector<PotentialCallback*> pairCallbacks;
    int numAtomTypes;
    vector<vector<int*> > *bondedPairs;
    vector<int> **bondedAtoms;
    vector<Potential*> *bondedPotentials;
    const bool pureAtoms;
    bool rigidMolecules;
    void computeAllBonds(bool doForces, double &uTot, double &virialTot);
    inline bool checkSkip(int jAtom, int iMolecule, vector<int> *iBondedAtoms) {
      if (pureAtoms) return false;
      int jMolecule = box.getMolecule(jAtom);
      if (rigidMolecules) return iMolecule == jMolecule;
      int jFirstAtom = jAtom, jLastAtom = jAtom, jSpecies = 0;
      box.getMoleculeInfo(jMolecule, jSpecies, jFirstAtom, jLastAtom);
      return binary_search(iBondedAtoms->begin(), iBondedAtoms->end(), jAtom-jFirstAtom);
    }

  public:
    PotentialMaster(const SpeciesList &speciesList, Box& box);
    virtual ~PotentialMaster() {}
    Box& getBox();
    void setPairPotential(int iType, int jType, Potential* pij);
    void setBondPotential(int iSpecies, vector<int*> &bondedPairs, Potential *pBond);
    virtual void computeAll(vector<PotentialCallback*> &callbacks);
    virtual void computeOne(const int iAtom, const double *ri, double &energy, const bool isTrial);
    virtual void computeOneMolecule(int iMolecule, double &energy, bool isTrial);
    virtual void updateAtom(int iAtom) {}
    virtual void newAtom();
    virtual void removeAtom(int iAtom);
    double oldEnergy(int iAtom);
    void resetAtomDU();
    void processAtomU(int coeff);
    void addCallback(PotentialCallback* pcb);
};

class PotentialMasterCell : public PotentialMaster {
  protected:
    int cellRange;
    double boxHalf[3];
    int numCells[3];
    vector<int> cellNextAtom;
    vector<int> atomCell;
    vector<int> cellLastAtom;
    int jump[3];
    vector<int> cellOffsets;
    vector<int> wrapMap;
    double** rawBoxOffsets;
    double** boxOffsets;
    int numAtoms;

    void handleComputeAll(const int iAtom, const int jAtom, const double *ri, const double *rj, Potential* pij, double &ui, double &uj, double *fi, double *fj, double& uTot, double& virialTot, const double rc2, const bool doForces);
    void handleComputeOne(Potential* pij, const double *ri, const double *rj, const int jAtom, double& uTot, double rc2);
    int wrappedIndex(int i, int nc);
    void moveAtomIndex(int oldIndex, int newIndex);
  public:
    PotentialMasterCell(const SpeciesList &speciesList, Box& box, int cellRange);
    ~PotentialMasterCell();
    virtual double getRange();
    virtual void init();
    virtual void computeAll(vector<PotentialCallback*> &callbacks);
    virtual void computeOne(const int iAtom, const double *ri, double &energy, const bool isTrial);
    virtual void updateAtom(int iAtom);
    virtual void newAtom();
    virtual void removeAtom(int iAtom);
    void assignCells();
    int cellForCoord(const double *r);
    int* getNumCells();
};

class PotentialMasterList : public PotentialMasterCell {
  protected:
    double nbrRange;
    int **nbrs;
    bool onlyUpNbrs; // standard MD only needs up.  MC or DMD needs down
    int *numAtomNbrsUp, *numAtomNbrsDn;
    int nbrsNumAtoms;
    int maxNab;
    double ***nbrBoxOffsets;
    bool forceReallocNbrs;
    double **oldAtomPositions;
    double safetyFac;
    double *maxR2, *maxR2Unsafe;

    int checkNbrPair(int iAtom, int jAtom, double *ri, double *rj, double rc2, double *jbo);
  public:
    PotentialMasterList(const SpeciesList& speciesList, Box& box, int cellRange, double nbrRange);
    ~PotentialMasterList();
    virtual double getRange();
    virtual void init();
    void reset();
    void setDoDownNbrs(bool doDown);
    void checkUpdateNbrs();
    virtual void computeAll(vector<PotentialCallback*> &callbacks);

};
