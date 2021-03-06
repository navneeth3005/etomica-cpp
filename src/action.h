/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#pragma once

class Box;

class Action {
  public:
    Action() {}
    virtual ~Action() {}
    virtual void go() = 0;
};

class ConfigurationLattice : Action {
  protected:
    Box& box;
    int numBasisAtoms;
    double** basis;
    double* cellShape;

  public:
    ConfigurationLattice(Box& box, double** basis, double* cellShape);
    ~ConfigurationLattice();
    virtual void go();
};

class ConfigurationFile : Action {
  protected:
    Box& box;
    const char* filename;

  public:
    ConfigurationFile(Box& box, const char* filename);
    ~ConfigurationFile();
    virtual void go();
};
