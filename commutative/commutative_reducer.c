#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <cilk/cilk.h>
#include <cilk/cilk_api.h>
#include "fasttime.h"
#include "defs.h"

#define CILK_NWORKERS 16

// Without "Atomic", reading directly from tls_worker can cause a wrong answer
//extern __thread _Atomic(struct __cilkrts_worker *)tls_worker;
extern __thread struct __cilkrts_worker *tls_worker;


void eval_commutative_red(Vector* arr, int* indices) {
  Vector* local_views;
  //local_views = malloc(CILK_NWORKERS * sizeof(Vector));
  local_views = aligned_alloc(64 * 8, CILK_NWORKERS * sizeof(Vector));
  memset(local_views, 0, CILK_NWORKERS * sizeof(Vector));

  Vector* n;
  n = malloc(sizeof(Vector));
  memset(n, 0, sizeof(Vector));

  fasttime_t start = gettime();
  // Sum
  for (int j = 0; j < NUM_SUM_OUTER * NUM_SUM_INNER; j += NUM_SUM_INNER) {
#pragma cilk grainsize 1
    cilk_for (int i = 0; i < NUM_SUM_INNER; i++) {
      //int worker_number = (int) (*(((uint64_t*) tls_worker) + 4));
      int worker_number = __cilkrts_get_worker_number();
      vector_add(&local_views[worker_number], &arr[indices[i + j]]);
    }
  }
  for (int i = 0; i < CILK_NWORKERS; i++) {
    vector_add(n, &local_views[i]);
  }
  fasttime_t stop = gettime();

  long sum = 0;
  for (int i = 0; i < VECTOR_LEN - 2; i++) {
    sum += n->ele[i];
  }
  printf("%f\t%ld\n", tdiff_sec(start, stop), sum);

  free(local_views);
  free(n);
}
