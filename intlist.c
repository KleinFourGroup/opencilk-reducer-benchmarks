#include <cilk/cilk.h>
#include <cilk/reducer.h>
#include <stdio.h>
#include <stdlib.h>

#include "ktiming.h"

typedef struct _intListNode {
  int value;
  struct _intListNode* next;
} IntListNode;
typedef struct { IntListNode* head; IntListNode* tail; } IntList;

// Initialize a list to be empty
void IntList_init(IntList* list) {
  list->head = list->tail = NULL;
}

// Append an integer to the list
void IntList_append(IntList* list, int x)
{
  IntListNode* node = (IntListNode*) malloc(sizeof(IntListNode));
  node->value = x;
  node->next = NULL;
    
  if (list->tail) {
    list->tail->next = node;
  } else {
    list->head = node;
  }
  list->tail = node;
}

// Append the right list to the left list, and leave the right list
// empty
void IntList_concat(IntList* left, IntList* right)
{
  if (left->head) {
    left->tail->next = right->head;
    if (right->tail) left->tail = right->tail;
  }
  else {
    *left = *right;
  }
  IntList_init(right);
}

void IntList_free(IntList* list)
{
  IntListNode* node = list->head;
  IntListNode* old;

  while (node != NULL) {
    old = node;
    node = node->next;
    free(old);
  }

  IntList_init(list);
}

void identity_IntList(void* reducer, void* list)
{
  IntList_init((IntList*)list);
}

void reduce_IntList(void* reducer, void* left, void* right)
{
  IntList_concat((IntList*)left, (IntList*)right);
}

void free_IntList(void* reducer, void* list)
{
  IntList_free((IntList*)list);
}

// Append an integer to the list
int IntList_check(IntList* list, int lo, int hi)
{
  IntListNode* node = list->head;
  int curr = lo;
  
  if (hi <= lo) return 0;
    
  while (node != NULL) {
    if (node->value != curr) return 0;
    node = node->next;
    curr++;
  }
  
  if (curr != hi) return 0;
  
  return 1;
}

CILK_C_DECLARE_REDUCER(IntList) int_list_reducer =
    CILK_C_INIT_REDUCER(IntList, reduce_IntList, identity_IntList,
                        free_IntList, {0});

IntList int_list_global;
// Initial values omitted //

void ilist_dac_serial(int lo, int hi) {
    int mid, ctr;

    if (hi - lo <= 1) {
        for (ctr = lo; ctr < hi; ctr++)
            IntList_append(&int_list_global, ctr);
        return;
    }

    mid = (lo + hi) / 2;

    ilist_dac_serial(lo, mid);
    ilist_dac_serial(mid, hi);

    return;
}

void ilist_dac_serialred(int lo, int hi) {
    int mid, ctr;

    if (hi - lo <= 1) {
        for (ctr = lo; ctr < hi; ctr++)
            IntList_append(&REDUCER_VIEW(int_list_reducer), ctr);
        return;
    }

    mid = (lo + hi) / 2;

    ilist_dac_serialred(lo, mid);
    ilist_dac_serialred(mid, hi);

    return;
}

void ilist_dac_cilk(int lo, int hi) {
    int mid, ctr;

    if (hi - lo <= 1) {
        for (ctr = lo; ctr < hi; ctr++)
            IntList_append(&REDUCER_VIEW(int_list_reducer), ctr);
        return;
    }

    mid = (lo + hi) / 2;

    cilk_spawn ilist_dac_cilk(lo, mid);

    ilist_dac_cilk(mid, hi);

    cilk_sync;

    return;
}

void ilist_dac_atomic(int lo, int hi) {
    int mid, ctr;

    if (hi - lo <= 1) {
        for (ctr = lo; ctr < hi; ctr++)
            IntList_append(&int_list_global, ctr);
        return;
    }

    mid = (lo + hi) / 2;

    cilk_spawn ilist_dac_atomic(lo, mid);

    ilist_dac_atomic(mid, hi);

    cilk_sync;

    return;
}

int main(int argc, char *args[]) {
    int i;
    int n, res = 0, check = 4;
    clockmark_t begin, end;
    uint64_t running_time[TIMING_COUNT];

    if (argc != 3) {
        fprintf(stderr, "Usage: ilist_dac [<cilk-options>] <n> [<b>]\n");
        exit(1);
    }

    n = atoi(args[1]);

    check = atoi(args[2]);

    for (i = 0; i < TIMING_COUNT; i++) {
        begin = ktiming_getmark();
        CILK_C_REGISTER_REDUCER(int_list_reducer);
        IntList_init(&REDUCER_VIEW(int_list_reducer));
        IntList_init(&int_list_global);

        IntList result;

        switch(check) {
        case 0:
            ilist_dac_serial(0, n);
            result = int_list_global;
            break;
        case 1:
            ilist_dac_serialred(0, n);
            result = REDUCER_VIEW(int_list_reducer);
            break;
        case 2:
            ilist_dac_cilk(0, n);
            result = REDUCER_VIEW(int_list_reducer);
            break;
        case 3:
            ilist_dac_atomic(0, n);
            result = int_list_global;
            break;
        default:
            break;
        }
        
        res += IntList_check(&result, 0, n);

        CILK_C_UNREGISTER_REDUCER(int_list_reducer);
        end = ktiming_getmark();

        IntList_free(&result);
        running_time[i] = ktiming_diff_nsec(&begin, &end);
    }
    printf("Result: %d/%d successes!\n", res, TIMING_COUNT);
    print_runtime(running_time, TIMING_COUNT);

    return res != TIMING_COUNT;
}
