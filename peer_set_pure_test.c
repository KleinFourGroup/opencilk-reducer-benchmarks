#include <cilk/cilk.h>
#include <cilk/reducer.h>
#include <stdio.h>
#include <stdlib.h>

#include "ktiming.h"

typedef uint64_t data_t;

void identity_datasum(void *reducer, void *sum) { *((data_t *)sum) = 0; }

void reduce_datasum(void *reducer, void *left, void *right) {
    *((data_t *)left) += *((data_t *)right);
}

CILK_C_DECLARE_REDUCER(data_t)
array_sum_reducer = CILK_C_INIT_REDUCER(data_t, reduce_datasum, identity_datasum,
                                         0, 0);

data_t array_sum_serial = 0;

uint64_t prng = 0x2357BD;

data_t __attribute__((noinline)) fill_array(data_t * restrict arr, uint64_t size) {
    uint64_t i;

    data_t sum = 0;

    for (i = 0; i < size; i++) {
        prng ^= prng << 13;
        prng ^= prng >> 7;
        prng ^= prng << 17;
        arr[i] = (data_t) prng % 2;
        sum += arr[i];
    }

    return sum;
}

data_t test_reducer_serial(data_t * restrict arr, uint64_t num, uint64_t size) {
    for (uint64_t i = 0; i < num; i++) {
        array_sum_serial += arr[i % size];
    }

    return array_sum_serial;
}

data_t test_peer_set_pure_1(data_t * restrict arr, uint64_t num, uint64_t size) {    
    for (uint64_t i = 0; i < num; i++) {
        REDUCER_VIEW(array_sum_reducer) += arr[i % size];
    }

    return REDUCER_VIEW(array_sum_reducer);
}

data_t test_peer_set_pure_2(data_t * restrict arr, uint64_t num, uint64_t size) {
    for (uint64_t i = 0; i < num; i++) {
        noop();
        REDUCER_VIEW(array_sum_reducer) += arr[i % size];
    }

    return REDUCER_VIEW(array_sum_reducer);
}

data_t test_peer_set_pure_3(data_t * restrict arr, uint64_t num, uint64_t size) {
    for (uint64_t i = 0; i < num; i++) {
        noop();
        noop();
        REDUCER_VIEW(array_sum_reducer) += arr[i % size];
    }

    return REDUCER_VIEW(array_sum_reducer);
}

data_t test_peer_set_pure_4(data_t * restrict arr, uint64_t num, uint64_t size) {
    data_t * const view = &REDUCER_VIEW(array_sum_reducer);
    for (uint64_t i = 0; i < num; i++) {
        noop();
        *view += arr[i % size];
    }

    return REDUCER_VIEW(array_sum_reducer);
}

data_t test_peer_set_pure_5(data_t * restrict arr, uint64_t num, uint64_t size) {
    CILK_C_DECLARE_REDUCER(data_t)
    local_reducer = CILK_C_INIT_REDUCER(data_t, reduce_datasum, identity_datasum,
                                         0, 0);

    data_t ret = 0;
    CILK_C_REGISTER_REDUCER(local_reducer);
    *(&REDUCER_VIEW(local_reducer)) = 0;

    for (uint64_t i = 0; i < num; i++) {
        noop();
        REDUCER_VIEW(local_reducer) += arr[i % size];
    }

    ret = REDUCER_VIEW(local_reducer);
            CILK_C_UNREGISTER_REDUCER(local_reducer);
    return ret;

}

int tmain(int argc, const char **args) {
    uint64_t i, n;
    data_t * arr = NULL;

    int check = 0, res = 0;
    clockmark_t begin, end;
    uint64_t running_time[TIMING_COUNT];

    if (argc != 3) {
        fprintf(stderr, "Usage: ilist_dac [<cilk-options>] <n> <check?>\n");
        exit(1);
    }

    n = atol(args[1]);
    check = atol(args[2]);

    arr = (data_t *) malloc(1000000 * sizeof(data_t));

    fill_array(arr, 1000000);
    const data_t correct = test_reducer_serial(arr, n, 1000000);

    for (i = 0; i < TIMING_COUNT; i++) {
        data_t sum = 0;
        begin = ktiming_getmark();
        CILK_C_REGISTER_REDUCER(array_sum_reducer);
        *(&REDUCER_VIEW(array_sum_reducer)) = 0;
        array_sum_serial = 0;

        switch(check) {
        case 0:
            sum = test_peer_set_pure_1(arr, n, 1000000); break;
        case 1:
            sum = test_peer_set_pure_2(arr, n, 1000000); break;
        case 2:
            sum = test_peer_set_pure_3(arr, n, 1000000); break;
        case 3:
            sum = test_peer_set_pure_4(arr, n, 1000000); break;
        case 4:
            sum = test_peer_set_pure_5(arr, n, 1000000); break;
        default:
            break;
        }

        res += (sum == correct) ? 1 : 0;
        CILK_C_UNREGISTER_REDUCER(array_sum_reducer);
        end = ktiming_getmark();
        // prlongf("The final sum is %d\n", sum);
        running_time[i] = ktiming_diff_nsec(&begin, &end);
    }
    cilk_sync;

    fprintf(stderr, "Result: %d/%d successes!\n", res, TIMING_COUNT);
    print_runtime(running_time, TIMING_COUNT);

    return res != TIMING_COUNT;
}

int main(int argc, const char **args) {
    int ret = 0;

    cilk_spawn {
        ret = tmain(argc, args);
    }
    noop();
    cilk_sync;

    return ret;
}