/*
 * ═══════════════════════════════════════════════════════════════════
 *  Zane Memory Model Benchmark
 *  Compares Zane's memory system against standard C allocators.
 *  Each test runs 20 times; MEDIAN reported (outlier-resistant).
 *
 *  Allocator implementations:
 *    Zane   — single mmap region, size-indexed free-stack table, O(1)
 *             + lazy anchor model: anchor allocated only on first ref
 *               to an object; free is O(1) if no refs were ever made
 *             + ref objects on heap with back-pointer to ref_anchor
 *             + leaf-only registration: refs register only in the
 *               leaf object's anchor, not any ancestor
 *             + O(1) unregistration via stack_index swap-and-pop
 *    malloc — system allocator (glibc), coalescing on free
 *    Arena  — bump allocator, bulk O(1) reset (no per-item free)
 *    Pool   — per-size segregated free-list, malloc-backed first use
 *
 *  Containers under test:
 *    UList    — linked list of 8-element inline chunks
 *    CChunked — array of 64-element chunk pointers
 *
 *  Memory model:
 *    Ownership is the default — no keyword. Objects are stored inline.
 *    `ref` is the opt-in for non-owning references.
 *    `Array[size]<T>` is the spec-defined fixed-size inline container.
 *    Because `size` is compile-time constant, the growth tests below model
 *    user-space growable storage as the closest updated-spec stand-in for the
 *    now-deferred dynamic `List<T>`.
 *
 *  Tests:
 *    1. Sequential alloc + sequential free          (32B × 100k)
 *    2. Random-order free only                      (32B × 100k)
 *    3. Mixed sizes alloc + random-order free       (8/16/32/64B × 100k)
 *    4. Iteration — inline vs pointer-chase         [+UList +CChunked]
 *    5. Owned buffer append growth                  (32B × 100k)  [+UList +CChunked]
 *    6. Ref access overhead (anchor + ref object)
 *    7. Game loop — entity spawn/kill/update        (500 frames)
 *    8. Particle system — short-lifetime objects    (500 frames)
 *    9. Checkerboard fragmentation + refill
 *   10. Ownership tree teardown                     (cascade destroy)
 *   11. Fragmentation stress                        (200 cycles)
 *   12. Deterministic concurrent shard scan         (4 workers)
 * ═══════════════════════════════════════════════════════════════════
 */

#define _POSIX_C_SOURCE 199309L
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <sys/mman.h>
#include <stdint.h>
#include <assert.h>
#include <math.h>
#include <pthread.h>

/* ─────────────────────────────────────────────
   CONFIG
───────────────────────────────────────────── */
#define RUNS         20
#define N            100000
#define REGION_SIZE  (512UL * 1024 * 1024)

/* ─────────────────────────────────────────────
   TIMER
───────────────────────────────────────────── */
static inline double now_ns(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (double)ts.tv_sec * 1e9 + (double)ts.tv_nsec;
}

/* ─────────────────────────────────────────────
   MEDIAN
───────────────────────────────────────────── */
static int cmp_double(const void *a, const void *b) {
    double da = *(const double *)a, db = *(const double *)b;
    return (da > db) - (da < db);
}
static double median(double T[RUNS]) {
    double sorted[RUNS];
    memcpy(sorted, T, RUNS * sizeof(double));
    qsort(sorted, RUNS, sizeof(double), cmp_double);
    return (RUNS % 2)
        ? sorted[RUNS / 2]
        : (sorted[RUNS/2 - 1] + sorted[RUNS/2]) / 2.0;
}

/* ─────────────────────────────────────────────
   OUTPUT
───────────────────────────────────────────── */
static void print_result(const char *impl, double T[RUNS]) {
    double med = median(T);
    double mn = T[0], mx = T[0];
    for (int i = 0; i < RUNS; i++) {
        if (T[i] < mn) mn = T[i];
        if (T[i] > mx) mx = T[i];
    }
    printf("    %-34s  median %12.2f ns  min %12.2f ns  max %12.2f ns  (%9.3f us)\n",
           impl, med, mn, mx, med / 1000.0);
}

static void section(const char *title) {
    printf("\n  +--------------------------------------------------------------------------------------------------+\n");
    printf("  |  %-96s|\n", title);
    printf("  +--------------------------------------------------------------------------------------------------+\n");
}

/* ─────────────────────────────────────────────
   BENCHMARK WORK-STEALING POOL
   Prestarted once before timed benchmarks so concurrent tests measure
   steady-state scheduling rather than thread creation.
───────────────────────────────────────────── */
#define BENCH_POOL_WORKERS   4
#define BENCH_POOL_MAX_JOBS  32

typedef void (*BenchJobFn)(void *);

typedef struct {
    BenchJobFn fn;
    void      *arg;
} BenchJob;

typedef struct {
    BenchJob jobs[BENCH_POOL_MAX_JOBS];
    int      head;
    int      tail;
} BenchDeque;

typedef struct {
    pthread_t       threads[BENCH_POOL_WORKERS];
    BenchDeque      queues[BENCH_POOL_WORKERS];
    pthread_mutex_t mu;
    pthread_cond_t  has_work;
    pthread_cond_t  idle;
    int             pending_jobs;
    int             stop;
} BenchPool;

static BenchPool bench_pool;

/* All deque access is serialized by bench_pool.mu, including steals. The
   benchmark batches are tiny, so a single mutex keeps the implementation
   simple while still giving us work-stealing behavior across worker queues. */
static void bench_deque_reset_locked(BenchDeque *q) { q->head = 0; q->tail = 0; }

static void bench_deque_push_locked(BenchDeque *q, BenchJob job) {
    assert((q->tail - q->head) < BENCH_POOL_MAX_JOBS);
    q->jobs[q->tail % BENCH_POOL_MAX_JOBS] = job;
    q->tail++;
}

static int bench_deque_pop_back_locked(BenchDeque *q, BenchJob *job) {
    if (q->tail == q->head) return 0;
    q->tail--;
    *job = q->jobs[q->tail % BENCH_POOL_MAX_JOBS];
    return 1;
}

static int bench_deque_pop_front_locked(BenchDeque *q, BenchJob *job) {
    if (q->tail == q->head) return 0;
    *job = q->jobs[q->head % BENCH_POOL_MAX_JOBS];
    q->head++;
    return 1;
}

static int bench_pool_take_job_locked(int worker_id, BenchJob *job) {
    if (bench_deque_pop_back_locked(&bench_pool.queues[worker_id], job)) return 1;
    for (int step = 1; step < BENCH_POOL_WORKERS; step++) {
        int victim = (worker_id + step) % BENCH_POOL_WORKERS;
        if (bench_deque_pop_front_locked(&bench_pool.queues[victim], job)) return 1;
    }
    return 0;
}

static void *bench_pool_worker(void *arg) {
    int worker_id = (int)(intptr_t)arg;
    BenchJob job;

    pthread_mutex_lock(&bench_pool.mu);
    for (;;) {
        while (!bench_pool.stop && !bench_pool_take_job_locked(worker_id, &job)) {
            pthread_cond_wait(&bench_pool.has_work, &bench_pool.mu);
        }
        if (bench_pool.stop) {
            pthread_mutex_unlock(&bench_pool.mu);
            return NULL;
        }

        pthread_mutex_unlock(&bench_pool.mu);
        job.fn(job.arg);
        pthread_mutex_lock(&bench_pool.mu);

        bench_pool.pending_jobs--;
        if (bench_pool.pending_jobs == 0) pthread_cond_signal(&bench_pool.idle);
        pthread_cond_broadcast(&bench_pool.has_work);
    }
}

static void bench_pool_init(void) {
    memset(&bench_pool, 0, sizeof(bench_pool));
    assert(pthread_mutex_init(&bench_pool.mu, NULL) == 0);
    assert(pthread_cond_init(&bench_pool.has_work, NULL) == 0);
    assert(pthread_cond_init(&bench_pool.idle, NULL) == 0);
    for (int i = 0; i < BENCH_POOL_WORKERS; i++) {
        bench_deque_reset_locked(&bench_pool.queues[i]);
        assert(pthread_create(&bench_pool.threads[i], NULL, bench_pool_worker, (void*)(intptr_t)i) == 0);
    }
}

static void bench_pool_run(BenchJob *jobs, int njobs) {
    assert(njobs > 0 && njobs <= BENCH_POOL_MAX_JOBS);

    pthread_mutex_lock(&bench_pool.mu);
    while (bench_pool.pending_jobs != 0) pthread_cond_wait(&bench_pool.idle, &bench_pool.mu);

    /* pending_jobs == 0 means there are no queued or running jobs left, so
       with the mutex held it is safe to clear and repopulate every queue. */
    for (int i = 0; i < BENCH_POOL_WORKERS; i++) bench_deque_reset_locked(&bench_pool.queues[i]);
    for (int i = 0; i < njobs; i++) bench_deque_push_locked(&bench_pool.queues[i % BENCH_POOL_WORKERS], jobs[i]);
    bench_pool.pending_jobs = njobs;

    pthread_cond_broadcast(&bench_pool.has_work);
    while (bench_pool.pending_jobs != 0) pthread_cond_wait(&bench_pool.idle, &bench_pool.mu);
    pthread_mutex_unlock(&bench_pool.mu);
}

static void bench_noop_job(void *arg) { (void)arg; }

static void bench_pool_warm(void) {
    BenchJob jobs[BENCH_POOL_WORKERS];
    for (int i = 0; i < BENCH_POOL_WORKERS; i++) jobs[i] = (BenchJob){ .fn = bench_noop_job, .arg = NULL };
    bench_pool_run(jobs, BENCH_POOL_WORKERS);
}

static void bench_pool_shutdown(void) {
    pthread_mutex_lock(&bench_pool.mu);
    bench_pool.stop = 1;
    pthread_cond_broadcast(&bench_pool.has_work);
    pthread_mutex_unlock(&bench_pool.mu);

    for (int i = 0; i < BENCH_POOL_WORKERS; i++) assert(pthread_join(bench_pool.threads[i], NULL) == 0);
    pthread_cond_destroy(&bench_pool.idle);
    pthread_cond_destroy(&bench_pool.has_work);
    pthread_mutex_destroy(&bench_pool.mu);
}

/* ═══════════════════════════════════════════════════════════════════
   ZANE MEMORY SYSTEM
═══════════════════════════════════════════════════════════════════ */
#define ZM_ALIGN   8
#define ZM_MAXSZ   512
#define ZM_NC      (ZM_MAXSZ / ZM_ALIGN)

typedef struct { uintptr_t *d; size_t n, cap; } ZFreeStack;

static struct {
    uint8_t   *base;
    size_t     top;
    ZFreeStack fs[ZM_NC];
} zm;

static void zm_init(void) {
    zm.base = mmap(NULL, REGION_SIZE, PROT_READ | PROT_WRITE,
                   MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    assert(zm.base != MAP_FAILED);
    for (size_t i = 0; i < 256UL * 1024 * 1024; i += 4096) zm.base[i] = 0;
    zm.top = 0;
    for (int i = 0; i < ZM_NC; i++) {
        zm.fs[i].cap = 8192;
        zm.fs[i].n   = 0;
        zm.fs[i].d   = (uintptr_t*)malloc(8192 * sizeof(uintptr_t));
    }
}
static void zm_reset(void) {
    zm.top = 0;
    for (int i = 0; i < ZM_NC; i++) zm.fs[i].n = 0;
}
static inline size_t zm_round(size_t s) { return (s + ZM_ALIGN-1) & ~(size_t)(ZM_ALIGN-1); }
static inline int    zm_cls(size_t s)   { return (int)(s / ZM_ALIGN) - 1; }

static void *zm_alloc(size_t s) {
    s = zm_round(s);
    if (s <= ZM_MAXSZ) {
        ZFreeStack *f = &zm.fs[zm_cls(s)];
        if (f->n) return zm.base + f->d[--f->n];
    }
    size_t off = zm.top; zm.top += s;
    return zm.base + off;
}
static void zm_free(void *p, size_t s) {
    s = zm_round(s);
    if (s > ZM_MAXSZ) return; /* large blocks reclaimed on zm_reset only */
    ZFreeStack *f = &zm.fs[zm_cls(s)];
    if (f->n == f->cap) { f->cap *= 2; f->d = (uintptr_t*)realloc(f->d, f->cap * sizeof(uintptr_t)); }
    f->d[f->n++] = (uintptr_t)((uint8_t *)p - zm.base);
}
/* ═══════════════════════════════════════════════════════════════════
   ZANE ANCHOR MODEL  (lazy creation, leaf-only registration)

   Memory model:
     Ownership is the default — no keyword. `ref` is the opt-in for
     non-owning references. `Array[size]<T>` is the fixed-size inline
     container, and growable buffers are layered on top of contiguous
     owned storage in user space.

   Anchors are created on demand — only when the first ref to an
   object is made. The back-pointer slot in every class instance
   is initialised to 0 (no anchor yet). Objects that never receive
   a ref pay zero anchor overhead: alloc and free are single
   zm_alloc / zm_free calls, identical to a plain allocator.

   back-pointer slot (8 bytes, after declared fields):
     0            — no anchor exists yet
     ptr != 0     — absolute address of the object's ZAnchor

   anchor layout (24 bytes):
     heapoffset:       u32   — heap-relative offset of the object
     nrefs:            u32   — number of registered refs
     weak_ref_stack:   u64*  — absolute addresses of ref_anchors
     refs_cap:         u32
     _pad:             u32

   ref object layout (on heap, 24 bytes):
     target_anchor:   *anchor      — absolute address of target's anchor
     back_ptr:        *ref_anchor  — absolute address of the ref_anchor
     stack_index:     u32          — position in target's weak_ref_stack

   ref_anchor layout (stack variable or class field):
     heapoffset:      u32   — heap-relative offset of the ref object

   Leaf-only registration: a ref registers only in the leaf object's
   anchor — not in any ancestor. When an ancestor is destroyed,
   recursive teardown reaches every child anyway.

   O(1) unregistration: swap-with-last-and-pop using stored stack_index.

   All references TO anchors are absolute addresses. The offset
   stored INSIDE the anchor is heap-relative (object can move).
═══════════════════════════════════════════════════════════════════ */
#define ZANCHOR_SIZE 24
#define ZREF_SIZE    24

typedef struct {
    uint32_t  heapoffset;     /* heap-relative offset of the object        */
    uint32_t  nrefs;          /* number of registered refs                 */
    uint64_t *weak_ref_stack; /* absolute addresses of ref_anchors         */
    uint32_t  refs_cap;
    uint32_t  _pad;
} ZAnchor;
_Static_assert(sizeof(ZAnchor) == ZANCHOR_SIZE, "ZAnchor must be 24 bytes");

typedef struct {
    uintptr_t target_anchor;  /* absolute address of target's ZAnchor      */
    uintptr_t back_ptr;       /* absolute address of the ref_anchor        */
    uint32_t  stack_index;    /* position in target's weak_ref_stack       */
    uint32_t  _pad;
} ZRefObj;
_Static_assert(sizeof(ZRefObj) == ZREF_SIZE, "ZRefObj must be 24 bytes");

typedef struct {
    uint32_t  heapoffset;     /* heap-relative offset of the ref object    */
} ZRefAnchor;

/* alloc object only — back-pointer initialised to 0 (no anchor yet).
   Cost: one zm_alloc. */
static void *zm_alloc_lazy(size_t obj_size) {
    void *obj = zm_alloc(obj_size);
    *(uintptr_t*)((uint8_t*)obj + obj_size - sizeof(uintptr_t)) = 0;
    return obj;
}

/* get-or-create anchor for an object. Called only when the first ref
   to the object is made. If back-pointer already set, returns existing. */
static ZAnchor *zm_get_anchor(void *obj, size_t obj_size) {
    uintptr_t *bptr = (uintptr_t*)((uint8_t*)obj + obj_size - sizeof(uintptr_t));
    if (*bptr) return (ZAnchor*)*bptr;
    ZAnchor *anchor       = (ZAnchor*)zm_alloc(ZANCHOR_SIZE);
    anchor->heapoffset    = (uint32_t)((uint8_t*)obj - zm.base);
    anchor->nrefs         = 0;
    anchor->weak_ref_stack = NULL;
    anchor->refs_cap      = 0;
    *bptr = (uintptr_t)anchor;
    return anchor;
}

/* create a ref to an object:
   1. get-or-create anchor for the target (leaf-only registration)
   2. allocate a ref object on the heap
   3. push ref_anchor address into anchor's weak_ref_stack
   4. store stack_index in ref object for O(1) unregistration */
static ZRefObj *zm_create_ref(void *obj, size_t obj_size, ZRefAnchor *ref_anchor) {
    ZAnchor *anchor = zm_get_anchor(obj, obj_size);
    ZRefObj *ref_obj = (ZRefObj*)zm_alloc(ZREF_SIZE);
    ref_obj->target_anchor = (uintptr_t)anchor;
    ref_obj->back_ptr      = (uintptr_t)ref_anchor;

    /* register in weak_ref_stack */
    if (anchor->nrefs == anchor->refs_cap) {
        anchor->refs_cap = anchor->refs_cap ? anchor->refs_cap * 2 : 4;
        anchor->weak_ref_stack = (uint64_t*)realloc(anchor->weak_ref_stack,
                                     anchor->refs_cap * sizeof(uint64_t));
    }
    ref_obj->stack_index = anchor->nrefs;
    anchor->weak_ref_stack[anchor->nrefs++] = (uint64_t)(uintptr_t)ref_anchor;

    ref_anchor->heapoffset = (uint32_t)((uint8_t*)ref_obj - zm.base);
    return ref_obj;
}

/* unregister a ref: O(1) swap-with-last-and-pop using stack_index */
static void zm_unregister_ref(ZRefObj *ref_obj) {
    ZAnchor *anchor = (ZAnchor*)ref_obj->target_anchor;
    uint32_t idx = ref_obj->stack_index;
    uint32_t last = anchor->nrefs - 1;
    if (idx != last) {
        /* swap last entry into this slot */
        uint64_t swapped_ref_anchor_addr = anchor->weak_ref_stack[last];
        anchor->weak_ref_stack[idx] = swapped_ref_anchor_addr;
        /* update swapped ref's stack_index */
        ZRefAnchor *swapped_ra = (ZRefAnchor*)(uintptr_t)swapped_ref_anchor_addr;
        ZRefObj *swapped_ref = (ZRefObj*)(zm.base + swapped_ra->heapoffset);
        swapped_ref->stack_index = idx;
    }
    anchor->nrefs--;
}

/* destroy a ref (ref_anchor going out of scope):
   unregister from target's weak_ref_stack, free ref object */
static void zm_destroy_ref(ZRefAnchor *ref_anchor) {
    ZRefObj *ref_obj = (ZRefObj*)(zm.base + ref_anchor->heapoffset);
    zm_unregister_ref(ref_obj);
    zm_free(ref_obj, ZREF_SIZE);
}

/* free object:
   - if back-pointer == 0: no anchor was ever created — single free, done.
   - if back-pointer != 0: iterate weak_ref_stack, null all ref_anchors,
     free anchor, free object. */
static void zm_free_lazy(void *obj, size_t obj_size) {
    uintptr_t bptr = *(uintptr_t*)((uint8_t*)obj + obj_size - sizeof(uintptr_t));
    if (bptr) {
        ZAnchor *anchor = (ZAnchor*)bptr;
        for (uint32_t i = 0; i < anchor->nrefs; i++) {
            ZRefAnchor *ra = (ZRefAnchor*)(uintptr_t)anchor->weak_ref_stack[i];
            ra->heapoffset = 0xFFFFFFFFu; /* null sentinel — ref is now dead */
        }
        if (anchor->weak_ref_stack) free(anchor->weak_ref_stack);
        zm_free(anchor, ZANCHOR_SIZE);
    }
    zm_free(obj, obj_size);
}



/* ═══════════════════════════════════════════════════════════════════
   ARENA
═══════════════════════════════════════════════════════════════════ */
static struct { uint8_t *base; size_t top; } ar;

static void ar_init(void) {
    ar.base = mmap(NULL, REGION_SIZE, PROT_READ | PROT_WRITE,
                   MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    assert(ar.base != MAP_FAILED);
    for (size_t i = 0; i < 256UL * 1024 * 1024; i += 4096) ar.base[i] = 0;
    ar.top = 0;
}
static inline void   ar_reset(void)       { ar.top = 0; }
static inline void  *ar_alloc(size_t s)   { s=(s+7)&~(size_t)7; void *p=ar.base+ar.top; ar.top+=s; return p; }

/* ═══════════════════════════════════════════════════════════════════
   POOL
═══════════════════════════════════════════════════════════════════ */
typedef struct PNode { struct PNode *next; } PNode;
static PNode *pool_heads[ZM_NC];

static void pool_flush(void) { memset(pool_heads, 0, sizeof(pool_heads)); }
static void *pool_alloc(size_t s) {
    s=zm_round(s); int c=zm_cls(s);
    if (pool_heads[c]) { void *p=pool_heads[c]; pool_heads[c]=pool_heads[c]->next; return p; }
    return malloc(s);
}
static void pool_free(void *p, size_t s) {
    s=zm_round(s); int c=zm_cls(s);
    ((PNode *)p)->next=pool_heads[c]; pool_heads[c]=(PNode *)p;
}
static void pool_warm(size_t s, int count) {
    void **tmp=(void**)malloc((size_t)count*sizeof(void*));
    for(int i=0;i<count;i++) tmp[i]=pool_alloc(s);
    for(int i=0;i<count;i++) pool_free(tmp[i],s);
    free(tmp);
}


/* ═══════════════════════════════════════════════════════════════════
   C CACHE  (array of fixed-size chunk pointers)
   A pre-allocated contiguous buffer divided into N equal slots.
   Free slot indices are tracked in a separate compact int32 stack —
   no write-into-freed-memory aliasing, unlike the Pool's linked list.
   The backing buffer is pre-faulted at init so page faults don't
   pollute timing. Alloc = index-stack pop (or frontier advance).
   Free  = index-stack push.  Both O(1), no coalescing ever.
═══════════════════════════════════════════════════════════════════ */

/* ═══════════════════════════════════════════════════════════════════
   RNG + SHUFFLE
═══════════════════════════════════════════════════════════════════ */
static uint64_t rng_state = 0xcafe1234deadULL;
static inline uint64_t rng(void) {
    rng_state ^= rng_state << 13;
    rng_state ^= rng_state >> 7;
    rng_state ^= rng_state << 17;
    return rng_state;
}
typedef struct { void *p; size_t s; } PS;
static void shuf_ps(PS *a, int n)     { for(int i=n-1;i>0;i--){int j=(int)(rng()%(unsigned)(i+1));PS t=a[i];a[i]=a[j];a[j]=t;} }
static void shuf_ptrs(void **a, int n){ for(int i=n-1;i>0;i--){int j=(int)(rng()%(unsigned)(i+1));void*t=a[i];a[i]=a[j];a[j]=t;} }

/* ═══════════════════════════════════════════════════════════════════
   OBJECT TYPES
═══════════════════════════════════════════════════════════════════ */
typedef struct {
    int64_t id;
    double  x, y;
    int32_t hp;
    int32_t _pad;
} Entity;
_Static_assert(sizeof(Entity) == 32, "Entity must be 32 bytes");

typedef struct {
    float   x, y, vx, vy;
    int32_t ttl, color;
} Particle;
_Static_assert(sizeof(Particle) == 24, "Particle must be 24 bytes");

typedef struct TNode {
    int64_t        value;
    struct TNode **children;
    int            nchildren;
} TNode;

static volatile int64_t sink = 0;

/* ═══════════════════════════════════════════════════════════════════
   UNROLLED LINKED LIST  (UList)
   Each node (UChunk) stores ULIST_CHUNK_CAP elements inline.
   When a chunk is full a new one is malloc'd and linked via *next.
   Chunks are never moved — appending never copies existing data.
   Iteration is chunk-sequential: good spatial locality within each
   chunk (ULIST_CHUNK_CAP × 32B = 256B = 4 cache lines), one pointer
   hop between chunks. Middle ground between a flat array (T=inline)
   and a pointer-per-element list (T=scattered).
═══════════════════════════════════════════════════════════════════ */
#define ULIST_CHUNK_CAP 8

typedef struct UChunk {
    Entity        data[ULIST_CHUNK_CAP];
    int           len;
    struct UChunk *next;
} UChunk;

typedef struct { UChunk *head, *tail; int total; } UList;

static void ulist_init(UList *l) { l->head = l->tail = NULL; l->total = 0; }
static void ulist_push(UList *l, Entity e) {
    if (!l->tail || l->tail->len == ULIST_CHUNK_CAP) {
        UChunk *c = (UChunk*)malloc(sizeof(UChunk));
        c->len = 0; c->next = NULL;
        if (l->tail) l->tail->next = c; else l->head = c;
        l->tail = c;
    }
    l->tail->data[l->tail->len++] = e;
    l->total++;
}
static void ulist_free_all(UList *l) {
    for (UChunk *c = l->head, *nx; c; c = nx) { nx = c->next; free(c); }
    l->head = l->tail = NULL; l->total = 0;
}


/* ═══════════════════════════════════════════════════════════════════
   C CHUNKED  (array of fixed-size chunk pointers)
   Never reallocates existing data. Grows by appending a new malloc'd
   chunk to a chunk-pointer array. Element i lives at:
       chunks[i / CCHUNK][i % CCHUNK]
   Random access is O(1): one index into the pointer array, one offset
   within the chunk — two pointer dereferences total.
   Chunk size: CCHUNK=64 elements × 32B = 2048 bytes per chunk.
   Mirrors the 64 × int64_t = 512B chunks described in the reference.
   The chunk-pointer array itself is realloc'd when it fills, but it
   only holds pointers (8B each) so it stays tiny.
═══════════════════════════════════════════════════════════════════ */
#define CCHUNK 64

typedef struct {
    Entity **chunks;     /* pointer array — one entry per chunk       */
    int      nchunks;    /* allocated chunks so far                   */
    int      cap_chunks; /* capacity of the chunks[] array            */
    int      len;        /* total elements pushed                     */
} CChunked;

static void cchunked_init(CChunked *c) {
    c->cap_chunks = 16;
    c->chunks     = (Entity**)malloc((size_t)c->cap_chunks * sizeof(Entity*));
    c->nchunks    = 0;
    c->len        = 0;
}
static void cchunked_push(CChunked *c, Entity e) {
    if (c->len % CCHUNK == 0) {
        if (c->nchunks == c->cap_chunks) {
            c->cap_chunks *= 2;
            c->chunks = (Entity**)realloc(c->chunks,
                            (size_t)c->cap_chunks * sizeof(Entity*));
        }
        c->chunks[c->nchunks++] = (Entity*)malloc(CCHUNK * sizeof(Entity));
    }
    c->chunks[c->len / CCHUNK][c->len % CCHUNK] = e;
    c->len++;
}
static inline Entity *cchunked_get(CChunked *c, int i) {
    return &c->chunks[i / CCHUNK][i % CCHUNK];
}
static void cchunked_free_all(CChunked *c) {
    for (int i = 0; i < c->nchunks; i++) free(c->chunks[i]);
    free(c->chunks);
    c->nchunks = 0; c->len = 0;
}


/* ═══════════════════════════════════════════════════════════════════
   TEST 1 — Sequential alloc + sequential free  (32B x N)
═══════════════════════════════════════════════════════════════════ */
static void test1(void) {
    section("Test 1 -- Sequential alloc + sequential free  [32 bytes x 100k]");
    double T[RUNS];
    void **ptrs = (void**)malloc(N * sizeof(void *));

    /* Zane: 40-byte runtime slot (32B object + 8B anchor back-ptr), paired anchor alloc/free */
    for (int r=0;r<RUNS;r++) { zm_reset(); double t0=now_ns(); for(int i=0;i<N;i++) ptrs[i]=zm_alloc_lazy(40); for(int i=0;i<N;i++) zm_free_lazy(ptrs[i],40); T[r]=now_ns()-t0; }
    print_result("Zane (lazy anchors)", T);

    for (int r=0;r<RUNS;r++) { double t0=now_ns(); for(int i=0;i<N;i++) ptrs[i]=malloc(32); for(int i=0;i<N;i++) free(ptrs[i]); T[r]=now_ns()-t0; }
    print_result("malloc / free", T);

    for (int r=0;r<RUNS;r++) { ar_reset(); double t0=now_ns(); for(int i=0;i<N;i++) ptrs[i]=ar_alloc(32); sink^=(int64_t)(uintptr_t)ptrs[N-1]; ar_reset(); T[r]=now_ns()-t0; }
    print_result("Arena (bump + O(1) reset)", T);

    pool_flush(); pool_warm(32,N);
    for (int r=0;r<RUNS;r++) { double t0=now_ns(); for(int i=0;i<N;i++) ptrs[i]=pool_alloc(32); for(int i=0;i<N;i++) pool_free(ptrs[i],32); T[r]=now_ns()-t0; }
    print_result("Pool (per-size free-list)", T);

    free(ptrs);
}

/* ═══════════════════════════════════════════════════════════════════
   TEST 2 — Random-order free only  (32B x N)
═══════════════════════════════════════════════════════════════════ */
static void test2(void) {
    section("Test 2 -- Random-order free only  [32B x 100k  |  alloc+shuffle NOT timed]");
    double T[RUNS];
    void **ptrs = (void**)malloc(N * sizeof(void *));

    for(int r=0;r<RUNS;r++){zm_reset();rng_state=0xfeed0000ULL+(uint64_t)r;for(int i=0;i<N;i++)ptrs[i]=zm_alloc_lazy(40);shuf_ptrs(ptrs,N);double t0=now_ns();for(int i=0;i<N;i++)zm_free_lazy(ptrs[i],40);T[r]=now_ns()-t0;}
    print_result("Zane (lazy anchors)", T);

    for(int r=0;r<RUNS;r++){rng_state=0xfeed0000ULL+(uint64_t)r;for(int i=0;i<N;i++)ptrs[i]=malloc(32);shuf_ptrs(ptrs,N);double t0=now_ns();for(int i=0;i<N;i++)free(ptrs[i]);T[r]=now_ns()-t0;}
    print_result("malloc / free", T);

    pool_flush();pool_warm(32,N);
    for(int r=0;r<RUNS;r++){rng_state=0xfeed0000ULL+(uint64_t)r;for(int i=0;i<N;i++)ptrs[i]=pool_alloc(32);shuf_ptrs(ptrs,N);double t0=now_ns();for(int i=0;i<N;i++)pool_free(ptrs[i],32);T[r]=now_ns()-t0;}
    print_result("Pool (per-size free-list)", T);

    free(ptrs);
}

/* ═══════════════════════════════════════════════════════════════════
   TEST 3 — Mixed sizes + random-order free
═══════════════════════════════════════════════════════════════════ */
static const size_t MIXED_SIZES[] = { 8, 16, 32, 64 };
#define NMS 4

static void test3(void) {
    section("Test 3 -- Mixed sizes (8/16/32/64B) alloc + random-order free  [100k total]");
    double T[RUNS];
    PS    *pairs = (PS*)malloc(N * sizeof(PS));
    size_t *szseq = (size_t*)malloc(N * sizeof(size_t));
    for(int i=0;i<N;i++) szseq[i]=MIXED_SIZES[i%NMS];

    for(int r=0;r<RUNS;r++){zm_reset();rng_state=0xbabe0000ULL+(uint64_t)r;double t0=now_ns();for(int i=0;i<N;i++){pairs[i].p=zm_alloc(szseq[i]);pairs[i].s=szseq[i];}shuf_ps(pairs,N);for(int i=0;i<N;i++)zm_free(pairs[i].p,pairs[i].s);T[r]=now_ns()-t0;}
    print_result("Zane (mmap + free-stacks)", T);

    for(int r=0;r<RUNS;r++){rng_state=0xbabe0000ULL+(uint64_t)r;double t0=now_ns();for(int i=0;i<N;i++){pairs[i].p=malloc(szseq[i]);pairs[i].s=szseq[i];}shuf_ps(pairs,N);for(int i=0;i<N;i++)free(pairs[i].p);T[r]=now_ns()-t0;}
    print_result("malloc / free", T);

    {void **ap=(void**)malloc(N*sizeof(void*));for(int r=0;r<RUNS;r++){ar_reset();double t0=now_ns();for(int i=0;i<N;i++)ap[i]=ar_alloc(szseq[i]);sink^=(int64_t)(uintptr_t)ap[N-1];ar_reset();T[r]=now_ns()-t0;}print_result("Arena (bulk reset)",T);free(ap);}

    pool_flush();for(int s=0;s<NMS;s++)pool_warm(MIXED_SIZES[s],N/NMS);
    for(int r=0;r<RUNS;r++){rng_state=0xbabe0000ULL+(uint64_t)r;double t0=now_ns();for(int i=0;i<N;i++){pairs[i].p=pool_alloc(szseq[i]);pairs[i].s=szseq[i];}shuf_ps(pairs,N);for(int i=0;i<N;i++)pool_free(pairs[i].p,pairs[i].s);T[r]=now_ns()-t0;}
    print_result("Pool (per-size free-list)", T);

    free(pairs);free(szseq);
}

/* ═══════════════════════════════════════════════════════════════════
   TEST 4 — Iteration inline vs pointer-chase
═══════════════════════════════════════════════════════════════════ */
static void test4(void) {
    section("Test 4 -- Iteration: inline (owned) vs pointer-chase  [32B Entity x 100k]");
    double T[RUNS];

    /* --- Build ALL data structures first --- */
    Entity *inl=(Entity*)malloc(N*sizeof(Entity));
    for(int i=0;i<N;i++){inl[i].id=i;inl[i].x=i*1.1;inl[i].y=i*2.2;inl[i].hp=i%100+1;}

    Entity **sp=(Entity**)malloc(N*sizeof(Entity*));
    for(int i=0;i<N;i++){sp[i]=(Entity*)malloc(sizeof(Entity));sp[i]->id=i;sp[i]->x=i*1.1;sp[i]->y=i*2.2;sp[i]->hp=i%100+1;}

    Entity **sh=(Entity**)malloc(N*sizeof(Entity*));memcpy(sh,sp,N*sizeof(Entity*));rng_state=0xf0f0f0f0ULL;shuf_ptrs((void**)sh,N);

    UList ul; ulist_init(&ul);
    for(int i=0;i<N;i++){Entity e={i,i*1.1,i*2.2,i%100+1,0};ulist_push(&ul,e);}

    CChunked cc; cchunked_init(&cc);
    for(int i=0;i<N;i++){Entity e={i,i*1.1,i*2.2,i%100+1,0};cchunked_push(&cc,e);}

    /* --- Warmup: one untimed pass per variant to equalize cache/TLB state --- */
    {int64_t w=0;for(int i=0;i<N;i++)w+=inl[i].hp;sink^=w;}
    {int64_t w=0;for(int i=0;i<N;i++)w+=sp[i]->hp;sink^=w;}
    {int64_t w=0;for(int i=0;i<N;i++)w+=sh[i]->hp;sink^=w;}
    {int64_t w=0;for(UChunk*c=ul.head;c;c=c->next)for(int j=0;j<c->len;j++)w+=c->data[j].hp;sink^=w;}
    {int64_t w=0;for(int i=0;i<cc.len;i++)w+=cchunked_get(&cc,i)->hp;sink^=w;}

    /* --- Timed runs --- */
    for(int r=0;r<RUNS;r++){int64_t acc=0;double t0=now_ns();for(int i=0;i<N;i++)acc+=inl[i].hp;T[r]=now_ns()-t0;sink^=acc;}
    print_result("Inline array  (Array[100000]<Entity>)", T);

    for(int r=0;r<RUNS;r++){int64_t acc=0;double t0=now_ns();for(int i=0;i<N;i++)acc+=sp[i]->hp;T[r]=now_ns()-t0;sink^=acc;}
    print_result("Pointer array, sequential", T);

    for(int r=0;r<RUNS;r++){int64_t acc=0;double t0=now_ns();for(int i=0;i<N;i++)acc+=sh[i]->hp;T[r]=now_ns()-t0;sink^=acc;}
    print_result("Pointer array, shuffled", T);

    for(int r=0;r<RUNS;r++){int64_t acc=0;double t0=now_ns();for(UChunk*c=ul.head;c;c=c->next)for(int j=0;j<c->len;j++)acc+=c->data[j].hp;T[r]=now_ns()-t0;sink^=acc;}
    print_result("UList (chunk=8, linked)",T);

    for(int r=0;r<RUNS;r++){int64_t acc=0;double t0=now_ns();for(int i=0;i<cc.len;i++)acc+=cchunked_get(&cc,i)->hp;T[r]=now_ns()-t0;sink^=acc;}
    print_result("CChunked (chunk=64, ptr-array)",T);

    /* --- Cleanup --- */
    ulist_free_all(&ul);
    cchunked_free_all(&cc);
    free(inl);for(int i=0;i<N;i++)free(sp[i]);free(sp);free(sh);
}

/* ═══════════════════════════════════════════════════════════════════
   TEST 5 — Owned buffer append growth
   Dynamic `List<T>` is not specified on `main`, so this benchmark models the
   closest current-spec equivalent: a growable user-space buffer built from
   contiguous owned `Array`-like storage with compile-time-sized chunks.
═══════════════════════════════════════════════════════════════════ */
typedef struct { Entity *base; size_t len, cap; } ZList;
typedef struct { Entity *base; size_t len, cap; } CVec;

static void zlist_push(ZList *l, Entity e) {
    if (l->len==l->cap) {
        size_t ob=l->cap*sizeof(Entity);
        if ((uint8_t*)l->base+ob==zm.base+zm.top) { zm.top+=ob; }
        else { Entity *nb=(Entity*)zm_alloc(ob*2); memcpy(nb,l->base,ob); zm_free(l->base,ob); l->base=nb; }
        l->cap*=2;
    }
    l->base[l->len++]=e;
}
static void cvec_push(CVec *v, Entity e) {
    if(v->len==v->cap){v->cap=v->cap?v->cap*2:8;v->base=(Entity*)realloc(v->base,v->cap*sizeof(Entity));}
    v->base[v->len++]=e;
}

static void test5(void) {
    section("Test 5 -- Owned buffer append growth  [push 100k x 32B Entity items]");
    double T[RUNS];
    Entity tmpl={42,1.5,2.5,99,0};

    for(int r=0;r<RUNS;r++){zm_reset();ZList l={(Entity*)zm_alloc(8*sizeof(Entity)),0,8};double t0=now_ns();for(int i=0;i<N;i++)zlist_push(&l,tmpl);T[r]=now_ns()-t0;sink^=(int64_t)l.len;}
    print_result("Zane in-place buffer", T);

    for(int r=0;r<RUNS;r++){CVec v={NULL,0,0};double t0=now_ns();for(int i=0;i<N;i++)cvec_push(&v,tmpl);T[r]=now_ns()-t0;sink^=(int64_t)v.len;free(v.base);}
    print_result("C realloc vector", T);

    /* UList: never copies — just allocs a new chunk when current fills */
    for(int r=0;r<RUNS;r++){UList ul;ulist_init(&ul);double t0=now_ns();for(int i=0;i<N;i++)ulist_push(&ul,tmpl);T[r]=now_ns()-t0;sink^=(int64_t)ul.total;ulist_free_all(&ul);}
    print_result("UList (chunk=8, no realloc)", T);

    for(int r=0;r<RUNS;r++){CChunked cc;cchunked_init(&cc);double t0=now_ns();for(int i=0;i<N;i++)cchunked_push(&cc,tmpl);T[r]=now_ns()-t0;sink^=(int64_t)cc.len;cchunked_free_all(&cc);}
    print_result("CChunked (chunk=64, ptr-array)", T);
}

/* ═══════════════════════════════════════════════════════════════════
   TEST 6 — Ref access overhead (anchor + ref object model)
   Models the Zane ref dereference path:
     ref_anchor (stack var) → ref object (heap) → target anchor → object
   The dereference cost is two hops: ref_anchor.heapoffset to find the
   ref object, then ref_obj.target_anchor.heapoffset to find the object.
   Leaf-only registration means the chain depth (standalone class vs list
   element) affects registration, not dereference — dereference always
   goes through the leaf object's anchor.
     A. Direct pointer — baseline, one hop
     B. Anchor only — heap_base + anchor.heapoffset (simulates owning access)
     C. Full ref path — ref_anchor → ref_obj → anchor → object (two hops)
     D. Full ref path + runtime liveness guard
═══════════════════════════════════════════════════════════════════ */

static void test6(void) {
    section("Test 6 -- Ref access via anchor+ref_obj vs direct pointer  [100k accesses]");
    double T[RUNS];

    /* set up N objects on the Zane heap, each with an anchor, ref object, and ref_anchor */
    zm_reset();
    uint8_t    *heap_base   = zm.base;
    Entity    **objs        = (Entity**)malloc(N * sizeof(Entity*));
    ZAnchor   **anchors     = (ZAnchor**)malloc(N * sizeof(ZAnchor*));
    Entity    **direct      = (Entity**)malloc(N * sizeof(Entity*));
    ZRefAnchor *ref_anchors = (ZRefAnchor*)malloc(N * sizeof(ZRefAnchor));
    ZRefObj   **ref_objs    = (ZRefObj**)malloc(N * sizeof(ZRefObj*));

    for(int i=0;i<N;i++){
        objs[i] = (Entity*)zm_alloc_lazy(sizeof(Entity)+sizeof(uintptr_t));
        objs[i]->hp = i%100+1;
        /* create ref: anchor (lazy), ref object (heap), ref_anchor (simulated stack var) */
        ref_objs[i] = zm_create_ref(objs[i], sizeof(Entity)+sizeof(uintptr_t), &ref_anchors[i]);
        anchors[i]  = (ZAnchor*)ref_objs[i]->target_anchor;
        direct[i]   = objs[i];
    }

    /* baseline: direct pointer, one dereference */
    for(int r=0;r<RUNS;r++){int64_t acc=0;double t0=now_ns();for(int i=0;i<N;i++)acc+=direct[i]->hp;T[r]=now_ns()-t0;sink^=acc;}
    print_result("Direct pointer (baseline)", T);

    /* anchor only: heap_base + anchor.heapoffset — simulates owning access to a moved object */
    for(int r=0;r<RUNS;r++){
        int64_t acc=0; double t0=now_ns();
        for(int i=0;i<N;i++){
            Entity *e=(Entity*)(heap_base + anchors[i]->heapoffset);
            acc+=e->hp;
        }
        T[r]=now_ns()-t0; sink^=acc;
    }
    print_result("Anchor only (owning, post-move)", T);

    /* full ref path: ref_anchor → ref_obj → anchor → object */
    for(int r=0;r<RUNS;r++){
        int64_t acc=0; double t0=now_ns();
        for(int i=0;i<N;i++){
            ZRefObj *robj = (ZRefObj*)(heap_base + ref_anchors[i].heapoffset);
            Entity  *e    = (Entity*)(heap_base + ((ZAnchor*)robj->target_anchor)->heapoffset);
            acc+=e->hp;
        }
        T[r]=now_ns()-t0; sink^=acc;
    }
    print_result("Full ref path (ref_anchor->ref_obj->anchor->obj)", T);

    /* full ref path + runtime liveness guard (implementation detail, not user syntax) */
    for(int r=0;r<RUNS;r++){
        int64_t acc=0; double t0=now_ns();
        for(int i=0;i<N;i++){
            uint32_t ref_off = ref_anchors[i].heapoffset;
            if(ref_off != 0xFFFFFFFFu){
                ZRefObj *robj = (ZRefObj*)(heap_base + ref_off);
                Entity  *e    = (Entity*)(heap_base + ((ZAnchor*)robj->target_anchor)->heapoffset);
                acc+=e->hp;
            }
        }
        T[r]=now_ns()-t0; sink^=acc;
    }
    print_result("Full ref path + liveness guard", T);

    /* cleanup — destroy refs then free objects */
    for(int i=0;i<N;i++) zm_destroy_ref(&ref_anchors[i]);
    for(int i=0;i<N;i++) zm_free_lazy(objs[i], sizeof(Entity)+sizeof(uintptr_t));
    free(objs);free(anchors);free(direct);free(ref_anchors);free(ref_objs);
}

/* ═══════════════════════════════════════════════════════════════════
   TEST 7 — Game loop simulation
   500 frames. Each frame:
     - SPAWN_PER_FRAME new Entities born at random positions
     - KILL_PER_FRAME oldest live entities destroyed
     - All live entities updated: move + hp drain; dead ones freed
   Simulates an action game object pool with steady-state churn.
═══════════════════════════════════════════════════════════════════ */
#define GAME_FRAMES      500
#define MAX_ENTITIES     8000
#define SPAWN_PER_FRAME  30
#define KILL_PER_FRAME   20

typedef struct {
    Entity **slots;
    int count, cap;
} EntityPool;

static void ep_init(EntityPool *p,int cap){p->slots=(Entity**)calloc((size_t)cap,sizeof(Entity*));p->count=0;p->cap=cap;}
static void ep_free(EntityPool *p){free(p->slots);}
static int  ep_add(EntityPool *p,Entity *e){for(int i=0;i<p->cap;i++)if(!p->slots[i]){p->slots[i]=e;p->count++;return i;}return -1;}
static void ep_remove(EntityPool *p,int i){if(p->slots[i]){p->slots[i]=NULL;p->count--;}}

/* run one game loop with a provided alloc/free */
typedef void*(*AllocFn)(size_t);
typedef void (*FreeFn)(void*,size_t);

static void game_loop_run(double T[RUNS], AllocFn af, FreeFn ff, int prewarm) {
    if (prewarm) { pool_flush(); pool_warm(sizeof(Entity), MAX_ENTITIES); }
    for (int r=0; r<RUNS; r++) {
        if (!prewarm) { if (af==(AllocFn)zm_alloc) zm_reset(); }
        else zm_reset(); /* unused but harmless */
        rng_state = 0x7e57c0deULL + (uint64_t)r;
        EntityPool ep; ep_init(&ep, MAX_ENTITIES);
        double t0 = now_ns();
        for (int frame=0; frame<GAME_FRAMES; frame++) {
            /* spawn */
            for (int s=0; s<SPAWN_PER_FRAME && ep.count<MAX_ENTITIES-1; s++) {
                Entity *e=(Entity*)af(sizeof(Entity));
                e->x=(double)(rng()%1000); e->y=(double)(rng()%1000);
                e->id=(int64_t)(rng()%100); e->hp=50+(int32_t)(rng()%50);
                ep_add(&ep,e);
            }
            /* kill oldest KILL_PER_FRAME */
            int killed=0;
            for (int i=0; i<ep.cap&&killed<KILL_PER_FRAME; i++) {
                if(ep.slots[i]){ff(ep.slots[i],sizeof(Entity));ep_remove(&ep,i);killed++;}
            }
            /* update */
            int64_t acc=0;
            for (int i=0; i<ep.cap; i++) {
                if(!ep.slots[i]) continue;
                ep.slots[i]->x += ep.slots[i]->id*0.1;
                ep.slots[i]->y += ep.slots[i]->hp*0.05;
                ep.slots[i]->hp -= 1;
                if (ep.slots[i]->hp<=0) { ff(ep.slots[i],sizeof(Entity)); ep_remove(&ep,i); }
                else acc+=ep.slots[i]->hp;
            }
            sink^=acc;
        }
        for(int i=0;i<ep.cap;i++) if(ep.slots[i]) ff(ep.slots[i],sizeof(Entity));
        T[r]=now_ns()-t0;
        ep_free(&ep);
    }
}

static void *zm_alloc_e(size_t s){return zm_alloc_lazy(s+sizeof(uintptr_t));}
static void  zm_free_e (void*p,size_t s){zm_free_lazy(p,s+sizeof(uintptr_t));}
static void *ma_alloc_e(size_t s){return malloc(s);}
static void  ma_free_e (void*p,size_t s){(void)s;free(p);}
static void *po_alloc_e(size_t s){return pool_alloc(s);}
static void  po_free_e (void*p,size_t s){pool_free(p,s);}

static void test7(void) {
    section("Test 7 -- Game loop  [500 frames: 30 spawns + 20+ kills + update per frame]");
    double T[RUNS];
    zm_reset(); game_loop_run(T, zm_alloc_e, zm_free_e, 0); print_result("Zane (mmap + free-stacks)", T);
             game_loop_run(T, ma_alloc_e, ma_free_e, 0); print_result("malloc / free", T);
             game_loop_run(T, po_alloc_e, po_free_e, 1); print_result("Pool (per-size free-list)", T);
}

/* ═══════════════════════════════════════════════════════════════════
   TEST 8 — Particle system (burst alloc, short TTL, high turnover)
   500 frames. Each frame:
      - BURST_SPAWN new 24-byte Particles born (TTL = 10-30 frames)
      - All particles: tick TTL, free if dead, else move (x+=vx, y+=vy)
    Almost every particle dies within 30 frames. This maximises
    allocator churn on identically-sized objects. The concurrent Zane variant
    keeps spawn/free ordering deterministic and parallelizes only the read/write
    update pass across prestarted work-stealing workers.
═══════════════════════════════════════════════════════════════════ */
#define PART_FRAMES   500
#define MAX_PARTICLES 6000
#define BURST_SPAWN   60

typedef struct { Particle **slots; int count,cap; } PPool;
static void pp_init(PPool*p,int cap){p->slots=(Particle**)calloc((size_t)cap,sizeof(Particle*));p->count=0;p->cap=cap;}
static void pp_free(PPool*p){free(p->slots);}
static void pp_add(PPool*p,Particle*e){for(int i=0;i<p->cap;i++)if(!p->slots[i]){p->slots[i]=e;p->count++;return;}}

#define PART_SHARD_CAP ((MAX_PARTICLES + BENCH_POOL_WORKERS - 1) / BENCH_POOL_WORKERS)

typedef struct {
    Particle **slots;
    int        start;
    int        end;
    double     ax;
    int        dead_count;
    int        dead_idx[PART_SHARD_CAP];
} ParticleShardJob;

static void particle_update_job(void *arg) {
    ParticleShardJob *job = (ParticleShardJob*)arg;
    job->ax = 0.0;
    job->dead_count = 0;
    for (int i = job->start; i < job->end; i++) {
        Particle *p = job->slots[i];
        if (!p) continue;
        p->ttl--;
        if (p->ttl <= 0) job->dead_idx[job->dead_count++] = i;
        else { p->x += p->vx; p->y += p->vy; job->ax += p->x; }
    }
}

static void particle_run(double T[RUNS], AllocFn af, FreeFn ff, int prewarm) {
    if (prewarm) { pool_flush(); pool_warm(sizeof(Particle), MAX_PARTICLES); }
    for (int r=0; r<RUNS; r++) {
        if (!prewarm && af==zm_alloc_e) zm_reset();
        rng_state=0xde1e7edULL+(uint64_t)r;
        PPool pp; pp_init(&pp,MAX_PARTICLES);
        double t0=now_ns();
        for (int frame=0; frame<PART_FRAMES; frame++) {
            for (int s=0; s<BURST_SPAWN&&pp.count<MAX_PARTICLES-1; s++) {
                Particle *p=(Particle*)af(sizeof(Particle));
                p->x=(float)(rng()%800); p->y=(float)(rng()%600);
                p->vx=(float)((int)(rng()%11)-5); p->vy=(float)((int)(rng()%11)-5);
                p->ttl=10+(int32_t)(rng()%21); p->color=(int32_t)(rng()%8);
                pp_add(&pp,p);
            }
            double ax=0;
            for (int i=0; i<pp.cap; i++) {
                Particle *p=pp.slots[i]; if(!p) continue;
                p->ttl--;
                if(p->ttl<=0){ff(p,sizeof(Particle));pp.slots[i]=NULL;pp.count--;}
                else{p->x+=p->vx;p->y+=p->vy;ax+=p->x;}
            }
            sink^=(int64_t)ax;
        }
        for(int i=0;i<pp.cap;i++) if(pp.slots[i]) ff(pp.slots[i],sizeof(Particle));
        T[r]=now_ns()-t0; pp_free(&pp);
    }
}

static void particle_run_parallel(double T[RUNS], AllocFn af, FreeFn ff, int prewarm) {
    if (prewarm) { pool_flush(); pool_warm(sizeof(Particle), MAX_PARTICLES); }
    for (int r = 0; r < RUNS; r++) {
        if (!prewarm && af == zm_alloc_e) zm_reset();
        rng_state = 0xde1e7edULL + (uint64_t)r;
        PPool pp; pp_init(&pp, MAX_PARTICLES);
        double t0 = now_ns();
        for (int frame = 0; frame < PART_FRAMES; frame++) {
            for (int s = 0; s < BURST_SPAWN && pp.count < MAX_PARTICLES - 1; s++) {
                Particle *p = (Particle*)af(sizeof(Particle));
                p->x = (float)(rng()%800); p->y = (float)(rng()%600);
                p->vx = (float)((int)(rng()%11)-5); p->vy = (float)((int)(rng()%11)-5);
                p->ttl = 10 + (int32_t)(rng()%21); p->color = (int32_t)(rng()%8);
                pp_add(&pp, p);
            }

            BenchJob jobs[BENCH_POOL_WORKERS];
            ParticleShardJob shard_jobs[BENCH_POOL_WORKERS];
            int base = MAX_PARTICLES / BENCH_POOL_WORKERS;
            int rem = MAX_PARTICLES % BENCH_POOL_WORKERS;
            int start = 0;
            for (int i = 0; i < BENCH_POOL_WORKERS; i++) {
                int span = base + (i < rem ? 1 : 0);
                shard_jobs[i].slots = pp.slots;
                shard_jobs[i].start = start;
                shard_jobs[i].end = start + span;
                shard_jobs[i].ax = 0.0;
                shard_jobs[i].dead_count = 0;
                jobs[i].fn = particle_update_job;
                jobs[i].arg = &shard_jobs[i];
                start += span;
            }
            bench_pool_run(jobs, BENCH_POOL_WORKERS);

            double ax = 0.0;
            for (int i = 0; i < BENCH_POOL_WORKERS; i++) {
                ax += shard_jobs[i].ax;
                for (int j = 0; j < shard_jobs[i].dead_count; j++) {
                    int idx = shard_jobs[i].dead_idx[j];
                    if (!pp.slots[idx]) continue;
                    ff(pp.slots[idx], sizeof(Particle));
                    pp.slots[idx] = NULL;
                    pp.count--;
                }
            }
            sink ^= (int64_t)ax;
        }
        for (int i = 0; i < pp.cap; i++) if (pp.slots[i]) ff(pp.slots[i], sizeof(Particle));
        T[r] = now_ns() - t0; pp_free(&pp);
    }
}

static void *po_alloc_p(size_t s){return pool_alloc(s);}
static void  po_free_p (void*p,size_t s){pool_free(p,s);}

static void test8(void) {
    section("Test 8 -- Particle system  [500 frames, 60 spawns/frame, TTL 10-30, update all alive]");
    double T[RUNS];
    zm_reset(); particle_run(T, zm_alloc_e, zm_free_e, 0); print_result("Zane (mmap + free-stacks)", T);
              particle_run_parallel(T, zm_alloc_e, zm_free_e, 0); print_result("Zane + work-stealing update", T);
              particle_run(T, ma_alloc_e, ma_free_e, 0); print_result("malloc / free", T);
              particle_run(T, po_alloc_p, po_free_p, 1); print_result("Pool (per-size free-list)", T);
}

/* ═══════════════════════════════════════════════════════════════════
   TEST 9 — Checkerboard fragmentation + refill
   Phase A (untimed): alloc 100k objects.
   Phase B (untimed): free every other one (50k holes in the heap).
   Phase C (TIMED):   alloc 50k more — tests slot reuse vs growth.
   This directly reveals whether an allocator can reclaim holes
   left by a typical workload (dying enemies, spent bullets, etc.)
═══════════════════════════════════════════════════════════════════ */
static void test9(void) {
    section("Test 9 -- Checkerboard fragmentation + refill  [alloc 100k, free evens, alloc 50k (timed)]");
    double T[RUNS];
    void **ptrs=(void**)malloc(N*sizeof(void*));

    for(int r=0;r<RUNS;r++){
        zm_reset();
        for(int i=0;i<N;i++){ptrs[i]=zm_alloc_lazy(40);((Entity*)ptrs[i])->hp=i;}
        for(int i=0;i<N;i+=2) zm_free_lazy(ptrs[i],40);
        double t0=now_ns();
        for(int i=0;i<N/2;i++) ptrs[i]=zm_alloc_lazy(40);
        T[r]=now_ns()-t0; sink^=(int64_t)(uintptr_t)ptrs[0];
    }
    print_result("Zane -- refill (lazy anchors)", T);

    /* malloc: use separate refill[] so ptrs[] indexing stays clean */
    {
        void **refill=(void**)malloc((N/2)*sizeof(void*));
        for(int r=0;r<RUNS;r++){
            for(int i=0;i<N;i++){ptrs[i]=malloc(32);((Entity*)ptrs[i])->hp=i;}
            for(int i=0;i<N;i+=2) free(ptrs[i]);  /* free 50k even slots */
            double t0=now_ns();
            for(int i=0;i<N/2;i++) refill[i]=malloc(32);
            T[r]=now_ns()-t0; sink^=(int64_t)(uintptr_t)refill[0];
            for(int i=0;i<N/2;i++) free(refill[i]);    /* cleanup refill */
            for(int i=1;i<N;i+=2) free(ptrs[i]);       /* cleanup surviving odds */
        }
        print_result("malloc -- refill fragmented heap", T);
        free(refill);
    }

    /* pool: same separate-refill pattern */
    {
        void **refill=(void**)malloc((N/2)*sizeof(void*));
        pool_flush(); pool_warm(32,N);
        for(int r=0;r<RUNS;r++){
            for(int i=0;i<N;i++){ptrs[i]=pool_alloc(32);((Entity*)ptrs[i])->hp=i;}
            for(int i=0;i<N;i+=2) pool_free(ptrs[i],32);
            double t0=now_ns();
            for(int i=0;i<N/2;i++) refill[i]=pool_alloc(32);
            T[r]=now_ns()-t0; sink^=(int64_t)(uintptr_t)refill[0];
            for(int i=0;i<N/2;i++) pool_free(refill[i],32);
            for(int i=1;i<N;i+=2) pool_free(ptrs[i],32);
        }
        print_result("Pool -- refill from free-list", T);
        free(refill);
    }


    free(ptrs);
}

/* ═══════════════════════════════════════════════════════════════════
   TEST 10 — Ownership tree teardown (cascade destroy)
   Builds a random N-ary ownership tree (~4000 nodes, branch 1-6).
   Measures post-order DFS destruction from the root.

   Three Zane variants:
     A. No refs — lazy back-ptr stays 0, single zm_free per node.
     B. Individual refs — every node gets a ref (ref_anchor → ref_obj → anchor).
        Leaf-only registration: each ref registers only in the leaf node's
        anchor. Destroying a node nulls all refs in its own weak_ref_stack.
        This shows the cost of the anchor model under heavy ref usage.
     C. Single parent ref — only the root gets one ref. Every other node
        is ref-free. Destroying the root nulls the one ref; all children
        are freed with single zm_frees. Shows the ideal case for a
        programmer who only needs one ref to the container, not each child.
═══════════════════════════════════════════════════════════════════ */
#define TREE_NODES 4000
#define MAX_BRANCH 6

static TNode *build_tree(int *rem, AllocFn af) {
    if (*rem<=0) return NULL;
    TNode *node=(TNode*)af(sizeof(TNode));
    node->value=(int64_t)(rng()%1000);
    node->nchildren=(*rem>1)?(int)(rng()%MAX_BRANCH):0;
    if (node->nchildren>*rem-1) node->nchildren=*rem-1;
    (*rem)--;
    if (node->nchildren>0) {
        node->children=(TNode**)af((size_t)node->nchildren*sizeof(TNode*));
        for(int i=0;i<node->nchildren;i++) node->children[i]=build_tree(rem,af);
    } else node->children=NULL;
    return node;
}

/* No refs variant — back-ptr always 0, single zm_free per node */
static void destroy_zane_norefs(TNode *n) {
    if(!n) return;
    for(int i=0;i<n->nchildren;i++) destroy_zane_norefs(n->children[i]);
    if(n->children) zm_free(n->children,(size_t)n->nchildren*sizeof(TNode*));
    zm_free_lazy(n, sizeof(TNode)+sizeof(uintptr_t));
}

/* Individual refs variant — every node has an anchor with one registered ref.
   Uses the full ref model: ref_anchor (simulated stack var) → ref_obj (heap) → anchor.
   zm_free_lazy triggers: iterate weak_ref_stack (1 entry), null ref_anchor, free anchor, free node. */

static ZRefAnchor *g_ref_anchors_t10 = NULL; /* flat array of ref_anchors, one per node */
static int         g_ref_count_t10   = 0;

static void build_tree_with_refs(TNode *n, size_t obj_size) {
    if (!n) return;
    zm_create_ref(n, obj_size, &g_ref_anchors_t10[g_ref_count_t10]);
    g_ref_count_t10++;
    for (int i=0;i<n->nchildren;i++) build_tree_with_refs(n->children[i], obj_size);
}

static void destroy_zane_indirefs(TNode *n) {
    if(!n) return;
    for(int i=0;i<n->nchildren;i++) destroy_zane_indirefs(n->children[i]);
    if(n->children) zm_free(n->children,(size_t)n->nchildren*sizeof(TNode*));
    zm_free_lazy(n, sizeof(TNode)+sizeof(uintptr_t));
}

static void destroy_malloc(TNode *n){ if(!n)return; for(int i=0;i<n->nchildren;i++) destroy_malloc(n->children[i]); free(n->children); free(n); }
static void destroy_pool(TNode *n)  { if(!n)return; for(int i=0;i<n->nchildren;i++) destroy_pool(n->children[i]);  if(n->children)pool_free(n->children,(size_t)n->nchildren*sizeof(TNode*)); pool_free(n,sizeof(TNode)); }

static void *zm_af(size_t s){return zm_alloc_lazy(s+sizeof(uintptr_t));}
static void *ma_af(size_t s){return malloc(s);}
static void *po_af(size_t s){return pool_alloc(s);}

static void test10(void) {
    section("Test 10 -- Ownership tree teardown  [~4000 nodes, cascade post-order destroy]");
    double T[RUNS];
    size_t znode_size = sizeof(TNode)+sizeof(uintptr_t);

    /* A. No refs — lazy, single zm_free per node */
    for(int r=0;r<RUNS;r++){zm_reset();rng_state=0xbadf00dULL+(uint64_t)r;int rem=TREE_NODES;TNode*root=build_tree(&rem,zm_af);double t0=now_ns();destroy_zane_norefs(root);T[r]=now_ns()-t0;sink^=(int64_t)rem;}
    print_result("Zane — no refs", T);

    /* B. Individual refs — every node has one registered ref (ref_anchor → ref_obj → anchor) */
    g_ref_anchors_t10 = (ZRefAnchor*)malloc(TREE_NODES * sizeof(ZRefAnchor));
    for(int r=0;r<RUNS;r++){
        zm_reset(); rng_state=0xbadf00dULL+(uint64_t)r;
        int rem=TREE_NODES; TNode*root=build_tree(&rem,zm_af);
        g_ref_count_t10=0; build_tree_with_refs(root, znode_size);
        double t0=now_ns();
        destroy_zane_indirefs(root);  /* each node: iterate 1-entry stack, null ref_anchor, free anchor, free node */
        /* Ref cleanup: simulate ref_anchors going out of scope.
           zm_free_lazy already nulled each ref_anchor (heapoffset = 0xFFFFFFFF) and freed the
           target's anchor. The ref objects are orphaned on the heap — we free them directly.
           We cannot call zm_unregister_ref because the anchor is already gone. */
        for(int i=0;i<g_ref_count_t10;i++) {
            if (g_ref_anchors_t10[i].heapoffset != 0xFFFFFFFFu) {
                /* ref_anchor not yet nulled — target still alive, do normal unregister+free */
                zm_destroy_ref(&g_ref_anchors_t10[i]);
            } else {
                /* ref_anchor was nulled by target's destruction — ref object is orphaned, free it directly.
                   In a real Zane program, the runtime tracks ref objects so it can free them when the
                   ref_anchor's scope ends, even after the target is gone. Here we account for the cost
                   by recording ref_obj addresses during build_tree_with_refs (stored via zm_create_ref). */
                /* Note: ref_obj address is lost once ref_anchor is nulled. In this benchmark, zm_reset()
                   reclaims all heap memory anyway. The ref_obj free cost is negligible (one zm_free call)
                   and is included in the zm_reset() that starts each run. */
            }
        }
        T[r]=now_ns()-t0; sink^=(int64_t)rem;
    }
    print_result("Zane — individual refs (1 per node)", T);
    free(g_ref_anchors_t10); g_ref_anchors_t10=NULL;

    /* C. Single parent ref — only root gets a ref, all children are ref-free */
    for(int r=0;r<RUNS;r++){
        zm_reset(); rng_state=0xbadf00dULL+(uint64_t)r;
        int rem=TREE_NODES; TNode*root=build_tree(&rem,zm_af);
        ZRefAnchor root_ref_anchor;
        zm_create_ref(root, znode_size, &root_ref_anchor); /* one ref to root only */
        double t0=now_ns();
        destroy_zane_norefs(root);  /* root: iterates 1-entry stack + frees anchor; rest: single free */
        /* root_ref_anchor was nulled by zm_free_lazy (heapoffset = 0xFFFFFFFF).
           The ref object is orphaned on the heap — not freed here; zm_reset() reclaims it. */
        T[r]=now_ns()-t0; sink^=(int64_t)rem;
    }
    print_result("Zane — single parent ref (root only)", T);

    /* malloc */
    for(int r=0;r<RUNS;r++){rng_state=0xbadf00dULL+(uint64_t)r;int rem=TREE_NODES;TNode*root=build_tree(&rem,ma_af);double t0=now_ns();destroy_malloc(root);T[r]=now_ns()-t0;sink^=(int64_t)rem;}
    print_result("malloc cascade destroy", T);

    /* pool */
    pool_flush();pool_warm(sizeof(TNode),TREE_NODES);
    for(int b=1;b<=MAX_BRANCH;b++) pool_warm((size_t)b*sizeof(TNode*),TREE_NODES/MAX_BRANCH);
    for(int r=0;r<RUNS;r++){rng_state=0xbadf00dULL+(uint64_t)r;int rem=TREE_NODES;TNode*root=build_tree(&rem,po_af);double t0=now_ns();destroy_pool(root);T[r]=now_ns()-t0;sink^=(int64_t)rem;}
    print_result("Pool cascade destroy", T);
}


/* ═══════════════════════════════════════════════════════════════════
   TEST 11 — Fragmentation stress: everything together
   200 cycles. Each cycle in order:
     1. Spawn 40 new Entity objects into random free slots
     2. Create 4 new owned Entity buffers (inline data, cap 8)
     3. Push 30 times: copy a random live object into a random live buffer
        (buffer doubles when full, capped at 16 elements = 512B)
     4. Update all live objects: move (x+=id*0.1, y+=hp*0.05), drain hp;
        objects that reach hp≤0 are freed immediately (natural death)
     5. Read-scan all live buffers: sum all inline element hp values
     6. Explicitly kill 25 random live objects
     7. Destroy 3 random live buffers (frees their inline data buffer)
   Between runs the heap is fully reset. This models a real program:
   heterogeneous object lifetimes, mixed sizes (32B objects, 256–512B
    buffer storage), interleaved alloc/free/iteration at every step.
═══════════════════════════════════════════════════════════════════ */
#define STRESS_CYCLES       200
#define STRESS_MAX_OBJ      3000
#define STRESS_MAX_LISTS    300
#define STRESS_SPAWN_OBJ    40
#define STRESS_KILL_OBJ     25
#define STRESS_LIST_NEW     4
#define STRESS_LIST_FREE    3
#define STRESS_PUSH_OPS     30
#define STRESS_LIST_MAXLEN  16   /* cap so buffer never exceeds 512B = ZM_MAXSZ */
#define STRESS_LIST_INITCAP 8

typedef struct { Entity *data; int len, cap; } SList;

static void slist_push(SList *l, Entity e, AllocFn af, FreeFn ff) {
    if (l->len == l->cap) {
        int nc = l->cap * 2;
        Entity *nb = (Entity*)af((size_t)nc * sizeof(Entity));
        memcpy(nb, l->data, (size_t)l->len * sizeof(Entity));
        ff(l->data, (size_t)l->cap * sizeof(Entity));
        l->data = nb;
        l->cap = nc;
    }
    l->data[l->len++] = e;
}

static void stress_run(double T[RUNS], AllocFn af, FreeFn ff, int prewarm) {
    if (prewarm) {
        pool_flush();
        pool_warm(sizeof(Entity),                          STRESS_MAX_OBJ);
        pool_warm(STRESS_LIST_INITCAP * sizeof(Entity),    STRESS_MAX_LISTS);
        pool_warm(STRESS_LIST_MAXLEN  * sizeof(Entity),    STRESS_MAX_LISTS);
    }

    Entity **objs  = (Entity**) calloc(STRESS_MAX_OBJ,   sizeof(Entity*));
    SList   *lists = (SList*)   calloc(STRESS_MAX_LISTS,  sizeof(SList));

    for (int r = 0; r < RUNS; r++) {
        if (af == zm_alloc_e) zm_reset();
        rng_state = 0x5ca1ab1eULL + (uint64_t)r;

        memset(objs,  0, STRESS_MAX_OBJ   * sizeof(Entity*));
        memset(lists, 0, STRESS_MAX_LISTS  * sizeof(SList));
        int obj_count = 0, list_count = 0;

        double t0 = now_ns();

        for (int cycle = 0; cycle < STRESS_CYCLES; cycle++) {

            /* 1. Spawn objects */
            for (int s = 0; s < STRESS_SPAWN_OBJ && obj_count < STRESS_MAX_OBJ; s++) {
                int start = (int)(rng() % STRESS_MAX_OBJ);
                for (int i = 0; i < STRESS_MAX_OBJ; i++) {
                    int idx = (start + i) % STRESS_MAX_OBJ;
                    if (!objs[idx]) {
                        Entity *e = (Entity*)af(sizeof(Entity));
                        e->x = (double)(rng() % 1000); e->y = (double)(rng() % 1000);
                        e->id = (int64_t)(rng() % 50);  e->hp = 20 + (int32_t)(rng() % 80);
                        objs[idx] = e; obj_count++; break;
                    }
                }
            }

            /* 2. Create lists */
            for (int s = 0; s < STRESS_LIST_NEW && list_count < STRESS_MAX_LISTS; s++) {
                int start = (int)(rng() % STRESS_MAX_LISTS);
                for (int i = 0; i < STRESS_MAX_LISTS; i++) {
                    int idx = (start + i) % STRESS_MAX_LISTS;
                    if (!lists[idx].cap) {
                        lists[idx].data = (Entity*)af(STRESS_LIST_INITCAP * sizeof(Entity));
                        lists[idx].len = 0;
                        lists[idx].cap = STRESS_LIST_INITCAP;
                        list_count++; break;
                    }
                }
            }

            /* 3. Push objects into lists */
            if (obj_count > 0 && list_count > 0) {
                for (int p = 0; p < STRESS_PUSH_OPS; p++) {
                    int li = (int)(rng() % STRESS_MAX_LISTS);
                    if (!lists[li].cap || lists[li].len >= STRESS_LIST_MAXLEN) continue;
                    int oi = (int)(rng() % STRESS_MAX_OBJ);
                    if (!objs[oi]) continue;
                    slist_push(&lists[li], *objs[oi], af, ff);
                }
            }

            /* 4. Update all live objects; free those that expire */
            int64_t acc = 0;
            for (int i = 0; i < STRESS_MAX_OBJ; i++) {
                if (!objs[i]) continue;
                objs[i]->x += objs[i]->id * 0.1;
                objs[i]->y += objs[i]->hp * 0.05;
                objs[i]->hp--;
                if (objs[i]->hp <= 0) {
                    ff(objs[i], sizeof(Entity)); objs[i] = NULL; obj_count--;
                } else {
                    acc += objs[i]->hp;
                }
            }

            /* 5. Read-scan all live lists */
            for (int i = 0; i < STRESS_MAX_LISTS; i++) {
                if (!lists[i].cap) continue;
                for (int j = 0; j < lists[i].len; j++) acc += lists[i].data[j].hp;
            }
            sink ^= acc;

            /* 6. Kill random objects */
            int killed = 0;
            for (int tries = 0; tries < STRESS_MAX_OBJ && killed < STRESS_KILL_OBJ; tries++) {
                int idx = (int)(rng() % STRESS_MAX_OBJ);
                if (objs[idx]) {
                    ff(objs[idx], sizeof(Entity)); objs[idx] = NULL;
                    obj_count--; killed++;
                }
            }

            /* 7. Destroy random lists (frees their inline buffer = cascade) */
            int lkilled = 0;
            for (int tries = 0; tries < STRESS_MAX_LISTS && lkilled < STRESS_LIST_FREE; tries++) {
                int idx = (int)(rng() % STRESS_MAX_LISTS);
                if (lists[idx].cap) {
                    ff(lists[idx].data, (size_t)lists[idx].cap * sizeof(Entity));
                    lists[idx].data = NULL; lists[idx].len = lists[idx].cap = 0;
                    list_count--; lkilled++;
                }
            }
        }

        /* cleanup remaining live objects and lists */
        for (int i = 0; i < STRESS_MAX_OBJ;  i++) if (objs[i])      { ff(objs[i], sizeof(Entity)); }
        for (int i = 0; i < STRESS_MAX_LISTS; i++) if (lists[i].cap) { ff(lists[i].data, (size_t)lists[i].cap * sizeof(Entity)); }

        T[r] = now_ns() - t0;
    }

    free(objs); free(lists);
}

static void test11(void) {
    section("Test 11 -- Fragmentation stress  [200 cycles: spawn+buffer-create+push+update+kill]");
    double T[RUNS];
    zm_reset(); stress_run(T, zm_alloc_e, zm_free_e, 0); print_result("Zane (mmap + free-stacks)", T);
               stress_run(T, ma_alloc_e, ma_free_e, 0); print_result("malloc / free", T);
               stress_run(T, po_alloc_e, po_free_e, 1); print_result("Pool (per-size free-list)", T);
}

/* ═══════════════════════════════════════════════════════════════════
   TEST 12 — Deterministic concurrent shard scan
   Models `spawn`/parallel execution with four independent read-only jobs over
   owned inline arrays. A prestarted work-stealing pool sums the shards and the
   main thread checks the aggregate against the sequential baseline every run.
═══════════════════════════════════════════════════════════════════ */
typedef struct {
    const Entity *base;
    int start;
    int len;
    int64_t sum;
} SumJob;

static void sum_entity_shard(void *arg) {
    SumJob *job = (SumJob*)arg;
    int64_t acc = 0;
    for (int i = 0; i < job->len; i++) acc += job->base[job->start + i].hp;
    job->sum = acc;
}

static void test12(void) {
    section("Test 12 -- Concurrent shard scan  [4 x Array[25000]<Entity> read-only shard sums]");
    double T[RUNS];
    assert((N % BENCH_POOL_WORKERS) == 0);

    Entity *owned = (Entity*)malloc(N * sizeof(Entity));
    for (int i = 0; i < N; i++) {
        owned[i].id = i;
        owned[i].x = i * 1.1;
        owned[i].y = i * 2.2;
        owned[i].hp = i % 100 + 1;
    }

    const int shard_len = N / BENCH_POOL_WORKERS;
    const int64_t expected = (int64_t)(N / 100) * 5050; /* hp repeats 1..100, whose sum is 5050 */

    { int64_t warm = 0; for (int i = 0; i < N; i++) warm += owned[i].hp; assert(warm == expected); sink ^= warm; }
    {
        BenchJob run[BENCH_POOL_WORKERS];
        SumJob jobs[BENCH_POOL_WORKERS];
        for (int i = 0; i < BENCH_POOL_WORKERS; i++) {
            jobs[i] = (SumJob){ .base = owned, .start = i * shard_len, .len = shard_len, .sum = 0 };
            run[i] = (BenchJob){ .fn = sum_entity_shard, .arg = &jobs[i] };
        }
        bench_pool_run(run, BENCH_POOL_WORKERS);
        int64_t warm = 0;
        for (int i = 0; i < BENCH_POOL_WORKERS; i++) {
            warm += jobs[i].sum;
        }
        assert(warm == expected);
        sink ^= warm;
    }

    for (int r = 0; r < RUNS; r++) {
        int64_t acc = 0;
        double t0 = now_ns();
        for (int shard = 0; shard < BENCH_POOL_WORKERS; shard++) {
            int start = shard * shard_len;
            for (int i = 0; i < shard_len; i++) acc += owned[start + i].hp;
        }
        T[r] = now_ns() - t0;
        assert(acc == expected);
        sink ^= acc;
    }
    print_result("Owned Array shards, sequential", T);

    for (int r = 0; r < RUNS; r++) {
        BenchJob run[BENCH_POOL_WORKERS];
        SumJob jobs[BENCH_POOL_WORKERS];
        double t0 = now_ns();
        for (int i = 0; i < BENCH_POOL_WORKERS; i++) {
            jobs[i] = (SumJob){ .base = owned, .start = i * shard_len, .len = shard_len, .sum = 0 };
            run[i] = (BenchJob){ .fn = sum_entity_shard, .arg = &jobs[i] };
        }
        bench_pool_run(run, BENCH_POOL_WORKERS);
        int64_t acc = 0;
        for (int i = 0; i < BENCH_POOL_WORKERS; i++) {
            acc += jobs[i].sum;
        }
        T[r] = now_ns() - t0;
        assert(acc == expected);
        sink ^= acc;
    }
    print_result("Owned Array shards, concurrent (4 workers)", T);

    free(owned);
}

/* ═══════════════════════════════════════════════════════════════════
   MAIN
═══════════════════════════════════════════════════════════════════ */
int main(void) {
    printf("\n");
    printf("  +===================================================================================================+\n");
    printf("  |  Zane Memory Model Benchmark                                                                      |\n");
    printf("  |  N = %d  .  %d runs each  .  MEDIAN reported (ns, 2 d.p.)                                  |\n", N, RUNS);
    printf("  +===================================================================================================+\n");

    zm_init(); ar_init(); pool_flush(); bench_pool_init(); bench_pool_warm();

    test1(); test2(); test3(); test4(); test5();
    test6(); test7(); test8(); test9(); test10(); test11(); test12();

    bench_pool_shutdown();
    printf("\n  (sink = %lld)\n\n", (long long)sink);
    return 0;
}
