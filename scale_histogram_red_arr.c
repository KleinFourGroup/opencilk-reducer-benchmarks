#include <cilk/cilk.h>
#include <cilk/reducer.h>
#include <stdio.h>
#include <stdlib.h>

#include "ktiming.h"

uint64_t bins;

typedef struct Vector {
    uint64_t * ele; // MAK: Max for test
} __attribute__((aligned (64))) Vector;

/*
void vector_add(Vector* left, Vector* right) {
    for (int i = 0; i < bins; i++) {
        left->ele[i] = left->ele[i] + right->ele[i];
    }
}
*/

void vector_reduce(void* key, void* left, void* right) {
    Vector* left_v = (Vector*) left;
    Vector* right_v = (Vector*) right;
    for (int i = 0; i < bins; i++) {
        left_v->ele[i] = left_v->ele[i] + right_v->ele[i];
    }
}

void vector_identity(void* key, void* value) {
    ((Vector*)value)->ele = (uint64_t *)malloc(bins * sizeof(uint64_t));
    for (int i = 0; i < bins; i++) {
        ((Vector*)value)->ele[i] = 0;
    }
}

void vector_destroy(void* key, void* value) {
    free(((Vector*)value)->ele);
    ((Vector*)value)->ele = NULL;
}

CILK_C_DECLARE_REDUCER(Vector) a_histogram =
    CILK_C_INIT_REDUCER(Vector, vector_reduce, vector_identity, vector_destroy, 0);

// array_sum_reducer = CILK_C_INIT_REDUCER(uint64_t, reduce_uint64sum, identity_uint64sum,
//                                          0, 0);

void set_bins(int num) {
    uint64_t i;

    bins = num;
    
    CILK_C_REGISTER_REDUCER(a_histogram);
}

static inline void prepare() {
    if (REDUCER_VIEW(a_histogram).ele == NULL) {
        REDUCER_VIEW(a_histogram).ele =
            (uint64_t *)malloc(bins * sizeof(uint64_t));
    }
    for (int i = 0; i < bins; i++) {
        REDUCER_VIEW(a_histogram).ele[i] = 0;
    }
}

static inline void cleanup() {
    //CILK_C_UNREGISTER_REDUCER(a_histogram);
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

Vector * get_correct(uint64_t * restrict arr, uint64_t num, uint64_t size) {
    Vector * correct = (Vector *)malloc(sizeof(Vector));
    correct->ele = (uint64_t *)malloc(bins * sizeof(uint64_t));
    for (uint64_t i = 0; i < bins; i++) {
        correct->ele[i] = 0;
    }

    for (uint64_t i = 0; i < num; i++) {
        correct->ele[arr[i % size] % bins]++;
    }

    return correct;
}

void test_reducer_associative(uint64_t * restrict arr, uint64_t num, uint64_t size) {
    cilk_for (uint64_t i = 0; i < num; i++) {
        //printf("%p: %d %d %d %d\n", REDUCER_VIEW(a_histogram).ele, i, i % size, arr[i % size], arr[i % size] % bins);
        REDUCER_VIEW(a_histogram).ele[arr[i % size] % bins]++;
    }
}

static inline uint64_t check_bins(Vector * correct) {
    for (uint64_t i = 0; i < bins; i++) {
        if (correct->ele[i] != REDUCER_VIEW(a_histogram).ele[i]) return 0;
    }
    return 1;
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

    set_bins(b);
    arr = (uint64_t *) malloc(1000000 * sizeof(uint64_t));

    fill_array(arr, 1000000);
    Vector * correct = get_correct(arr, n, 1000000);

    for (i = 0; i < TIMING_COUNT; i++) {
        prepare();
        begin = ktiming_getmark();

        test_reducer_associative(arr, n, 1000000);

        end = ktiming_getmark();
        res += check_bins(correct) ? 1 : 0;
        cleanup();
        running_time[i] = ktiming_diff_nsec(&begin, &end);
    }
    printf("Result: %d/%d successes!\n", res, TIMING_COUNT);
    print_runtime(running_time, TIMING_COUNT);

    return res != TIMING_COUNT;
}
