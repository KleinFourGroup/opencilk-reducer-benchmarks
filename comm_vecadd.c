#include <cilk/cilk.h>
#include <cilk/reducer.h>
#include <stdio.h>
#include <stdlib.h>

#include "ktiming.h"

uint64_t bins;

typedef struct Vector {
    uint64_t ele[256]; // MAK: Max for test
} __attribute__((aligned (64))) Vector;

static inline void vector_add(Vector* left, Vector* right) {
    for (int i = 0; i < bins; i++) {
        left->ele[i] = left->ele[i] + right->ele[i];
    }
}

void vector_reduce(void* key, void* left, void* right) {
    Vector* left_v = (Vector*) left;
    Vector* right_v = (Vector*) right;
    for (int i = 0; i < bins; i++) {
        left_v->ele[i] = left_v->ele[i] + right_v->ele[i];
    }
}

void vector_identity(void* key, void* value) {
    for (int i = 0; i < bins; i++) {
        ((Vector*)value)->ele[i] = 0;
    }
}


typedef CILK_C_DECLARE_REDUCER(Vector) vecred;

Vector serial_reducer;
vecred a_reducer = CILK_C_INIT_REDUCER(Vector, vector_reduce, vector_identity, 0, 0);
vecred c_reducer = CILK_C_INIT_COM_REDUCER(Vector, vector_reduce, vector_identity, 0);

// array_sum_reducer = CILK_C_INIT_REDUCER(uint64_t, reduce_uint64sum, identity_uint64sum,
//                                          0, 0);

void set_bins(int num) {
    uint64_t i;

    bins = num;
    
    CILK_C_REGISTER_REDUCER(a_reducer);
    CILK_C_REGISTER_COM_REDUCER(c_reducer);
}

static inline void prepare() {
    for (uint64_t i = 0; i < bins; i++) {
        REDUCER_VIEW(a_reducer).ele[i] = 0;
        COM_REDUCER_VIEW(c_reducer).ele[i] = 0;
        serial_reducer.ele[i] = 0;
    }
}

static inline void cleanup() {
    // CILK_C_UNREGISTER_REDUCER(a_reducer);
    // CILK_C_UNREGISTER_COM_REDUCER(c_reducer);
}

uint64_t prng = 0x2357BD;

void __attribute__((noinline)) fill_array(uint64_t * restrict arr, uint64_t size) {
    uint64_t i;

    for (i = 0; i < size + 256; i++) {
        prng ^= prng << 13;
        prng ^= prng >> 7;
        prng ^= prng << 17;
        arr[i] = prng;
    }
}

Vector * get_correct(uint64_t * restrict arr, uint64_t num, uint64_t size) {
    Vector * correct = (Vector *)malloc(sizeof(Vector));
    for (uint64_t i = 0; i < bins; i++) {
        correct->ele[i] = 0;
    }
    for (uint64_t i = 0; i < num; i++) {
        vector_add(correct, (Vector *)(arr + (i % size)));
    }

    return correct;
}

void test_reducer_serial(uint64_t * restrict arr, uint64_t num, uint64_t size) {
    for (uint64_t i = 0; i < num; i++) {
        vector_add(&serial_reducer, (Vector *)(arr + (i % size)));
    }
}

void test_reducer_associative(uint64_t * restrict arr, uint64_t num, uint64_t size) {
    cilk_for (uint64_t i = 0; i < num; i++) {
        vector_add(&REDUCER_VIEW(a_reducer), (Vector *)(arr + (i % size)));
    }
}

void test_reducer_commutative(uint64_t * restrict arr, uint64_t num, uint64_t size) {
    cilk_for (uint64_t i = 0; i < num; i++) {
        vector_add(&COM_REDUCER_VIEW(c_reducer), (Vector *)(arr + (i % size)));
    }

    COM_REDUCER_MERGE(c_reducer);
}

static inline uint64_t check_bins(Vector * correct, int check) {
    switch(check) {
    case 0:
        for (uint64_t i = 0; i < bins; i++) {
            if (correct->ele[i] != serial_reducer.ele[i]) return 0;
        }
        return 1;
    case 1:
        for (uint64_t i = 0; i < bins; i++) {
            if (correct->ele[i] != REDUCER_VIEW(a_reducer).ele[i]) return 0;
        }
        return 1;
    case 2:
        for (uint64_t i = 0; i < bins; i++) {
            if (correct->ele[i] != COM_REDUCER_VIEW(c_reducer).ele[i]) return 0;
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
    arr = (uint64_t *) malloc((1000000 + 256) * sizeof(uint64_t));

    fill_array(arr, 1000000);
    Vector * correct = get_correct(arr, n, 1000000);

    for (i = 0; i < TIMING_COUNT; i++) {
        prepare();
        begin = ktiming_getmark();

        switch(check) {
        case 0:
            test_reducer_serial(arr, n, 1000000); break;
        case 1:
            test_reducer_associative(arr, n, 1000000); break;
        case 2:
            test_reducer_commutative(arr, n, 1000000); break;
        default:
            break;
        }

        end = ktiming_getmark();
        res += check_bins(correct, check) ? 1 : 0;
        cleanup();
        running_time[i] = ktiming_diff_nsec(&begin, &end);
    }
    printf("Result: %d/%d successes!\n", res, TIMING_COUNT);
    print_runtime(running_time, TIMING_COUNT);

    return res != TIMING_COUNT;
}
