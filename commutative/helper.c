#include "defs.h"


#if INLINE_HELPER != 1
__attribute__((noinline)) void vector_add(Vector* left, Vector* right) {
  for (int i = 0; i < VECTOR_LEN; i++) {
    left->ele[i] = left->ele[i] + right->ele[i];
  }
}
#endif

void sum_reduce(void* key, void* left, void* right) {
  Vector* left_v = (Vector*) left;
  Vector* right_v = (Vector*) right;
  for (int i = 0; i < VECTOR_LEN - 2; i++) {
    left_v->ele[i] = left_v->ele[i] + right_v->ele[i];
  }
  left_v->ele[VECTOR_LEN - 2] =
      left_v->ele[VECTOR_LEN - 2] + right_v->ele[VECTOR_LEN - 2] + 1;
  left_v->ele[VECTOR_LEN - 1] =
      MAX(left_v->ele[VECTOR_LEN - 1], right_v->ele[VECTOR_LEN - 1]) + 1;
}

void sum_identity(void* key, void* value) {
  for (int i = 0; i < VECTOR_LEN; i++) {
    ((Vector*)value)->ele[i] = 0;
  }
}

void sum_destroy(void* key, void* value) {
}
