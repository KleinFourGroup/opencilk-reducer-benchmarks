#include <stdio.h>
#include <stdlib.h>
#include <cilk/cilk.h>
#include "fasttime.h"
#include "defs.h"


void eval_serial(Vector* arr, int* indices) {
  Vector n = {};

  fasttime_t start = gettime();
  // Sum
  for (int j = 0; j < NUM_SUM_OUTER * NUM_SUM_INNER; j += NUM_SUM_INNER) {
    for (int i = 0; i < NUM_SUM_INNER; i++) {
      vector_add(&n, &arr[indices[i + j]]);
    }
  }
  fasttime_t stop = gettime();

  long sum = 0;
  for (int i = 0; i < VECTOR_LEN - 2; i++) {
    sum += n.ele[i];
  }
  printf("%f\t%ld\n", tdiff_sec(start, stop), sum);
}
