/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include <math.h>
#include <stdlib.h>
#include "random.h"

#include "SFMT.h"
#ifdef HAVE_GETRANDOM
#include <sys/random.h>
#else
#include <fcntl.h>
#include <sys/time.h>
#include <unistd.h>
#endif

#define MAXUINT ((uint32_t)-2)

Random::Random() : hasNextGaussian(false) {
  seed = makeSeed();
  sfmt_init_gen_rand(&sfmt, seed);
}

Random::Random(int s) : hasNextGaussian(false) {
  seed = s;
  sfmt_init_gen_rand(&sfmt, seed);
}

int Random::makeSeed() {
  uint32_t seed;
#ifdef HAVE_GETRANDOM
  getrandom(&seed, 4, 0);
#else
  int fd = open("/dev/urandom", O_RDONLY);
  if (fd<0) {
    struct timeval t;
    gettimeofday(&t, NULL);
    unsigned long long x = (unsigned long long)(t.tv_sec) * 1000000 + (unsigned long long)(t.tv_usec);
    seed = (uint32_t)x;
  }
  else {
    ssize_t b = read(fd, &seed, 4);
    if (b!=4) {
      fprintf(stderr, "unable to read from urandom\n");
      exit(1);
    }
    close(fd);
  }
#endif
  return seed;
}

int Random::getSeed() {
  return seed;
}

int Random::nextInt(int max) {
  uint32_t maxRand = MAXUINT - ((MAXUINT+1) % max);
  uint32_t s;
  do {
    s = sfmt_genrand_uint32(&sfmt);
  } while (s > maxRand);
  return s % max;
}

double Random::nextDouble() {
  return sfmt_genrand_res53(&sfmt);
}

double Random::nextDouble32() {
  return sfmt_genrand_real1(&sfmt);
}

double Random::nextGaussian() {
  if (hasNextGaussian) {
    hasNextGaussian = false;
    return nextG;
  }

  double x1, x2, w;
  do {
    x1 = nextDouble();
    x2 = nextDouble();
    w = x1 * x1 + x2 * x2;
  } while (w >= 1);
  w = sqrt(-2 * log(w) / w);
  int signs = nextInt(4);
  if ((signs & 0x00000001) == 1) x1 = -x1;
  if ((signs & 0x00000002) == 2) x2 = -x2;
  nextG = x2 * w;
  hasNextGaussian = true;
  return x1 * w;
}

void Random::onSphere(double *v) {
  //Based on M.P. Allen and D.J. Tildesley, Computer Simulation of Liquids, p 349.
  double z1, z2, zsq;
  do  {
    z1 = 2.0 * nextDouble32() - 1.0;
    z2 = 2.0 * nextDouble32() - 1.0;
    zsq = z1 * z1 + z2 * z2;
  } while (zsq > 1.0);

  double ranh = 2.0 * sqrt(1.0 - zsq);
  v[0] = z1 * ranh;
  v[1] = z2 * ranh;
  v[2] = 1.0 - 2.0 * zsq;
}

//generate point on surface of sphere according to method (4) here:
//http://www.math.niu.edu/~rusin/known-math/96/sph.rand
//and scale into the interior according to r^2
void Random::inSphere(double *v) {
  double r = cbrt(nextDouble32());
  double u, w, s;
  do {
    u = 1.0 - 2.0*nextDouble32();
    w = 1.0 - 2.0*nextDouble32();
    s = u*u + w*w;
  } while(s > 1);
  double ra = 2.*r * sqrt(1.-s);
  v[0] = ra * u;
  v[1] = ra * w;
  v[2] = r * (2*s- 1.);
}
