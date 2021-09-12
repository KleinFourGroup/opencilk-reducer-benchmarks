#include <cilk/cilk.h>
#include <cilk/reducer.h>
#include <stdio.h>
#include <stdlib.h>

#include "ktiming.h"

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

uint64_t intsum_serial(uint64_t * restrict arr, uint64_t num, uint64_t size) {
    uint64_t sum = 0;
    for (uint64_t i = 0; i < num; i++) {
        sum += arr[i % size];
    }

    return sum;
}

uint64_t intsum_cilk(uint64_t * restrict arr, uint64_t num, uint64_t size) {
    uint64_t sum = 0;
    cilk_for (uint64_t i = 0; i < num; i++) {
        sum += arr[i % size];
    }

    return sum;
}

int main(int argc, const char **args) {
    uint64_t i, n;
    uint64_t * arr = NULL;

    int res = 0;
    clockmark_t begin, end;
    uint64_t running_time[TIMING_COUNT];

    if (argc != 2) {
        fprintf(stderr, "Usage: ilist_dac [<cilk-options>] <n> <check?>\n");
        exit(1);
    }

    n = atol(args[1]);

    arr = (uint64_t *) malloc(1000000 * sizeof(uint64_t));

    fill_array(arr, 1000000);
    const uint64_t correct = intsum_serial(arr, n, 1000000);

    for (i = 0; i < TIMING_COUNT; i++) {
        uint64_t ret = 0;
        begin = ktiming_getmark();

        ret = intsum_cilk(arr, n, 1000000);

        res += (ret == correct) ? 1 : 0;
        end = ktiming_getmark();
        running_time[i] = ktiming_diff_nsec(&begin, &end);
    }
    fprintf(stderr, "Result: %d/%d successes!\n", res, TIMING_COUNT);
    print_runtime(running_time, TIMING_COUNT);
    printf(",");

    return res != TIMING_COUNT;
}
