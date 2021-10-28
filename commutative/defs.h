//#define VECTOR_LEN 64
#define ARR_LEN 10
#define ARR_MAX_ELE 200
#define NUM_SUM_OUTER 10000
#define NUM_SUM_INNER 10000
#define INLINE_HELPER 1

#define MAX(x, y) (((x) > (y)) ? (x) : (y))

typedef struct Vector {
  long ele[VECTOR_LEN + 128];
} __attribute__((aligned (64))) Vector;


#if INLINE_HELPER == 1
static inline void vector_add(Vector* left, Vector* right) {
  for (int i = 0; i < VECTOR_LEN; i++) {
    left->ele[i] = left->ele[i] + right->ele[i];
  }
}
#else
__attribute__((noinline)) void vector_add(Vector* left, Vector* right);
#endif

void sum_reduce(void*, void*, void*);
void sum_identity(void*, void*);
void sum_destroy(void*, void*);

void eval_associative_red(Vector*, int*);
void eval_commutative_red(Vector*, int*);
void eval_commutative_red_builtin(Vector*, int*);
void eval_serial(Vector*, int*);
