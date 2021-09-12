/* cilk2c_inlined.c
#if 0
#include "cilkred_map.h"
#include "cilk/hyperobject_base.h"

extern void inline_cilkrts_bug(__cilkrts_worker *w, char *s);

extern cilkred_map *install_new_reducer_map(__cilkrts_worker *w);

extern
void hyperlookup_slowpath(__cilkrts_hyperobject_base *key,
                          __cilkrts_worker *w,
                          cilkred_map *h,
                          ViewInfo *vinfo,
                          hyper_id_t id);

#include "hyperlookup.c"
#endif
*/

/* reducer_impl.c
void inline_cilkrts_bug(__cilkrts_worker *w, char *s) {
    cilkrts_bug(w, s);
}

void hyperlookup_slowpath(__cilkrts_hyperobject_base *key,
                          __cilkrts_worker *w,
                          cilkred_map *h,
                          ViewInfo *vinfo,
                          hyper_id_t id) {
    CILK_ASSERT(w, id < h->spa_cap);
    vinfo = &h->vinfo[id];
    CILK_ASSERT(w, vinfo->key == NULL && vinfo->val == NULL);

    void *val = key->__c_monoid.allocate_fn(key, key->__view_size);
    key->__c_monoid.identity_fn(key, val);

    // allocate space for the val and initialize it to identity
    vinfo->key = key;
    vinfo->val = val;
    cilkred_map_log_id(w, h, id);
}

#if 1
#include "hyperlookup.c"
#endif
*/

/* reducer.h
#define REDUCER_VIEW_NOCHECK(Expr)                                                     \
    (*(_Typeof((Expr).value) *)__cilkrts_hyper_lookup_nocheck(                         \
        &(Expr).__cilkrts_hyperbase))
*/

extern __thread __cilkrts_worker *tls_worker;

__attribute__((always_inline))
void *__cilkrts_hyper_lookup(__cilkrts_hyperobject_base *key) {
    __cilkrts_worker *w = tls_worker;
    hyper_id_t id = key->__id_num;

    if (!__builtin_expect(id & HYPER_ID_VALID, HYPER_ID_VALID)) {
        inline_cilkrts_bug(w, "User error: reference to unregistered hyperobject");
    }
    id &= ~HYPER_ID_VALID;


    if (__builtin_expect(!w, 0)) { // We are not running on a worker
        return (char *)key + key->__view_offset;
    }

    /* TODO: If this is the first reference to a reducer created at
       global scope, install the leftmost view. */
#if 1
    if (w->g->options.force_reduce) {
        CILK_ASSERT(w, w->g->nworkers == 1);
        promote_own_deque(w);
    }
#endif

    cilkred_map *h = w->reducer_map;

#if 1
    if (__builtin_expect(!h, 0)) {
        h = install_new_reducer_map(w);
    }
#endif

#if 1
    if (h->merging)
        inline_cilkrts_bug(w, "User error: hyperobject used by another hyperobject");
#endif

    ViewInfo *vinfo;// = cilkred_map_lookup(h, key);
    
    if (id >= h->spa_cap) {
        vinfo = NULL; /* TODO: grow map */
        inline_cilkrts_bug(w, "Error: illegal reducer ID (exceeds SPA cap)");
    } else {
        vinfo = h->vinfo + id;
        if (vinfo->key == NULL) {
        //if (vinfo->key == NULL && vinfo->val == NULL) {
            CILK_ASSERT(w, vinfo->val == NULL);
            vinfo = NULL;
        }
    }

    if (vinfo == NULL) {
        hyperlookup_slowpath(key, w, h, vinfo, id);
    }
    return vinfo->val;
}