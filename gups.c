/*
 * =====================================================================================
 *
 *       Filename:  gups.c
 *
 *    Description:  
 *
 *        Version:  1.0
 *        Created:  02/21/2018 02:36:27 PM
 *       Revision:  none
 *       Compiler:  gcc
 *
 *         Author:  YOUR NAME (), 
 *   Organization:  
 *
 * =====================================================================================
 */
#include <stdlib.h>
#include <stdio.h>
#include <assert.h>
#include <sys/time.h>
#include <math.h>

unsigned long *indices;

#define GET_NEXT_INDEX(i,size) indices[i]

/* GUPS for 64-bit wide data, serial */
void
gups64(unsigned long *field, unsigned long iters, unsigned long size)
{
  unsigned long data;
  unsigned long i;
  unsigned long long index;

  for (i = 0; i < iters; i++) {
    index = GET_NEXT_INDEX(i, size);
    data = field[index];
    data = data + ((unsigned long)iters);
    field[index] = data;
  }
}

/* Returns the number of seconds encoded in T, a "struct timeval". */
#define tv_to_double(t) (t.tv_sec + (t.tv_usec / 1000000.0))

/* Useful for doing arithmetic on struct timevals. */
void
timeDiff(struct timeval *d, struct timeval *a, struct timeval *b)
{
  d->tv_sec = a->tv_sec - b->tv_sec;
  d->tv_usec = a->tv_usec - b->tv_usec;
  if (d->tv_usec < 0) {
    d->tv_sec -= 1;
    d->tv_usec += 1000000;
  }
}

/* Return the no. of elapsed seconds between Starttime and Endtime. */
double
elapsed(struct timeval *starttime, struct timeval *endtime)
{
  struct timeval diff;

  timeDiff(&diff, endtime, starttime);
  return tv_to_double(diff);
}

#ifdef UNIFORM_RANDOM
void
calc_indices(unsigned long updates, unsigned long nelems)
{
  unsigned int i;

  indices = (unsigned long*)malloc(updates * sizeof(unsigned long));
  if (!indices) {
    printf("Error: couldn't malloc space for array of indices\n");
    assert(indices != NULL);
  }
  for (i = 0; i < updates; i++) {
    indices[i] = random() % nelems;
  }
}
#else
const double ZETAN = 26.46902820178302;
const double ZIPFIAN_CONSTANT = 0.99;
unsigned long min, max, itemcount;
unsigned long items, base, countforzeta;
double zipfianconstant, alpha, zetan, eta, theta, zeta2theta;
unsigned long lastVal;
int allowitemdecrease = 0;
const long FNV_OFFSET_BASIS_64 = 0xCBF29CE484222325L;
const long FNV_PRIME_64 = 1099511628211L;

unsigned long
fnvhash64(unsigned long val) {
  long hashval = FNV_OFFSET_BASIS_64;

  for (int i = 0; i < 8; i++) {
    long octet = val & 0x00ff;
    val = val >> 8;

    hashval = hashval ^ octet;
    hashval = hashval * FNV_PRIME_64;
  }

  return (unsigned long)abs(hashval);
}

double
_zetastatic(unsigned long st, unsigned long n, double theta, double initialsum)
{
  double sum = initialsum;
  for (unsigned long i = st; i < n; i++) {
    sum += 1 / (pow(i + 1, theta));
  }
  return sum;
}

double
_zeta(unsigned long st, unsigned long n, double thetaVal, double initialsum)
{
  countforzeta = n;
  return _zetastatic(st, n, thetaVal, initialsum);
}
	
double
zetastatic(unsigned long n, double theta)
{
  return _zetastatic(0, n, theta, 0);
}

double
zeta(unsigned long n, double thetaVal)
{
  countforzeta = n;
  return zetastatic(n, thetaVal);

}

unsigned long 
nextValue(unsigned long itemcount)
{
  if (itemcount != countforzeta) {
    if (itemcount > countforzeta) {
      printf("recomputing zeta due to item increase\n");
      zetan = _zeta(countforzeta, itemcount, theta, zetan);
      eta = (1 - pow(2.0 / items, 1 - theta)) / (1 - zeta2theta / zetan);
    } else if (itemcount > countforzeta) {
      printf("recomputing zeta due to item decrease (warning: slow)\n");
      zetan = zeta(itemcount, theta);
      eta = (1 - pow(2.0 / items, 1 - theta)) / (1 - zeta2theta / zetan); 
    }
  }

  double u = (double)rand() / RAND_MAX;
  double uz = u * zetan;

  if (uz < 1.0) {
    return base;
  }

  if (uz < 1.0 + pow(0.5, theta)) {
    return base + 1;
  }

  unsigned long ret = base + (unsigned long)((itemcount) * pow(eta * u - eta + 1, alpha));
  lastVal = ret;
  return ret;
}

void 
calc_indices(unsigned long updates, unsigned long nelems)
{
  unsigned int i;
  indices = (unsigned long*)malloc(updates * sizeof(unsigned long));
  if (!indices) {
    printf("Error: couldn't malloc space for array of indices\n");
    assert(indices != NULL);
  }

  // init zipfian distrobution variables
  min = 0;
  max = nelems - 1;
  itemcount = max - min + 1;
  items = max - min + 1;
  base = min;
  zipfianconstant = ZIPFIAN_CONSTANT;
  theta = zipfianconstant;
  zeta2theta = zeta(2, theta);

  alpha = 1.0 / (1.0 - theta);
  zetan = ZETAN;
  countforzeta = items;
  eta = (1 - pow(2.0 / items, 1 - theta)) / (1 - zeta2theta / zetan);
  nextValue(nelems);

  for (i = 0; i < updates; i++) {
    unsigned long ret = nextValue(nelems);
    ret = min + fnvhash64(ret) % itemcount; //TODO: fnvhash64
    lastVal = ret;
    indices[i] = ret;
  }
}
#endif

int
main(int argc, char **argv)
{
  unsigned long updates, expt;
  unsigned long size, elt_size, nelems;
  void *field;
  unsigned long *field64;
  struct timeval starttime, stoptime;
  double secs, gups;

  if (argc != 3) {
    printf("Usage: %s [updates] [exponent]\n", argv[0]);
    return 0;
  }
  
  updates = atol(argv[1]);
  updates -= updates % 256;
  expt = atoi(argv[2]);
  assert(expt > 8);
  assert(updates > 0 && (updates % 256 == 0));
  size = (unsigned long)(1) << expt;
  size -= (size % 256);
  assert(size > 0 && (size % 256 == 0));

  printf("%lu updates, ", updates);
  printf("field of 2^%lu (%lu) bytes.\n", expt, size);

  field = malloc(size);
  if (field == NULL) {
    printf("Error: Failed to malloc %lu bytes.\n", size);
    assert(field != NULL);
  }

  field64 = (unsigned long*)field;
  assert(sizeof(unsigned long) == 8);
  elt_size = sizeof(unsigned long);

  printf("Element size is %lu bytes.\n", elt_size);
  nelems = size / elt_size;
  printf("Field is %lu data elements starting at 0x%08lx.\n", nelems,
	  (unsigned long) field);

  printf("Calculating indices.\n");
  calc_indices (updates, nelems);

  printf("Timing.\n");
  gettimeofday(&starttime, NULL);
  gups64(field64, updates, nelems);
  gettimeofday(&stoptime, NULL);

  secs = elapsed (&starttime, &stoptime);
  printf("Elapsed time: %.4f seconds.\n", secs);
  gups = ((double)updates) / (secs * 1.0e9);
  printf("GUPS = %.10f\n", gups);

  free(field);
  free(indices);

  return 0;
}