/* this is Matteo's idea of how the all-pairs shortest path problem
   should be solved in Cilk */
#include <cilk/cilk.h>

#include <stdlib.h>
#include <stdio.h>
#include <assert.h>

extern "C" {
#include "ktiming.h"
}


#define SUB(A, lda, i, j) (A)[(lda) * (j) + (i)]

typedef float REAL;

static inline REAL rmin(REAL a, REAL b)
{
     return (a < b) ? a : b;
}

void kernel(REAL *A, int lda, int i, int j, int k)
{
     SUB(A, lda, i, j) =
	  rmin(SUB(A, lda, i, j),
	       SUB(A, lda, i, k) + SUB(A, lda, k, j));
}

bool between(int x, int y0, int y1)
{
     return (y0 <= x && x < y1);
}

/* TRUE if the ranges [x0,x1) and [y0,y1) overlap */
bool overlaps(int x0, int x1, int y0, int y1)
{
     return between(x0, y0, y1) || between(y0, x0, x1);
}

void floyd(REAL *A, int lda,
	   int i0, int i1,
	   int j0, int j1,
	   int k0, int k1)
{
     int i, j, k;
     for (k = k0; k < k1; ++k)
	  for (i = i0; i < i1; ++i)
	       for (j = j0; j < j1; ++j)
		    kernel(A, lda, i, j, k);
}

void recur(REAL *A, int lda,
	   int i0, int i1,
	   int j0, int j1,
	   int k0, int k1)
{
     int di = i1 - i0, dj = j1 - j0, dk = k1 - k0;

     if (di >= dj && di >= dk && di > 1) {
	  int im = (i0 + i1) / 2;
	  if (overlaps(i0, im, k0, k1)) {
	       if (overlaps(im, i1, k0, k1)) {
		    /* cannot cut i */
	       } else {
		    recur(A, lda, i0, im, j0, j1, k0, k1);
		    recur(A, lda, im, i1, j0, j1, k0, k1);
		    return;
	       }
	  } else {
	       cilk_spawn recur(A, lda, im, i1, j0, j1, k0, k1);
	       if (overlaps(im, i1, k0, k1)) 
		    cilk_sync;
	       recur(A, lda, i0, im, j0, j1, k0, k1);
	       return;
	  }
     }

     if (dj >= dk && dj > 1) {
	  int jm = (j0 + j1) / 2;
	  if (overlaps(j0, jm, k0, k1)) {
	       if (overlaps(jm, j1, k0, k1)) {
		    /* cannot cut j */
	       } else {
		    recur(A, lda, i0, i1, j0, jm, k0, k1);
		    recur(A, lda, i0, i1, jm, j1, k0, k1);
		    return;
	       } 
	  } else {
	       cilk_spawn recur(A, lda, i0, i1, jm, j1, k0, k1);
	       if (overlaps(jm, j1, k0, k1)) 
		    cilk_sync;
	       recur(A, lda, i0, i1, j0, jm, k0, k1);
	       return;
	  }
     }
	
     if (dk > 1) {
	  int km = (k0 + k1) / 2;
	  recur(A, lda, i0, i1, j0, j1, k0, km);
	  recur(A, lda, i0, i1, j0, j1, km, k1);
	  return;
     } 

     /* base case: */
     floyd(A, lda, i0, i1, j0, j1, k0, k1);
}


int test(int N)
{
	int res = 1;

     REAL *A0 = new REAL[N*N];
     REAL *A1 = new REAL[N*N];
     const int lda = N;
     int i, j, r;

     for (i = 0; i < N; ++i) {
	  for (j = 0; j < N; ++j) 
	       SUB(A0, lda, i, j) = SUB(A1, lda, i, j) = 1.0e30;
     }
     
     for (r = 0; r < 5 * N; ++r) {
	  i = rand() % N;
	  j = rand() % N;
	  SUB(A0, lda, i, j) = SUB(A1, lda, i, j) = REAL(rand() % N);
     }

     recur(A0, lda, 0, N, 0, N, 0, N);
     floyd(A1, lda, 0, N, 0, N, 0, N);

     for (i = 0; i < N; ++i)
	  for (j = 0; j < N; ++j)
	       if (SUB(A0, lda, i, j) != SUB(A1, lda, i, j)) res = 0;

     delete [] A0;
     delete [] A1;

	return res;
}

int main(int argc, char *argv[])
{
	int res = 0;
	clockmark_t begin, end;
	begin = ktiming_getmark();
     for (int N = 0; N < 200; ++N) {
	  res += test(N);
	  //printf("%d OK\n", N);
     }
	end = ktiming_getmark();
	printf("%g\n,", ktiming_diff_sec(&begin, &end));

     return res != 200;
}
