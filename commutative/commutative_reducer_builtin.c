#include <stdio.h>
#include <stdlib.h>
#include <cilk/cilk.h>
#include <cilk/reducer.h>
#include "fasttime.h"
#include "defs.h"


typedef CILK_C_DECLARE_REDUCER(Vector) ComSumReducer;


ComSumReducer n_com = CILK_C_INIT_COM_REDUCER(Vector,
    sum_reduce, sum_identity, sum_destroy);


void eval_commutative_red_builtin(Vector* arr, int* indices) {
  CILK_C_REGISTER_COM_REDUCER(n_com);

  fasttime_t start = gettime();
  // Sum
  for (int j = 0; j < NUM_SUM_OUTER * NUM_SUM_INNER; j += NUM_SUM_INNER) {
#pragma cilk grainsize 1
    cilk_for (int i = 0; i < NUM_SUM_INNER; i++) {
      vector_add(&COM_REDUCER_VIEW(n_com), &arr[indices[i + j]]);
    }
  }
  COM_REDUCER_MERGE(n_com);
  fasttime_t stop = gettime();

  long sum = 0;
  for (int i = 0; i < VECTOR_LEN - 2; i++) {
    sum += COM_REDUCER_VIEW(n_com).ele[i];
  }
  printf("%f\t%ld\n", tdiff_sec(start, stop), sum);

  //CILK_C_UNREGISTER_REDUCER(n_com);
}
