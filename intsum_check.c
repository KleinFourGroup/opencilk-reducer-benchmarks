#include <cilk/cilk.h>
#include <cilk/reducer.h>
#include <stdio.h>
#include <stdlib.h>

#include "ktiming.h"

void identity_uint64sum(void *reducer, void *sum) { *((uint64_t *)sum) = 0; }

void reduce_uint64sum(void *reducer, void *left, void *right) {
    *((uint64_t *)left) += *((uint64_t *)right);
}

CILK_C_DECLARE_REDUCER(uint64_t)
array_sum_reducer = CILK_C_INIT_REDUCER(uint64_t, reduce_uint64sum, identity_uint64sum,
                                         0, 0);

uint64_t array_sum_serial = 0;

uint64_t prng = 0x2357BD;

uint64_t __attribute__((noinline)) fill_array(uint64_t * restrict arr, uint64_t size) {
    uint64_t i;

    uint64_t sum = 0;

    for (i = 0; i < size; i++) {
        prng ^= prng << 13;
        prng ^= prng >> 7;
        prng ^= prng << 17;
        arr[i] = prng % 2;
        sum += prng % 2;
    }

    return sum;
}

uint64_t test_reducer_serial(uint64_t * restrict arr, uint64_t num, uint64_t size) {
    for (uint64_t i = 0; i < num; i++) {
        array_sum_serial += arr[i % size];
    }

    return array_sum_serial;
}

uint64_t test_reducer_redserial(uint64_t * restrict arr, uint64_t num, uint64_t size) {
    for (uint64_t i = 0; i < num; i++) {
        REDUCER_VIEW(array_sum_reducer) += arr[i % size];
    }

    return REDUCER_VIEW(array_sum_reducer);
}

uint64_t test_reducer_parallel(uint64_t * restrict arr, uint64_t num, uint64_t size) {
    cilk_for (uint64_t i = 0; i < num; i++) {
        REDUCER_VIEW(array_sum_reducer) += arr[i % size];
    }

    return REDUCER_VIEW(array_sum_reducer);
}

uint64_t test_reducer_atomic(uint64_t * restrict arr, uint64_t num, uint64_t size) {
    cilk_for (uint64_t i = 0; i < num; i++) {
        array_sum_serial += arr[i % size];
    }

    return array_sum_serial;
}

int main(int argc, const char **args) {
    uint64_t i, n;
    uint64_t * arr = NULL;

    int check = 0, res = 0;
    clockmark_t begin, end;
    uint64_t running_time[TIMING_COUNT];

    if (argc != 3) {
        fprintf(stderr, "Usage: ilist_dac [<cilk-options>] <n> <check?>\n");
        exit(1);
    }

    n = atol(args[1]);
    check = atol(args[2]);

    arr = (uint64_t *) malloc(1000000 * sizeof(uint64_t));

    fill_array(arr, 1000000);
    const uint64_t correct = test_reducer_serial(arr, n, 1000000);

    for (i = 0; i < TIMING_COUNT; i++) {
        uint64_t sum = 0;
        begin = ktiming_getmark();
        CILK_C_REGISTER_REDUCER(array_sum_reducer);
        *(&REDUCER_VIEW(array_sum_reducer)) = 0;
        array_sum_serial = 0;

        switch(check) {
        case 0:
            sum = test_reducer_serial(arr, n, 1000000); break;
        case 1:
            sum = test_reducer_redserial(arr, n, 1000000); break;
        case 2:
            sum = test_reducer_parallel(arr, n, 1000000); break;
        case 3:
            sum = test_reducer_atomic(arr, n, 1000000); break;
        default:
            break;
        }

        res += (sum == correct) ? 1 : 0;
        // printf("Calculated: %lu; Correct: %lu\n", sum, correct);
        CILK_C_UNREGISTER_REDUCER(array_sum_reducer);
        end = ktiming_getmark();
        // prlongf("The final sum is %d\n", sum);
        running_time[i] = ktiming_diff_nsec(&begin, &end);
    }
    fprintf(stderr, "Result: %d/%d successes!\n", res, TIMING_COUNT);
    print_runtime(running_time, TIMING_COUNT);

    return res != TIMING_COUNT;
}
