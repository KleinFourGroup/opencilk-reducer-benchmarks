/**
 * Copyright (c) 2012 MIT License by 6.172 Staff
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to
 * deal in the Software without restriction, including without limitation the
 * rights to use, copy, modify, merge, publish, distribute, sublicense, and/or
 * sell copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
 * IN THE SOFTWARE.
 **/

/**
 * Linux kernel-assisted timing library -- provides high-precision time
 * measurements for the execution time of your algorithms.
 *
 * You shouldn't need to modify this file. More importantly, you should not
 * depend on any modifications you make here, as we will replace it with a
 * fresh copy when we test your code.
 **/

#include "./ktiming.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

#define NSEC_TO_SEC(x) ((double)(x)*1.0e-9)

uint64_t extzero = 0;

clockmark_t ktiming_getmark(void) {
    struct timespec temp = {0, 0};
    uint64_t nanos;

    int stat = clock_gettime(CLOCK_MONOTONIC, &temp);
    if (stat != 0) {
        perror("ktiming_getmark()");
        exit(-1);
    }
    nanos = temp.tv_nsec;
    nanos += ((uint64_t)temp.tv_sec) * 1000 * 1000 * 1000;
    return nanos;
}

uint64_t ktiming_diff_nsec(const clockmark_t *const start,
                           const clockmark_t *const end) {
    return *end - *start;
}

double ktiming_diff_sec(const clockmark_t *const start,
                        const clockmark_t *const end) {
    return NSEC_TO_SEC(ktiming_diff_nsec(start, end));
}

void sort(uint64_t * arr, int n) {
    int i, j;
    uint64_t comp;
    for (i = 1; i < n; i++) {
        comp = arr[i];
        j = i - 1;
        
        while (comp < arr[j] && j >= 0) {
            arr[j + 1] = arr[j];
            j--;
        }
        
        arr[j + 1] = comp;
    }
}

static void print_runtime_helper(uint64_t *nsec_elapsed, int size,
                                 int summary) {

    int i;
    uint64_t total = 0;
    uint64_t min = (uint64_t)(-1); // Hurray for overflow
    uint64_t med;
    double ave, std_dev = 0, dev_sq_sum = 0;

    for (i = 0; i < size; i++) {
        // min = (nsec_elapsed[i] < min) ? nsec_elapsed[i] : min;
        // total += nsec_elapsed[i];
        if (!summary) {
            printf("Running time %d: %gs\n", (i + 1),
                    NSEC_TO_SEC(nsec_elapsed[i]));
        }
    }
    // ave = total / size;
    sort(nsec_elapsed, size);

    if (size % 2 == 1) {
        med = nsec_elapsed[size / 2];
    } else {
        med = (nsec_elapsed[(size - 1) / 2] + nsec_elapsed[size / 2]) / 2;
    }

    //printf("Running time average: %g s\n", NSEC_TO_SEC(ave));
    //printf("%g\n", NSEC_TO_SEC(ave));
    //printf("%g\n", NSEC_TO_SEC(min));
    printf("%g\n", NSEC_TO_SEC(med));
}

void print_runtime(uint64_t *tm_elapsed, int size) {
    print_runtime_helper(tm_elapsed, size, 0);
}

void print_runtime_summary(uint64_t *tm_elapsed, int size) {
    print_runtime_helper(tm_elapsed, size, 1);
}

void noop() {}
