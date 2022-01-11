#include <cilk/cilk.h>
#include <cilk/reducer.h>
#include <stdio.h>
#include <stdlib.h>

#include "ktiming.h"

typedef struct hbin {
    uint64_t count;
    uint64_t val;
} hbin;


void identity_stealred(void *reducer, void *sum) { *((uint64_t *)sum) = 1; }

void reduce_stealred(void *reducer, void *left, void *right) {
    *((uint64_t *)left) += *((uint64_t *)right);
}

void identity_binred(void *reducer, void *bin) {
    ((hbin *)bin)->count = 1;
    ((hbin *)bin)->val = 0;
}

void reduce_binred(void *reducer, void *left, void *right) {
    hbin * left_h = (hbin *)left;
    hbin * right_h = (hbin *)right;

    left_h->count = left_h->count + right_h->count;
    left_h->val = left_h->val + right_h->val;
}

typedef CILK_C_DECLARE_REDUCER(uint64_t) stealred;
typedef CILK_C_DECLARE_REDUCER(hbin) binred;

uint64_t * serial_histogram = NULL;
stealred steal_ctr = CILK_C_INIT_REDUCER(uint64_t,
                                         reduce_stealred,
                                         identity_stealred,
                                         0, 0);
binred * histogram = NULL;
uint64_t bins = 0;

uint64_t merges, steals;

extern uint64_t extzero;

// array_sum_reducer = CILK_C_INIT_REDUCER(uint64_t, reduce_uint64sum, identity_uint64sum,
//                                          0, 0);

void set_bins(int num) {
    uint64_t i;

    bins = num;

    serial_histogram = (uint64_t *)malloc(num * sizeof(uint64_t));
    for (i = 0; i < num; i++) {
        serial_histogram[i] = 0;
    }

    histogram = (binred *)malloc(num * sizeof(binred));
    for (i = 0; i < num; i++) {
        histogram[i] =
            (binred)CILK_C_INIT_REDUCER(hbin,
                                        reduce_binred,
                                        identity_binred,
                                        0, 0);
    }
}

static inline void prepare() {
    CILK_C_REGISTER_REDUCER(steal_ctr);
    REDUCER_VIEW(steal_ctr) = 0;
    for (uint64_t i = 0; i < bins; i++) {
        CILK_C_REGISTER_REDUCER(histogram[i]);
        REDUCER_VIEW(histogram[i]).count = 0;
        REDUCER_VIEW(histogram[i]).val = 0;
        serial_histogram[i] = 0;
    }
}

static inline void cleanup() {
    for (uint64_t i = 0; i < bins; i++) {
        CILK_C_UNREGISTER_REDUCER(histogram[i]);
    }
    CILK_C_UNREGISTER_REDUCER(steal_ctr);
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

void test_reducer_parallel(uint64_t * restrict arr, uint64_t num, uint64_t size) {
    cilk_for (uint64_t i = 0; i < num; i++) {
        REDUCER_VIEW(steal_ctr) += extzero;
        REDUCER_VIEW(histogram[arr[i % size] % bins]).val++;
    }
}

static inline uint64_t check_bins(uint64_t * correct) {
    uint64_t ret = 1;
    for (uint64_t i = 0; i < bins; i++) {
        if (correct[i] != REDUCER_VIEW(histogram[i]).val) ret = 0;
        merges += REDUCER_VIEW(histogram[i]).count;
    }
    steals += REDUCER_VIEW(steal_ctr);
    return ret;
}

int main(int argc, const char **args) {
    uint64_t i, b, n;
    uint64_t * arr = NULL;

    int check = 0, res = 0;
    clockmark_t begin, end;
    uint64_t running_time[TIMING_COUNT];

    if (argc != 3) {
        fprintf(stderr, "Usage: ilist_dac [<cilk-options>] <n> <check?>\n");
        exit(1);
    }

    b = atol(args[1]);
    n = atol(args[2]);

    merges = 0;
    steals = 0;

    set_bins(b);
    arr = (uint64_t *) malloc(1000000 * sizeof(uint64_t));

    fill_array(arr, 1000000);
    uint64_t * correct = get_correct(arr, n, 1000000);

    for (i = 0; i < TIMING_COUNT; i++) {
        prepare();
        begin = ktiming_getmark();

        test_reducer_parallel(arr, n, 1000000);

        end = ktiming_getmark();
        res += check_bins(correct) ? 1 : 0;
        cleanup();
        running_time[i] = ktiming_diff_nsec(&begin, &end);
    }
    printf("Result: %d/%d successes!\n", res, TIMING_COUNT);
    print_runtime(running_time, TIMING_COUNT);
    printf("%lu\n%lu\n", merges, steals);

    return res != TIMING_COUNT;
}
