#include <cilk/cilk.h>
#include <cilk/reducer.h>
#include <stdio.h>
#include <stdlib.h>

#include "ktiming.h"

#define THRESHOLD 0

void identity_uint64sum(void *reducer, void *sum) { *((uint64_t *)sum) = 0; }

void reduce_uint64sum(void *reducer, void *left, void *right) {
    *((uint64_t *)left) += *((uint64_t *)right);
}

CILK_C_DECLARE_REDUCER(uint64_t)
fib_result_reducer = CILK_C_INIT_REDUCER(uint64_t, reduce_uint64sum, identity_uint64sum,
                                         0, 0);

uint64_t fib_result_serial = 0;

void fib_serial(int n) {
    if (n == 0) return;
    if (n == 1) {
        fib_result_serial += 1;
        return;
    }
    if (n <= THRESHOLD) {
        fib_serial(n - 1);
        fib_serial(n - 2);
        return;
    }
    // else
    fib_serial(n - 1);
    fib_serial(n - 2);
    return;
}

void fib_redserial(int n) {
    if (n == 0) return;
    if (n == 1) {
        REDUCER_VIEW(fib_result_reducer) += 1;
        return;
    }
    if (n <= THRESHOLD) {
        fib_redserial(n - 1);
        fib_redserial(n - 2);
        return;
    }
    // else
    fib_redserial(n - 1);
    fib_redserial(n - 2);
    return;
}

void fib_cilk(int n) {
    if (n == 0) return;
    if (n == 1) {
        REDUCER_VIEW(fib_result_reducer) += 1;
        return;
    }
    if (n <= THRESHOLD) {
        fib_cilk(n - 1);
        fib_cilk(n - 2);
        return;
    }
    // else
    cilk_spawn fib_cilk(n - 1);
    fib_cilk(n - 2);
    cilk_sync;

    return;
}

void fib_cilkglobal(int n) {
    if (n == 0) return;
    if (n == 1) {
        fib_result_serial += 1;
        return;
    }
    if (n <= THRESHOLD) {
        fib_cilkglobal(n - 1);
        fib_cilkglobal(n - 2);
        return;
    }
    // else
    cilk_spawn fib_cilkglobal(n - 1);
    fib_cilkglobal(n - 2);
    cilk_sync;

    return;
}

int main(int argc, const char **args) {
    uint64_t i, n;

    int check = 0, res = 0;
    clockmark_t begin, end;
    uint64_t running_time[TIMING_COUNT];

    if (argc != 3) {
        fprintf(stderr, "Usage: ilist_dac [<cilk-options>] <n> <check?>\n");
        exit(1);
    }

    n = atol(args[1]);
    check = atol(args[2]);

    fib_serial(n);
    const uint64_t correct = fib_result_serial;

    for (i = 0; i < TIMING_COUNT; i++) {
        uint64_t ret = 0;
        begin = ktiming_getmark();
        CILK_C_REGISTER_REDUCER(fib_result_reducer);
        REDUCER_VIEW(fib_result_reducer) = 0;
        fib_result_serial = 0;

        switch(check) {
        case 0:
            fib_serial(n); ret = fib_result_serial; break;
        case 1:
            fib_redserial(n); ret = REDUCER_VIEW(fib_result_reducer); break;
        case 2:
            fib_cilk(n); ret = REDUCER_VIEW(fib_result_reducer); break;
        case 3:
            fib_cilkglobal(n); ret = fib_result_serial; break;
        default:
            break;
        }

        res += (ret == correct) ? 1 : 0;
        CILK_C_UNREGISTER_REDUCER(fib_result_reducer);
        printf("Calculated: %lu; Correct: %lu\n", ret, correct);
        end = ktiming_getmark();
        // prlongf("The final sum is %d\n", sum);
        running_time[i] = ktiming_diff_nsec(&begin, &end);
    }
    printf("Result: %d/%d successes!\n", res, TIMING_COUNT);
    print_runtime(running_time, TIMING_COUNT);

    return res != TIMING_COUNT;
}
