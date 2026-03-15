/*
 * ═══════════════════════════════════════════════════════════════════
 *  Zane Memory Model Benchmark
 *  Compares Zane's memory system against standard C allocators.
 *  Each test runs 20 times; MEDIAN reported (outlier-resistant).
 *
 *  Allocator implementations:
 *    Zane   — single mmap region, size-indexed free-stack table, O(1)
 *             + lazy anchor model: anchor allocated only on first weak ref
 *               to an object; free is O(1) if no weak refs were ever made
 *    malloc — system allocator (glibc), coalescing on free
 *    Arena  — bump allocator, bulk O(1) reset (no per-item free)
 *    Pool   — per-size segregated free-list, malloc-backed first use
 *
 *  Containers under test:
 *    UList    — linked list of 8-element inline chunks
 *    CChunked — array of 64-element chunk pointers
 *
 *  Tests:
 *    1. Sequential alloc + sequential free          (32B × 100k)
 *    2. Random-order free only                      (32B × 100k)
 *    3. Mixed sizes alloc + random-order free       (8/16/32/64B × 100k)
 *    4. Iteration — inline vs pointer-chase         [+UList +CChunked]
 *    5. List append growth                          (32B × 100k)  [+UList +CChunked]
 *    6. Weak-ref access overhead (anchor model)
 *    7. Game loop — entity spawn/kill/update        (500 frames)
 *    8. Particle system — short-lifetime objects    (500 frames)
 *    9. Checkerboard fragmentation + refill
 *   10. Ownership tree teardown                     (cascade destroy)
 *   11. Fragmentation stress                        (200 cycles)
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
   ZANE ANCHOR MODEL  (lazy creation)
   Anchors are created on demand — only when the first weak reference
   to an object is made. The back-pointer slot in every class instance
   is initialised to 0 (no anchor yet). This means objects that never
   receive a weak ref pay zero anchor overhead: alloc and free are
   single zm_alloc / zm_free calls, identical to a plain allocator.

   back-pointer slot (8 bytes, after declared fields):
     0            — no anchor exists yet
     ptr != 0     — absolute address of the object's ZAnchor

   anchor layout (24 bytes):
     offset:      u32   — heap-relative offset of the object
     nrefs:       u32   — number of registered weak refs
     refs:        u64*  — absolute addresses of registered weak ref anchors
     refs_cap:    u32
     _pad:        u32

   All references TO anchors are absolute addresses. The offset
   stored INSIDE the anchor is heap-relative (object can move).
═══════════════════════════════════════════════════════════════════ */
#define ZANCHOR_SIZE 24

typedef struct {
    uint32_t  offset;
    uint32_t  nrefs;
    uint64_t *refs;
    uint32_t  refs_cap;
    uint32_t  _pad;
} ZAnchor;
_Static_assert(sizeof(ZAnchor) == ZANCHOR_SIZE, "ZAnchor must be 24 bytes");

/* alloc object only — back-pointer initialised to 0 (no anchor yet).
   Cost: one zm_alloc. */
static void *zm_alloc_lazy(size_t obj_size) {
    void *obj = zm_alloc(obj_size);
    *(uintptr_t*)((uint8_t*)obj + obj_size - sizeof(uintptr_t)) = 0;
    return obj;
}

/* get-or-create anchor for an object. Called only when a weak ref is first made.
   If the back-pointer is already set, returns the existing anchor. */
static ZAnchor *zm_get_anchor(void *obj, size_t obj_size) {
    uintptr_t *bptr = (uintptr_t*)((uint8_t*)obj + obj_size - sizeof(uintptr_t));
    if (*bptr) return (ZAnchor*)*bptr;
    ZAnchor *anchor  = (ZAnchor*)zm_alloc(ZANCHOR_SIZE);
    anchor->offset   = (uint32_t)((uint8_t*)obj - zm.base);
    anchor->nrefs    = 0;
    anchor->refs     = NULL;
    anchor->refs_cap = 0;
    *bptr = (uintptr_t)anchor;
    return anchor;
}

/* free object:
   - if back-pointer == 0: no anchor was ever created — single free, done.
   - if back-pointer != 0: iterate weak_ref_stack, free anchor, free object. */
static void zm_free_lazy(void *obj, size_t obj_size) {
    uintptr_t bptr = *(uintptr_t*)((uint8_t*)obj + obj_size - sizeof(uintptr_t));
    if (bptr) {
        ZAnchor *anchor = (ZAnchor*)bptr;
        for (uint32_t i = 0; i < anchor->nrefs; i++) {
            ZAnchor *wref_anchor = (ZAnchor*)anchor->refs[i];
            wref_anchor->offset  = 0xFFFFFFFFu; /* null sentinel */
        }
        if (anchor->refs) free(anchor->refs);
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
    section("Test 4 -- Iteration: inline (store T) vs pointer-chase  [32B Entity x 100k]");
    double T[RUNS];

    Entity *inl=(Entity*)malloc(N*sizeof(Entity));
    for(int i=0;i<N;i++){inl[i].id=i;inl[i].x=i*1.1;inl[i].y=i*2.2;inl[i].hp=i%100+1;}
    for(int r=0;r<RUNS;r++){int64_t acc=0;double t0=now_ns();for(int i=0;i<N;i++)acc+=inl[i].hp;T[r]=now_ns()-t0;sink^=acc;}
    print_result("Inline array  (store T)", T);

    Entity **sp=(Entity**)malloc(N*sizeof(Entity*));
    for(int i=0;i<N;i++){sp[i]=(Entity*)malloc(sizeof(Entity));sp[i]->id=i;sp[i]->x=i*1.1;sp[i]->y=i*2.2;sp[i]->hp=i%100+1;}
    for(int r=0;r<RUNS;r++){int64_t acc=0;double t0=now_ns();for(int i=0;i<N;i++)acc+=sp[i]->hp;T[r]=now_ns()-t0;sink^=acc;}
    print_result("Pointer array, sequential", T);

    Entity **sh=(Entity**)malloc(N*sizeof(Entity*));memcpy(sh,sp,N*sizeof(Entity*));rng_state=0xf0f0f0f0ULL;shuf_ptrs((void**)sh,N);
    for(int r=0;r<RUNS;r++){int64_t acc=0;double t0=now_ns();for(int i=0;i<N;i++)acc+=sh[i]->hp;T[r]=now_ns()-t0;sink^=acc;}
    print_result("Pointer array, shuffled", T);

    /* UList: chunks of 8 elements inline, linked between chunks */
    { UList ul; ulist_init(&ul);
      for(int i=0;i<N;i++){Entity e={i,i*1.1,i*2.2,i%100+1,0};ulist_push(&ul,e);}
      for(int r=0;r<RUNS;r++){int64_t acc=0;double t0=now_ns();for(UChunk*c=ul.head;c;c=c->next)for(int j=0;j<c->len;j++)acc+=c->data[j].hp;T[r]=now_ns()-t0;sink^=acc;}
      print_result("UList (chunk=8, linked)",T); ulist_free_all(&ul); }

    /* CChunked: chunks[i/64][i%64] — two indirections, never copies */
    { CChunked cc; cchunked_init(&cc);
      for(int i=0;i<N;i++){Entity e={i,i*1.1,i*2.2,i%100+1,0};cchunked_push(&cc,e);}
      for(int r=0;r<RUNS;r++){int64_t acc=0;double t0=now_ns();for(int i=0;i<cc.len;i++)acc+=cchunked_get(&cc,i)->hp;T[r]=now_ns()-t0;sink^=acc;}
      print_result("CChunked (chunk=64, ptr-array)",T); cchunked_free_all(&cc); }

    free(inl);for(int i=0;i<N;i++)free(sp[i]);free(sp);free(sh);
}

/* ═══════════════════════════════════════════════════════════════════
   TEST 5 — List append growth
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
    section("Test 5 -- List append growth  [push 100k x 32B Entity items]");
    double T[RUNS];
    Entity tmpl={42,1.5,2.5,99,0};

    for(int r=0;r<RUNS;r++){zm_reset();ZList l={(Entity*)zm_alloc(8*sizeof(Entity)),0,8};double t0=now_ns();for(int i=0;i<N;i++)zlist_push(&l,tmpl);T[r]=now_ns()-t0;sink^=(int64_t)l.len;}
    print_result("Zane in-place list", T);

    for(int r=0;r<RUNS;r++){CVec v={NULL,0,0};double t0=now_ns();for(int i=0;i<N;i++)cvec_push(&v,tmpl);T[r]=now_ns()-t0;sink^=(int64_t)v.len;free(v.base);}
    print_result("C realloc vector", T);

    /* UList: never copies — just allocs a new chunk when current fills */
    for(int r=0;r<RUNS;r++){UList ul;ulist_init(&ul);double t0=now_ns();for(int i=0;i<N;i++)ulist_push(&ul,tmpl);T[r]=now_ns()-t0;sink^=(int64_t)ul.total;ulist_free_all(&ul);}
    print_result("UList (chunk=8, no realloc)", T);

    for(int r=0;r<RUNS;r++){CChunked cc;cchunked_init(&cc);double t0=now_ns();for(int i=0;i<N;i++)cchunked_push(&cc,tmpl);T[r]=now_ns()-t0;sink^=(int64_t)cc.len;cchunked_free_all(&cc);}
    print_result("CChunked (chunk=64, ptr-array)", T);
}

/* ═══════════════════════════════════════════════════════════════════
   TEST 6 — Weak-ref access overhead (anchor model)
   Models the Zane Stack<*anchor> dereference for three cases:
     A. Standalone class:  weak_ref.anchors[0] → heap_base + anchor.offset
     B. List element:      anchors[0] (list) + anchors[1] (element offset)
     C. Both + null check  (realistic Zane usage)
   All anchor pointers are absolute addresses — no base arithmetic needed
   to locate them. Only the final object address requires heap_base.
═══════════════════════════════════════════════════════════════════ */

/* Anchor as used in T6: reuses ZAnchor, offset = heap-relative object offset */
typedef struct {
    ZAnchor *anchors[2]; /* Stack<*anchor> — length 1 or 2, statically known */
    int       depth;     /* 1 = standalone class, 2 = list element            */
} ZWeakRef;

static void test6(void) {
    section("Test 6 -- Anchor (weak ref) vs direct pointer  [100k accesses]");
    double T[RUNS];

    /* set up N objects on the Zane heap, each with its own anchor */
    zm_reset();
    uint8_t  *heap_base = zm.base;
    Entity  **objs      = (Entity**)malloc(N * sizeof(Entity*));
    ZAnchor **anchors   = (ZAnchor**)malloc(N * sizeof(ZAnchor*));
    Entity  **direct    = (Entity**)malloc(N * sizeof(Entity*));
    ZWeakRef *wrefs1    = (ZWeakRef*)malloc(N * sizeof(ZWeakRef)); /* depth=1 */
    ZWeakRef *wrefs2    = (ZWeakRef*)malloc(N * sizeof(ZWeakRef)); /* depth=2 */

    /* simulate List<store Entity>: one list anchor + per-element anchor */
    ZAnchor *list_anchor = (ZAnchor*)zm_alloc(ZANCHOR_SIZE);
    list_anchor->offset  = 0; /* list base = heap_base+0 for simulation */
    list_anchor->nrefs   = 0; list_anchor->refs = NULL; list_anchor->refs_cap = 0;

    for(int i=0;i<N;i++){
        objs[i] = (Entity*)zm_alloc_lazy(sizeof(Entity)+sizeof(uintptr_t)); /* back-ptr slot init to 0 */
        objs[i]->hp = i%100+1;
        /* create anchor now — this is the first weak ref moment */
        anchors[i] = zm_get_anchor(objs[i], sizeof(Entity)+sizeof(uintptr_t));
        direct[i] = objs[i];
        /* depth=1: standalone class weak ref */
        wrefs1[i].anchors[0] = anchors[i]; wrefs1[i].depth = 1;
        /* depth=2: list element weak ref — list_anchor + element_anchor */
        /* element anchor offset = byte distance from list base to element */
        wrefs2[i].anchors[0] = list_anchor;
        wrefs2[i].anchors[1] = anchors[i];
        wrefs2[i].depth = 2;
        /* set element anchor offset to be relative to list base (simulated) */
        anchors[i]->offset = (uint32_t)((uint8_t*)objs[i] - heap_base); /* absolute for standalone */
    }

    /* baseline: direct pointer, one dereference */
    for(int r=0;r<RUNS;r++){int64_t acc=0;double t0=now_ns();for(int i=0;i<N;i++)acc+=direct[i]->hp;T[r]=now_ns()-t0;sink^=acc;}
    print_result("Direct pointer (baseline)", T);

    /* depth=1: standalone class — heap_base + anchor.offset */
    for(int r=0;r<RUNS;r++){
        int64_t acc=0; double t0=now_ns();
        for(int i=0;i<N;i++){
            Entity *e=(Entity*)(heap_base + wrefs1[i].anchors[0]->offset);
            acc+=e->hp;
        }
        T[r]=now_ns()-t0; sink^=acc;
    }
    print_result("Anchor depth=1 (standalone class)", T);

    /* depth=2: list element — heap_base + list_anchor.offset + element_anchor.offset */
    for(int r=0;r<RUNS;r++){
        int64_t acc=0; double t0=now_ns();
        for(int i=0;i<N;i++){
            uint8_t *base=(heap_base + wrefs2[i].anchors[0]->offset);
            Entity  *e   =(Entity*)(base + wrefs2[i].anchors[1]->offset);
            acc+=e->hp;
        }
        T[r]=now_ns()-t0; sink^=acc;
    }
    print_result("Anchor depth=2 (list element)", T);

    /* depth=1 + null check (realistic Zane usage) */
    for(int r=0;r<RUNS;r++){
        int64_t acc=0; double t0=now_ns();
        for(int i=0;i<N;i++){
            uint32_t off=wrefs1[i].anchors[0]->offset;
            if(off!=0xFFFFFFFFu){ Entity *e=(Entity*)(heap_base+off); acc+=e->hp; }
        }
        T[r]=now_ns()-t0; sink^=acc;
    }
    print_result("Anchor depth=1 + null check", T);

    /* cleanup — objects were allocated as sizeof(Entity)+sizeof(uintptr_t), anchors via zm_free_lazy */
    for(int i=0;i<N;i++) zm_free_lazy(objs[i], sizeof(Entity)+sizeof(uintptr_t));
    zm_free(list_anchor,ZANCHOR_SIZE);
    free(objs);free(anchors);free(direct);free(wrefs1);free(wrefs2);
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
   allocator churn on identically-sized objects.
═══════════════════════════════════════════════════════════════════ */
#define PART_FRAMES   500
#define MAX_PARTICLES 6000
#define BURST_SPAWN   60

typedef struct { Particle **slots; int count,cap; } PPool;
static void pp_init(PPool*p,int cap){p->slots=(Particle**)calloc((size_t)cap,sizeof(Particle*));p->count=0;p->cap=cap;}
static void pp_free(PPool*p){free(p->slots);}
static void pp_add(PPool*p,Particle*e){for(int i=0;i<p->cap;i++)if(!p->slots[i]){p->slots[i]=e;p->count++;return;}}

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

static void *po_alloc_p(size_t s){return pool_alloc(s);}
static void  po_free_p (void*p,size_t s){pool_free(p,s);}

static void test8(void) {
    section("Test 8 -- Particle system  [500 frames, 60 spawns/frame, TTL 10-30, update all alive]");
    double T[RUNS];
    zm_reset(); particle_run(T, zm_alloc_e, zm_free_e, 0); print_result("Zane (mmap + free-stacks)", T);
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
   Measures post-order DFS destruction from the root — identical to
   what Zane does automatically when a strong ref dies: children are
   destroyed first, then the parent, recursively.
   Shows the raw cost of the cascade teardown pattern.
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

static void destroy_zane(TNode *n)  { if(!n)return; for(int i=0;i<n->nchildren;i++) destroy_zane(n->children[i]);  if(n->children)zm_free(n->children,(size_t)n->nchildren*sizeof(TNode*));  zm_free_lazy(n,sizeof(TNode)+sizeof(uintptr_t)); }
static void destroy_malloc(TNode *n){ if(!n)return; for(int i=0;i<n->nchildren;i++) destroy_malloc(n->children[i]); free(n->children); free(n); }
static void destroy_pool(TNode *n)  { if(!n)return; for(int i=0;i<n->nchildren;i++) destroy_pool(n->children[i]);  if(n->children)pool_free(n->children,(size_t)n->nchildren*sizeof(TNode*)); pool_free(n,sizeof(TNode)); }

static void *zm_af(size_t s){return zm_alloc_lazy(s+sizeof(uintptr_t));}
static void *ma_af(size_t s){return malloc(s);}
static void *po_af(size_t s){return pool_alloc(s);}

static void test10(void) {
    section("Test 10 -- Ownership tree teardown  [~4000 nodes, cascade post-order destroy]");
    double T[RUNS];

    for(int r=0;r<RUNS;r++){zm_reset();rng_state=0xbadf00dULL+(uint64_t)r;int rem=TREE_NODES;TNode*root=build_tree(&rem,zm_af);double t0=now_ns();destroy_zane(root);T[r]=now_ns()-t0;sink^=(int64_t)rem;}
    print_result("Zane cascade destroy", T);

    for(int r=0;r<RUNS;r++){rng_state=0xbadf00dULL+(uint64_t)r;int rem=TREE_NODES;TNode*root=build_tree(&rem,ma_af);double t0=now_ns();destroy_malloc(root);T[r]=now_ns()-t0;sink^=(int64_t)rem;}
    print_result("malloc cascade destroy", T);

    pool_flush();pool_warm(sizeof(TNode),TREE_NODES);
    for(int b=1;b<=MAX_BRANCH;b++) pool_warm((size_t)b*sizeof(TNode*),TREE_NODES/MAX_BRANCH);
    for(int r=0;r<RUNS;r++){rng_state=0xbadf00dULL+(uint64_t)r;int rem=TREE_NODES;TNode*root=build_tree(&rem,po_af);double t0=now_ns();destroy_pool(root);T[r]=now_ns()-t0;sink^=(int64_t)rem;}
    print_result("Pool cascade destroy", T);
}


/* ═══════════════════════════════════════════════════════════════════
   TEST 11 — Fragmentation stress: everything together
   200 cycles. Each cycle in order:
     1. Spawn 40 new Entity objects into random free slots
     2. Create 4 new List<store Entity> (inline buffer, cap 8)
     3. Push 30 times: copy a random live object into a random live list
        (list doubles its buffer when full, capped at 16 elements = 512B)
     4. Update all live objects: move (x+=id*0.1, y+=hp*0.05), drain hp;
        objects that reach hp≤0 are freed immediately (natural death)
     5. Read-scan all live lists: sum all inline element hp values
     6. Explicitly kill 25 random live objects
     7. Destroy 3 random live lists (frees their inline data buffer)
   Between runs the heap is fully reset. This models a real program:
   heterogeneous object lifetimes, mixed sizes (32B objects, 256–512B
   list buffers), interleaved alloc/free/iteration at every step.
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
    section("Test 11 -- Fragmentation stress  [200 cycles: spawn+list-create+push+update+kill]");
    double T[RUNS];
    zm_reset(); stress_run(T, zm_alloc_e, zm_free_e, 0); print_result("Zane (mmap + free-stacks)", T);
               stress_run(T, ma_alloc_e, ma_free_e, 0); print_result("malloc / free", T);
               stress_run(T, po_alloc_e, po_free_e, 1); print_result("Pool (per-size free-list)", T);
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

    zm_init(); ar_init(); pool_flush();

    test1(); test2(); test3(); test4(); test5();
    test6(); test7(); test8(); test9(); test10(); test11();

    printf("\n  (sink = %lld)\n\n", (long long)sink);
    return 0;
}
