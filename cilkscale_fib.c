#include <cilk/cilk.h>
#include <cilk/reducer.h>
#include <stdio.h>
#include <stdlib.h>

#include "ktiming.h"

uint64_t fib_result_serial;

uint64_t fib_serial(int n) {
    if (n == 0) return 0;
    if (n == 1) return 1;
    uint64_t x, y;
    x = fib_serial(n - 1);
    y = fib_serial(n - 2);

    return x + y;
}

uint64_t __attribute__((weak)) fib_cilk(int n) {
    if (n == 0) return 0;
    if (n == 1) return 1;
    uint64_t x, y;
    x = cilk_spawn fib_cilk(n - 1);
    y = fib_cilk(n - 2);
    cilk_sync;

    return x+y;
}

int main(int argc, const char **args) {
    uint64_t i, n;

    int res = 0;
    clockmark_t begin, end;
    uint64_t running_time[TIMING_COUNT];

    if (argc != 2) {
        fprintf(stderr, "Usage: ilist_dac [<cilk-options>] <n> <check?>\n");
        exit(1);
    }

    n = atol(args[1]);

    uint64_t correct = fib_serial(n);

    for (i = 0; i < TIMING_COUNT; i++) {
        uint64_t ret = 0;
        begin = ktiming_getmark();

        ret = fib_cilk(n);

        res += (ret == correct) ? 1 : 0;
        end = ktiming_getmark();
        running_time[i] = ktiming_diff_nsec(&begin, &end);
    }
    printf("Result: %d/%d successes!\n", res, TIMING_COUNT);
    print_runtime(running_time, TIMING_COUNT);

    return res != TIMING_COUNT;
}
