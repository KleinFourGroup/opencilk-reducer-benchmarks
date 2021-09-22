#include <cilk/cilk.h>
#include <cilk/reducer.h>
#include <stdio.h>
#include <stdlib.h>

#include "ktiming.h"

void identity_uint64sum(void *reducer, void *sum) { *((uint64_t *)sum) = 0; }

void reduce_uint64sum(void *reducer, void *left, void *right) {
    *((uint64_t *)left) += *((uint64_t *)right);
}

typedef CILK_C_DECLARE_REDUCER(uint64_t) uint64red;

uint64_t * serial_histogram = NULL;
uint64red * histogram = NULL;
uint64_t bins = 0;

// array_sum_reducer = CILK_C_INIT_REDUCER(uint64_t, reduce_uint64sum, identity_uint64sum,
//                                          0, 0);

void set_bins(int num) {
    uint64_t i;

    bins = num;

    serial_histogram = (uint64_t *)malloc(num * sizeof(uint64_t));
    for (i = 0; i < num; i++) {
        serial_histogram[i] = 0;
    }

    histogram = (uint64red *)malloc(num * sizeof(uint64red));
    for (i = 0; i < num; i++) {
        histogram[i] = (uint64red)CILK_C_INIT_REDUCER(uint64_t, reduce_uint64sum, identity_uint64sum,
                                                        0, 0);
    }
}

inline void prepare() {
    for (uint64_t i = 0; i < bins; i++) {
        CILK_C_REGISTER_REDUCER(histogram[i]);
        *(&REDUCER_VIEW(histogram[i])) = 0;
        serial_histogram[i] = 0;
    }
}

inline void cleanup() {
    for (uint64_t i = 0; i < bins; i++) {
        CILK_C_UNREGISTER_REDUCER(histogram[i]);
    }
}

uint64_t prng = 0x2357BD;

void __attribute__((noinline)) fill_array(uint64_t * restrict arr, uint64_t size) {
    uint64_t i;

    for (i = 0; i < size; i++) {
        prng ^= prng << 13;
        prng ^= prng >> 7;
        prng ^= prng << 17;
        arr[i] = prng;
    }
}

uint64_t * get_correct(uint64_t * restrict arr, uint64_t num, uint64_t size) {
    uint64_t * correct = (uint64_t *)malloc(num * sizeof(uint64_t));
    for (uint64_t i = 0; i < num; i++) {
        correct[arr[i % size] % bins]++;
    }

    return correct;
}

void test_reducer_serial(uint64_t * restrict arr, uint64_t num, uint64_t size) {
    for (uint64_t i = 0; i < num; i++) {
        serial_histogram[arr[i % size] % bins]++;
    }
}

void test_reducer_redserial(uint64_t * restrict arr, uint64_t num, uint64_t size) {
    for (uint64_t i = 0; i < num; i++) {
        REDUCER_VIEW(histogram[arr[i % size] % bins])++;
    }
}

void test_reducer_parallel(uint64_t * restrict arr, uint64_t num, uint64_t size) {
    cilk_for (uint64_t i = 0; i < num; i++) {
        REDUCER_VIEW(histogram[arr[i % size] % bins])++;
    }
}

void test_reducer_atomic(uint64_t * restrict arr, uint64_t num, uint64_t size) {
    cilk_for (uint64_t i = 0; i < num; i++) {
        serial_histogram[arr[i % size] % bins]++;
    }
}

inline uint64_t check_bins(uint64_t * correct, int check) {
    switch(check) {
    case 0:
        for (uint64_t i = 0; i < bins; i++) {
            if (correct[i] != serial_histogram[i]) return 0;
        }
        return 1;
    case 1:
        for (uint64_t i = 0; i < bins; i++) {
            if (correct[i] != REDUCER_VIEW(histogram[i])) return 0;
        }
        return 1;
    case 2:
        for (uint64_t i = 0; i < bins; i++) {
            if (correct[i] != REDUCER_VIEW(histogram[i])) return 0;
        }
        return 1;
    case 3:
        for (uint64_t i = 0; i < bins; i++) {
            if (correct[i] != serial_histogram[i]) return 0;
        }
        return 1;
    default:
        break;
    }
    
    return 0;
}

int main(int argc, const char **args) {
    uint64_t i, b, n;
    uint64_t * arr = NULL;

    int check = 0, res = 0;
    clockmark_t begin, end;
    uint64_t running_time[TIMING_COUNT];

    if (argc != 4) {
        fprintf(stderr, "Usage: ilist_dac [<cilk-options>] <n> <check?>\n");
        exit(1);
    }

    b = atol(args[1]);
    n = atol(args[2]);
    check = atol(args[3]);

    set_bins(b);
    arr = (uint64_t *) malloc(1000000 * sizeof(uint64_t));

    fill_array(arr, 1000000);
    uint64_t * correct = get_correct(arr, n, 1000000);

    for (i = 0; i < TIMING_COUNT; i++) {
        begin = ktiming_getmark();
        prepare();

        switch(check) {
        case 0:
            test_reducer_serial(arr, n, 1000000); break;
        case 1:
            test_reducer_redserial(arr, n, 1000000); break;
        case 2:
            test_reducer_parallel(arr, n, 1000000); break;
        case 3:
            test_reducer_atomic(arr, n, 1000000); break;
        default:
            break;
        }

        res += check_bins(correct, check) ? 1 : 0;
        cleanup();
        end = ktiming_getmark();
        running_time[i] = ktiming_diff_nsec(&begin, &end);
    }
    printf("Result: %d/%d successes!\n", res, TIMING_COUNT);
    print_runtime(running_time, TIMING_COUNT);

    return res != TIMING_COUNT;
}
