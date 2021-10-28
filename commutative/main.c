#include <stdio.h>
#include <stdlib.h>
#include <cilk/cilk.h>
#include "defs.h"


int main() {
  srand(0);

  //Vector* arr = (Vector*) malloc(ARR_LEN * sizeof(Vector));
  if (VECTOR_LEN < 2) {
    printf("Vector length too small\n");
    return 0;
  }
  Vector* arr = (Vector*) aligned_alloc(64, ARR_LEN * sizeof(Vector));
  int* indices = (int*) malloc(NUM_SUM_INNER * NUM_SUM_OUTER * sizeof(int));
  for (int i = 0; i < ARR_LEN; i++) {
    for (int j = 0; j < VECTOR_LEN - 2; j++) {
      arr[i].ele[j] = rand() % ARR_MAX_ELE;
    }
    arr[i].ele[VECTOR_LEN - 2] = 0;
    arr[i].ele[VECTOR_LEN - 1] = 0;
  }
  for (int i = 0; i < NUM_SUM_INNER * NUM_SUM_OUTER; i++) {
    indices[i] = rand() % ARR_LEN;
  }

#if METHOD == 0
  eval_serial(arr, indices);
#elif METHOD == 1
  eval_associative_red(arr, indices);
#elif METHOD == 2
  eval_commutative_red(arr, indices);
#elif METHOD == 3
  eval_commutative_red_builtin(arr, indices);
#endif
}
