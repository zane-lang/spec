

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

#define RUNS         20
#define N            100000
#define REGION_SIZE  (512UL * 1024 * 1024)

static inline double now_ns(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (double)ts.tv_sec * 1e9 + (double)ts.tv_nsec;
}

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

static void bench_deque_reset_locked(BenchDeque *q) { q->head = 0; q->tail = 0; }

static void bench_deque_push_locked(BenchDeque *q, BenchJob job) {
    assert((q->tail - q->head + 1) <= BENCH_POOL_MAX_JOBS);
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

#define ZM_ALIGN      8
#define ZM_MAXSZ      512
#define ZM_NC         (ZM_MAXSZ / ZM_ALIGN)
#define ZM_CHUNK      (1UL << 20)
#define ZM_WORDBITS   17
#define ZM_OFFMASK    ((1u << ZM_WORDBITS) - 1)
#define ZM_NCHUNKS    (REGION_SIZE / ZM_CHUNK)
#define ZM_LINE       64
#define ZM_LIST_MIN   128

#define ZA_PAYLOAD    0u
#define ZA_FORWARD    1u

#define ZD_NIL        0xFFFFFFFFu
#define ZD_STACKS     1024
#define ZM_RETIRE_MAX 262144

typedef uint32_t ZRef;

typedef struct { uint32_t target; uint32_t kind; } ZAnchor;
_Static_assert(sizeof(ZAnchor) == 8, "anchor cell must be one 8-byte slot");

typedef struct { uint32_t chunk; uint32_t off; int live; } ZBump;
typedef struct { uint32_t size; uint32_t align; uint32_t head; } ZSizeStack;

static struct {
    uint8_t   *base;
    uint32_t   next_chunk;
    uint8_t   *dir[ZM_NCHUNKS];
    ZBump      fixed;
    ZBump      dyn;
    ZBump      pool;
    ZSizeStack stacks[ZD_STACKS];
    ZRef       anchor_free;
    ZRef       retire[ZM_RETIRE_MAX];
    int        retire_top;
    uint64_t   anchors_made;
    uint64_t   forwarders_made;
} zm;

static inline uint32_t zm_seg(void *p) {
    assert((uint8_t*)p >= zm.base && (uint8_t*)p < zm.base + REGION_SIZE);
    size_t bo = (uint8_t*)p - zm.base;
    assert((bo & 7) == 0);
    return ((uint32_t)(bo >> 20) << ZM_WORDBITS) | (uint32_t)((bo & (ZM_CHUNK - 1)) >> 3);
}
static inline void *zm_resolve(uint32_t seg) {
    uint32_t chunk_id = seg >> ZM_WORDBITS;
    assert(chunk_id < ZM_NCHUNKS);
    return zm.dir[chunk_id] + ((size_t)(seg & ZM_OFFMASK) << 3);
}

static uint32_t zm_take_chunk(void) {
    assert(zm.next_chunk < ZM_NCHUNKS);
    return zm.next_chunk++;
}

static void *zbump(ZBump *b, size_t size, size_t align) {
    assert(size <= ZM_CHUNK);
    if (!b->live) { b->chunk = zm_take_chunk(); b->off = 0; b->live = 1; }
    size_t off = ((size_t)b->off + (align - 1)) & ~(align - 1);
    if (off + size > ZM_CHUNK) {
        b->chunk = zm_take_chunk();
        off = 0;
    }
    void *p = zm.dir[b->chunk] + off;
    b->off = (uint32_t)(off + size);
    return p;
}

static void *zm_fixed_alloc(size_t s) {
    return zbump(&zm.fixed, (s + ZM_ALIGN - 1) & ~(size_t)(ZM_ALIGN - 1), ZM_ALIGN);
}
static void zm_fixed_release(void *p, size_t s) { (void)p; (void)s; }

static ZSizeStack *zd_stack(size_t size, size_t align) {
    uint32_t h = ((uint32_t)size * 0x9E3779B1u) ^ ((uint32_t)align * 0x85EBCA6Bu);
    uint32_t i = h & (ZD_STACKS - 1);
    for (uint32_t probe = 0; probe < ZD_STACKS; probe++) {
        ZSizeStack *s = &zm.stacks[(i + probe) & (ZD_STACKS - 1)];
        if (s->size == 0) {
            s->size = (uint32_t)size; s->align = (uint32_t)align; s->head = ZD_NIL;
            return s;
        }
        if (s->size == size && s->align == align) return s;
    }
    assert(0);
    return NULL;
}

static void *zd_span(size_t bytes) {
    size_t n = (bytes + ZM_CHUNK - 1) / ZM_CHUNK;
    uint32_t first = zm_take_chunk();
    for (size_t i = 1; i < n; i++) (void)zm_take_chunk();
    return zm.dir[first];
}

static void *zd_try_stack(size_t size, size_t align) {
    ZSizeStack *s = zd_stack(size, align);
    if (s->head == ZD_NIL) return NULL;
    void *p = zm_resolve(s->head);
    s->head = *(uint32_t*)p;
    return p;
}

static void *zd_alloc(size_t size, size_t align) {
    void *p = zd_try_stack(size, align);
    if (p) return p;
    if (size > ZM_CHUNK) return zd_span(size);
    return zbump(&zm.dyn, size, align);
}
static void zd_free(void *p, size_t size, size_t align) {
    ZSizeStack *s = zd_stack(size, align);
    *(uint32_t*)p = s->head;
    s->head = zm_seg(p);
}

static int zd_grow_in_place(void *block, size_t old_bytes, size_t new_bytes) {
    if (new_bytes > ZM_CHUNK) return 0;
    if (!zm.dyn.live) return 0;
    uint32_t seg = zm_seg(block);
    size_t off = (size_t)(seg & ZM_OFFMASK) << 3;
    if ((seg >> ZM_WORDBITS) != zm.dyn.chunk) return 0;
    if (off + old_bytes != zm.dyn.off) return 0;
    size_t extra = new_bytes - old_bytes;
    if ((size_t)zm.dyn.off + extra > ZM_CHUNK) return 0;
    zm.dyn.off += (uint32_t)extra;
    return 1;
}

static void *za_page_alloc(void) {
#ifdef ZM_INTERLEAVE
    return zbump(&zm.fixed, sizeof(ZAnchor), ZM_ALIGN);
#else
    if (!zm.pool.live) {
        zm.pool.chunk = zm_take_chunk();
        zm.pool.off = (zm.pool.chunk == 0) ? ZM_ALIGN : 0;
        zm.pool.live = 1;
    }
    return zbump(&zm.pool, sizeof(ZAnchor), ZM_ALIGN);
#endif
}

static ZRef za_alloc(void) {
    ZAnchor *a;
    ZRef r;
    if (zm.anchor_free) {
        r = zm.anchor_free;
        a = (ZAnchor*)zm_resolve(r);
        zm.anchor_free = a->target;
    } else {
        a = (ZAnchor*)za_page_alloc();
        r = zm_seg(a);
        assert(r != 0);
    }
    a->target = 0;
    a->kind = ZA_PAYLOAD;
    zm.anchors_made++;
    return r;
}
static void za_release(ZRef r) {
    ZAnchor *a = (ZAnchor*)zm_resolve(r);
    a->target = zm.anchor_free;
    a->kind = ZA_PAYLOAD;
    zm.anchor_free = r;
}

static inline ZRef *zm_backptr(void *obj, size_t obj_size) {
    return (ZRef*)((uint8_t*)obj + obj_size - sizeof(ZRef));
}

static void *zm_host(size_t obj_size) {
    void *obj = zm_fixed_alloc(obj_size);
    *zm_backptr(obj, obj_size) = 0;
    return obj;
}
static void zm_host_release(void *obj, size_t obj_size) { (void)obj; (void)obj_size; }

static void zm_host_destroy(void *obj, size_t obj_size) {
    ZRef *bp = zm_backptr(obj, obj_size);
    if (*bp) { za_release(*bp); *bp = 0; }
}

static void zm_overwrite(void *slot, size_t obj_size) {
    ZRef bp = *zm_backptr(slot, obj_size);
    if (bp) ((ZAnchor*)zm_resolve(bp))->target = zm_seg(slot);
}

static ZRef zm_mint_guest(void *obj, size_t obj_size) {
    ZRef *bp = zm_backptr(obj, obj_size);
    if (*bp) return *bp;
    ZRef r = za_alloc();
    ((ZAnchor*)zm_resolve(r))->target = zm_seg(obj);
    *bp = r;
    return r;
}

static inline void *zm_deref(ZRef t) {
    assert(t != 0);
    ZAnchor *a = (ZAnchor*)zm_resolve(t);
    while (a->kind == ZA_FORWARD) a = (ZAnchor*)zm_resolve(a->target);
    return zm_resolve(a->target);
}
static inline ZRef zm_terminal(ZRef t) {
    ZAnchor *a = (ZAnchor*)zm_resolve(t);
    while (a->kind == ZA_FORWARD) { t = a->target; a = (ZAnchor*)zm_resolve(t); }
    return t;
}
static inline void *zm_deref_compress(ZRef *t) {
    ZRef cur = *t;
    ZAnchor *a = (ZAnchor*)zm_resolve(cur);
    while (a->kind == ZA_FORWARD) { cur = a->target; a = (ZAnchor*)zm_resolve(cur); }
    *t = cur;
    return zm_resolve(a->target);
}

static void zm_rehost(void *src, void *dst, size_t obj_size) {
    ZRef sbp = *zm_backptr(src, obj_size);
    ZRef dbp = *zm_backptr(dst, obj_size);
    memcpy(dst, src, obj_size - sizeof(ZRef));
    if (dbp == 0) {
        if (sbp) ((ZAnchor*)zm_resolve(sbp))->target = zm_seg(dst);
        *zm_backptr(dst, obj_size) = sbp;
        *zm_backptr(src, obj_size) = sbp;
        return;
    }
    ((ZAnchor*)zm_resolve(dbp))->target = zm_seg(dst);
    if (sbp && sbp != dbp && zm_terminal(sbp) != dbp) {
        ZAnchor *sa = (ZAnchor*)zm_resolve(sbp);
        sa->target = dbp;
        sa->kind = ZA_FORWARD;
        assert(zm.retire_top < ZM_RETIRE_MAX);
        zm.retire[zm.retire_top++] = sbp;
        zm.forwarders_made++;
    }
    *zm_backptr(dst, obj_size) = dbp;
    *zm_backptr(src, obj_size) = dbp;
}

static void zm_scope_drain(void) {
    while (zm.retire_top > 0) za_release(zm.retire[--zm.retire_top]);
}

static void zm_reset(void) {
    zm.next_chunk = 0;
    zm.fixed.live = zm.dyn.live = zm.pool.live = 0;
    zm.fixed.off  = zm.dyn.off  = zm.pool.off  = 0;
    memset(zm.stacks, 0, sizeof(zm.stacks));
    zm.anchor_free = 0;
    zm.retire_top = 0;
    zm.fixed.chunk = zm_take_chunk();
    zm.fixed.live = 1;
#ifdef ZM_INTERLEAVE
    zm.fixed.off = ZM_ALIGN;
#endif
}

static void zm_init(void) {
    zm.base = mmap(NULL, REGION_SIZE, PROT_READ | PROT_WRITE,
                   MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    assert(zm.base != MAP_FAILED);
    for (size_t i = 0; i < REGION_SIZE; i += 4096) zm.base[i] = 0;
    for (uint32_t c = 0; c < ZM_NCHUNKS; c++) zm.dir[c] = zm.base + (size_t)c * ZM_CHUNK;
    zm.anchors_made = 0;
    zm.forwarders_made = 0;
    zm_reset();
}

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

static inline size_t zm_round(size_t s) { return (s + ZM_ALIGN-1) & ~(size_t)(ZM_ALIGN-1); }
static inline int    zm_cls(size_t s)   { return (int)(s / ZM_ALIGN) - 1; }

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

#define CCHUNK 64

typedef struct {
    Entity **chunks;
    int      nchunks;
    int      cap_chunks;
    int      len;
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

static void test1(void) {
    section("Test 1 -- Sequential alloc + sequential free  [32 bytes x 100k]");
    double T[RUNS];
    void **ptrs = (void**)malloc(N * sizeof(void *));

    for (int r=0;r<RUNS;r++) { zm_reset(); double t0=now_ns(); for(int i=0;i<N;i++) ptrs[i]=zm_host(40); for(int i=0;i<N;i++) zm_host_release(ptrs[i],40); T[r]=now_ns()-t0; }
    print_result("Zane (fixed-region bump, lazy anchors)", T);

    for (int r=0;r<RUNS;r++) { double t0=now_ns(); for(int i=0;i<N;i++) ptrs[i]=malloc(32); for(int i=0;i<N;i++) free(ptrs[i]); T[r]=now_ns()-t0; }
    print_result("malloc / free", T);

    for (int r=0;r<RUNS;r++) { ar_reset(); double t0=now_ns(); for(int i=0;i<N;i++) ptrs[i]=ar_alloc(32); sink^=(int64_t)(uintptr_t)ptrs[N-1]; ar_reset(); T[r]=now_ns()-t0; }
    print_result("Arena (bump + O(1) reset)", T);

    pool_flush(); pool_warm(32,N);
    for (int r=0;r<RUNS;r++) { double t0=now_ns(); for(int i=0;i<N;i++) ptrs[i]=pool_alloc(32); for(int i=0;i<N;i++) pool_free(ptrs[i],32); T[r]=now_ns()-t0; }
    print_result("Pool (per-size free-list)", T);

    free(ptrs);
}

static void test2(void) {
    section("Test 2 -- Random-order free only  [32B x 100k  |  alloc+shuffle NOT timed]");
    double T[RUNS];
    void **ptrs = (void**)malloc(N * sizeof(void *));

    for(int r=0;r<RUNS;r++){zm_reset();rng_state=0xfeed0000ULL+(uint64_t)r;for(int i=0;i<N;i++)ptrs[i]=zm_host(40);shuf_ptrs(ptrs,N);double t0=now_ns();for(int i=0;i<N;i++)zm_host_release(ptrs[i],40);T[r]=now_ns()-t0;}
    print_result("Zane (fixed-region bump, lazy anchors)", T);

    for(int r=0;r<RUNS;r++){rng_state=0xfeed0000ULL+(uint64_t)r;for(int i=0;i<N;i++)ptrs[i]=malloc(32);shuf_ptrs(ptrs,N);double t0=now_ns();for(int i=0;i<N;i++)free(ptrs[i]);T[r]=now_ns()-t0;}
    print_result("malloc / free", T);

    pool_flush();pool_warm(32,N);
    for(int r=0;r<RUNS;r++){rng_state=0xfeed0000ULL+(uint64_t)r;for(int i=0;i<N;i++)ptrs[i]=pool_alloc(32);shuf_ptrs(ptrs,N);double t0=now_ns();for(int i=0;i<N;i++)pool_free(ptrs[i],32);T[r]=now_ns()-t0;}
    print_result("Pool (per-size free-list)", T);

    free(ptrs);
}

static const size_t MIXED_SIZES[] = { 8, 16, 32, 64 };
#define NMS 4

static void test3(void) {
    section("Test 3 -- Mixed sizes (8/16/32/64B) alloc + random-order free  [100k total]");
    double T[RUNS];
    PS    *pairs = (PS*)malloc(N * sizeof(PS));
    size_t *szseq = (size_t*)malloc(N * sizeof(size_t));
    for(int i=0;i<N;i++) szseq[i]=MIXED_SIZES[i%NMS];

    for(int r=0;r<RUNS;r++){zm_reset();rng_state=0xbabe0000ULL+(uint64_t)r;double t0=now_ns();for(int i=0;i<N;i++){pairs[i].p=zm_fixed_alloc(szseq[i]);pairs[i].s=szseq[i];}shuf_ps(pairs,N);for(int i=0;i<N;i++)zm_fixed_release(pairs[i].p,pairs[i].s);T[r]=now_ns()-t0;}
    print_result("Zane (fixed-region bump)", T);

    for(int r=0;r<RUNS;r++){rng_state=0xbabe0000ULL+(uint64_t)r;double t0=now_ns();for(int i=0;i<N;i++){pairs[i].p=malloc(szseq[i]);pairs[i].s=szseq[i];}shuf_ps(pairs,N);for(int i=0;i<N;i++)free(pairs[i].p);T[r]=now_ns()-t0;}
    print_result("malloc / free", T);

    {void **ap=(void**)malloc(N*sizeof(void*));for(int r=0;r<RUNS;r++){ar_reset();double t0=now_ns();for(int i=0;i<N;i++)ap[i]=ar_alloc(szseq[i]);sink^=(int64_t)(uintptr_t)ap[N-1];ar_reset();T[r]=now_ns()-t0;}print_result("Arena (bulk reset)",T);free(ap);}

    pool_flush();for(int s=0;s<NMS;s++)pool_warm(MIXED_SIZES[s],N/NMS);
    for(int r=0;r<RUNS;r++){rng_state=0xbabe0000ULL+(uint64_t)r;double t0=now_ns();for(int i=0;i<N;i++){pairs[i].p=pool_alloc(szseq[i]);pairs[i].s=szseq[i];}shuf_ps(pairs,N);for(int i=0;i<N;i++)pool_free(pairs[i].p,pairs[i].s);T[r]=now_ns()-t0;}
    print_result("Pool (per-size free-list)", T);

    free(pairs);free(szseq);
}

static void test4(void) {
    section("Test 4 -- Iteration: inline (hosted) vs pointer-chase  [32B Entity x 100k]");
    double T[RUNS];

    Entity *inl=(Entity*)malloc(N*sizeof(Entity));
    for(int i=0;i<N;i++){inl[i].id=i;inl[i].x=i*1.1;inl[i].y=i*2.2;inl[i].hp=i%100+1;}

    Entity **sp=(Entity**)malloc(N*sizeof(Entity*));
    for(int i=0;i<N;i++){sp[i]=(Entity*)malloc(sizeof(Entity));sp[i]->id=i;sp[i]->x=i*1.1;sp[i]->y=i*2.2;sp[i]->hp=i%100+1;}

    Entity **sh=(Entity**)malloc(N*sizeof(Entity*));memcpy(sh,sp,N*sizeof(Entity*));rng_state=0xf0f0f0f0ULL;shuf_ptrs((void**)sh,N);

    UList ul; ulist_init(&ul);
    for(int i=0;i<N;i++){Entity e={i,i*1.1,i*2.2,i%100+1,0};ulist_push(&ul,e);}

    CChunked cc; cchunked_init(&cc);
    for(int i=0;i<N;i++){Entity e={i,i*1.1,i*2.2,i%100+1,0};cchunked_push(&cc,e);}

    {int64_t w=0;for(int i=0;i<N;i++)w+=inl[i].hp;sink^=w;}
    {int64_t w=0;for(int i=0;i<N;i++)w+=sp[i]->hp;sink^=w;}
    {int64_t w=0;for(int i=0;i<N;i++)w+=sh[i]->hp;sink^=w;}
    {int64_t w=0;for(UChunk*c=ul.head;c;c=c->next)for(int j=0;j<c->len;j++)w+=c->data[j].hp;sink^=w;}
    {int64_t w=0;for(int i=0;i<cc.len;i++)w+=cchunked_get(&cc,i)->hp;sink^=w;}

    for(int r=0;r<RUNS;r++){int64_t acc=0;double t0=now_ns();for(int i=0;i<N;i++)acc+=inl[i].hp;T[r]=now_ns()-t0;sink^=acc;}
    print_result("Inline array  (Array<Entity, 100000>)", T);

    for(int r=0;r<RUNS;r++){int64_t acc=0;double t0=now_ns();for(int i=0;i<N;i++)acc+=sp[i]->hp;T[r]=now_ns()-t0;sink^=acc;}
    print_result("Pointer array, sequential", T);

    for(int r=0;r<RUNS;r++){int64_t acc=0;double t0=now_ns();for(int i=0;i<N;i++)acc+=sh[i]->hp;T[r]=now_ns()-t0;sink^=acc;}
    print_result("Pointer array, shuffled", T);

    for(int r=0;r<RUNS;r++){int64_t acc=0;double t0=now_ns();for(UChunk*c=ul.head;c;c=c->next)for(int j=0;j<c->len;j++)acc+=c->data[j].hp;T[r]=now_ns()-t0;sink^=acc;}
    print_result("UList (chunk=8, linked)",T);

    for(int r=0;r<RUNS;r++){int64_t acc=0;double t0=now_ns();for(int i=0;i<cc.len;i++)acc+=cchunked_get(&cc,i)->hp;T[r]=now_ns()-t0;sink^=acc;}
    print_result("CChunked (chunk=64, ptr-array)",T);

    ulist_free_all(&ul);
    cchunked_free_all(&cc);
    free(inl);for(int i=0;i<N;i++)free(sp[i]);free(sp);free(sh);
}

typedef struct { Entity *base; size_t len, cap, block; } ZList;
typedef struct { Entity *base; size_t len, cap; } CVec;

static size_t zlist_first_block(size_t stride) {
    size_t b = ZM_LIST_MIN;
    while (b < stride) b <<= 1;
    return b;
}
static void zlist_init(ZList *l) {
    l->block = zlist_first_block(sizeof(Entity));
    l->base  = (Entity*)zd_alloc(l->block, ZM_LINE);
    l->len   = 0;
    l->cap   = l->block / sizeof(Entity);
}
static void zlist_push(ZList *l, Entity e) {
    if (l->len == l->cap) {
        size_t want = l->block * 2;
        Entity *nb = (Entity*)zd_try_stack(want, ZM_LINE);
        if (!nb && !zd_grow_in_place(l->base, l->block, want)) {
            nb = (want > ZM_CHUNK) ? (Entity*)zd_span(want)
                                   : (Entity*)zbump(&zm.dyn, want, ZM_LINE);
        }
        if (nb) {
            memcpy(nb, l->base, l->len * sizeof(Entity));
            zd_free(l->base, l->block, ZM_LINE);
            l->base = nb;
        }
        l->block = want;
        l->cap   = l->block / sizeof(Entity);
    }
    l->base[l->len++] = e;
}
static void cvec_push(CVec *v, Entity e) {
    if(v->len==v->cap){v->cap=v->cap?v->cap*2:8;v->base=(Entity*)realloc(v->base,v->cap*sizeof(Entity));}
    v->base[v->len++]=e;
}

static void test5(void) {
    section("Test 5 -- List backing-store growth  [push 100k x 32B Entity items]");
    double T[RUNS];
    Entity tmpl={42,1.5,2.5,99,0};

    for(int r=0;r<RUNS;r++){zm_reset();ZList l;zlist_init(&l);double t0=now_ns();for(int i=0;i<N;i++)zlist_push(&l,tmpl);T[r]=now_ns()-t0;sink^=(int64_t)l.len;}
    print_result("Zane List (128B start, doubling)", T);

    for(int r=0;r<RUNS;r++){CVec v={NULL,0,0};double t0=now_ns();for(int i=0;i<N;i++)cvec_push(&v,tmpl);T[r]=now_ns()-t0;sink^=(int64_t)v.len;free(v.base);}
    print_result("C realloc vector", T);

    for(int r=0;r<RUNS;r++){UList ul;ulist_init(&ul);double t0=now_ns();for(int i=0;i<N;i++)ulist_push(&ul,tmpl);T[r]=now_ns()-t0;sink^=(int64_t)ul.total;ulist_free_all(&ul);}
    print_result("UList (chunk=8, no realloc)", T);

    for(int r=0;r<RUNS;r++){CChunked cc;cchunked_init(&cc);double t0=now_ns();for(int i=0;i<N;i++)cchunked_push(&cc,tmpl);T[r]=now_ns()-t0;sink^=(int64_t)cc.len;cchunked_free_all(&cc);}
    print_result("CChunked (chunk=64, ptr-array)", T);
}

static void test6(void) {
    section("Test 6 -- Guest access via segmented tether vs direct pointer  [100k accesses]");
    double T[RUNS];

    zm_reset();
    Entity **objs   = (Entity**)malloc(N * sizeof(Entity*));
    Entity **direct = (Entity**)malloc(N * sizeof(Entity*));
    ZRef    *refs   = (ZRef*)malloc(N * sizeof(ZRef));

    for(int i=0;i<N;i++){
        objs[i] = (Entity*)zm_host(sizeof(Entity)+sizeof(ZRef));
        objs[i]->hp = i%100+1;
        refs[i] = zm_mint_guest(objs[i], sizeof(Entity)+sizeof(ZRef));
        direct[i] = objs[i];
    }

    for(int r=0;r<RUNS;r++){
        for(int i=0;i<N;i++) sink^=(int64_t)direct[i]->hp;
        int64_t acc=0; double t0=now_ns();
        for(int i=0;i<N;i++) acc+=direct[i]->hp;
        T[r]=now_ns()-t0; sink^=acc;
    }
    print_result("Direct pointer (baseline)", T);

    for(int r=0;r<RUNS;r++){
        uint8_t **dir = zm.dir;
        for(int i=0;i<N;i++){
            uint32_t cs=refs[i];
            uint32_t os=*(uint32_t*)(dir[cs>>ZM_WORDBITS] + ((size_t)(cs&ZM_OFFMASK)<<3));
            sink^=(int64_t)((Entity*)(dir[os>>ZM_WORDBITS] + ((size_t)(os&ZM_OFFMASK)<<3)))->hp;
        }
        int64_t acc=0; double t0=now_ns();
        for(int i=0;i<N;i++){
            uint32_t cs=refs[i];
            uint32_t os=*(uint32_t*)(dir[cs>>ZM_WORDBITS] + ((size_t)(cs&ZM_OFFMASK)<<3));
            Entity *e=(Entity*)(dir[os>>ZM_WORDBITS] + ((size_t)(os&ZM_OFFMASK)<<3));
            acc+=e->hp;
        }
        T[r]=now_ns()-t0; sink^=acc;
    }
    print_result("Segmented tether (chunk dir cached)", T);

    for(int r=0;r<RUNS;r++){
        for(int i=0;i<N;i++){
            uint32_t cs=refs[i];
            uint32_t os=*(uint32_t*)(zm.dir[cs>>ZM_WORDBITS] + ((size_t)(cs&ZM_OFFMASK)<<3));
            sink^=(int64_t)((Entity*)(zm.dir[os>>ZM_WORDBITS] + ((size_t)(os&ZM_OFFMASK)<<3)))->hp;
        }
        int64_t acc=0; double t0=now_ns();
        for(int i=0;i<N;i++){
            __asm__ volatile("" ::: "memory");
            uint32_t cs=refs[i];
            uint32_t os=*(uint32_t*)(zm.dir[cs>>ZM_WORDBITS] + ((size_t)(cs&ZM_OFFMASK)<<3));
            Entity *e=(Entity*)(zm.dir[os>>ZM_WORDBITS] + ((size_t)(os&ZM_OFFMASK)<<3));
            acc+=e->hp;
        }
        T[r]=now_ns()-t0; sink^=acc;
    }
    print_result("Segmented tether (chunk dir reloaded)", T);

    for(int i=0;i<N;i++) zm_host_release(objs[i], sizeof(Entity)+sizeof(ZRef));
    free(objs);free(direct);free(refs);
}

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

typedef void*(*AllocFn)(size_t);
typedef void (*FreeFn)(void*,size_t);

static void *zm_alloc_e(size_t s){return zm_host(s+sizeof(ZRef));}
static void  zm_free_e (void*p,size_t s){zm_host_release(p,s+sizeof(ZRef));}
static void *ma_alloc_e(size_t s){return malloc(s);}
static void  ma_free_e (void*p,size_t s){(void)s;free(p);}
static void *po_alloc_e(size_t s){return pool_alloc(s);}
static void  po_free_e (void*p,size_t s){pool_free(p,s);}

static void game_loop_run(double T[RUNS], AllocFn af, FreeFn ff, int prewarm) {
    if (prewarm) { pool_flush(); pool_warm(sizeof(Entity), MAX_ENTITIES); }
    for (int r=0; r<RUNS; r++) {
        if (!prewarm) { if (af==zm_alloc_e) zm_reset(); }
        else zm_reset();
        rng_state = 0x7e57c0deULL + (uint64_t)r;
        EntityPool ep; ep_init(&ep, MAX_ENTITIES);
        double t0 = now_ns();
        for (int frame=0; frame<GAME_FRAMES; frame++) {

            for (int s=0; s<SPAWN_PER_FRAME && ep.count<MAX_ENTITIES-1; s++) {
                Entity *e=(Entity*)af(sizeof(Entity));
                e->x=(double)(rng()%1000); e->y=(double)(rng()%1000);
                e->id=(int64_t)(rng()%100); e->hp=50+(int32_t)(rng()%50);
                ep_add(&ep,e);
            }

            int killed=0;
            for (int i=0; i<ep.cap&&killed<KILL_PER_FRAME; i++) {
                if(ep.slots[i]){ff(ep.slots[i],sizeof(Entity));ep_remove(&ep,i);killed++;}
            }

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

static void test7(void) {
    section("Test 7 -- Game loop  [500 frames: 30 spawns + 20+ kills + update per frame]");
    double T[RUNS];
    zm_reset(); game_loop_run(T, zm_alloc_e, zm_free_e, 0); print_result("Zane (fixed-region bump)", T);
             game_loop_run(T, ma_alloc_e, ma_free_e, 0); print_result("malloc / free", T);
             game_loop_run(T, po_alloc_e, po_free_e, 1); print_result("Pool (per-size free-list)", T);
}

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
    zm_reset(); particle_run(T, zm_alloc_e, zm_free_e, 0); print_result("Zane (fixed-region bump)", T);
              particle_run_parallel(T, zm_alloc_e, zm_free_e, 0); print_result("Zane + work-stealing update", T);
              particle_run(T, ma_alloc_e, ma_free_e, 0); print_result("malloc / free", T);
              particle_run(T, po_alloc_p, po_free_p, 1); print_result("Pool (per-size free-list)", T);
}

static void test9(void) {
    section("Test 9 -- Checkerboard fragmentation + refill  [alloc 100k, free evens, alloc 50k (timed)]");
    double T[RUNS];
    void **ptrs=(void**)malloc(N*sizeof(void*));

    for(int r=0;r<RUNS;r++){
        zm_reset();
        for(int i=0;i<N;i++){ptrs[i]=zm_host(40);((Entity*)ptrs[i])->hp=i;}
        for(int i=0;i<N;i+=2) zm_host_release(ptrs[i],40);
        double t0=now_ns();
        for(int i=0;i<N/2;i++) ptrs[i]=zm_host(40);
        T[r]=now_ns()-t0; sink^=(int64_t)(uintptr_t)ptrs[0];
    }
    print_result("Zane -- refill (fixed-region bump)", T);

    {
        void **refill=(void**)malloc((N/2)*sizeof(void*));
        for(int r=0;r<RUNS;r++){
            for(int i=0;i<N;i++){ptrs[i]=malloc(32);((Entity*)ptrs[i])->hp=i;}
            for(int i=0;i<N;i+=2) free(ptrs[i]);
            double t0=now_ns();
            for(int i=0;i<N/2;i++) refill[i]=malloc(32);
            T[r]=now_ns()-t0; sink^=(int64_t)(uintptr_t)refill[0];
            for(int i=0;i<N/2;i++) free(refill[i]);
            for(int i=1;i<N;i+=2) free(ptrs[i]);
        }
        print_result("malloc -- refill fragmented heap", T);
        free(refill);
    }

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

#define TREE_NODES 4000
#define MAX_BRANCH 6

typedef void*(*ChildAllocFn)(int);
typedef void (*ChildFreeFn)(void*,int);

static void *zane_children_alloc(int n){ (void)n; return zd_alloc(ZM_LIST_MIN, ZM_LINE); }
static void  zane_children_free(void*p,int n){ (void)n; zd_free(p, ZM_LIST_MIN, ZM_LINE); }
static void *ma_children_alloc(int n){ return malloc((size_t)n*sizeof(TNode*)); }
static void  ma_children_free(void*p,int n){ (void)n; free(p); }
static void *po_children_alloc(int n){ return pool_alloc((size_t)n*sizeof(TNode*)); }
static void  po_children_free(void*p,int n){ pool_free(p,(size_t)n*sizeof(TNode*)); }

static TNode *build_tree(int *rem, AllocFn af, ChildAllocFn caf) {
    if (*rem<=0) return NULL;
    TNode *node=(TNode*)af(sizeof(TNode));
    node->value=(int64_t)(rng()%1000);
    node->nchildren=(*rem>1)?(int)(rng()%MAX_BRANCH):0;
    if (node->nchildren>*rem-1) node->nchildren=*rem-1;
    (*rem)--;
    if (node->nchildren>0) {
        node->children=(TNode**)caf(node->nchildren);
        for(int i=0;i<node->nchildren;i++) node->children[i]=build_tree(rem,af,caf);
    } else node->children=NULL;
    return node;
}

static void destroy_zane(TNode *n) {
    if(!n) return;
    for(int i=0;i<n->nchildren;i++) destroy_zane(n->children[i]);
    if(n->children) zane_children_free(n->children,n->nchildren);
    zm_host_destroy(n, sizeof(TNode)+sizeof(ZRef));
}

static void tree_mint_guests(TNode *n, size_t obj_size) {
    if (!n) return;
    zm_mint_guest(n, obj_size);
    for (int i=0;i<n->nchildren;i++) tree_mint_guests(n->children[i], obj_size);
}

static void destroy_malloc(TNode *n){ if(!n)return; for(int i=0;i<n->nchildren;i++) destroy_malloc(n->children[i]); if(n->children)ma_children_free(n->children,n->nchildren); free(n); }
static void destroy_pool(TNode *n)  { if(!n)return; for(int i=0;i<n->nchildren;i++) destroy_pool(n->children[i]);  if(n->children)po_children_free(n->children,n->nchildren); pool_free(n,sizeof(TNode)); }

static void *zm_af(size_t s){return zm_host(s+sizeof(ZRef));}
static void *ma_af(size_t s){return malloc(s);}
static void *po_af(size_t s){return pool_alloc(s);}

static void test10(void) {
    section("Test 10 -- Hosting tree teardown  [~4000 nodes, cascade post-order destroy]");
    double T[RUNS];
    size_t znode_size = sizeof(TNode)+sizeof(ZRef);

    for(int r=0;r<RUNS;r++){zm_reset();rng_state=0xbadf00dULL+(uint64_t)r;int rem=TREE_NODES;TNode*root=build_tree(&rem,zm_af,zane_children_alloc);double t0=now_ns();destroy_zane(root);T[r]=now_ns()-t0;sink^=(int64_t)rem;}
    print_result("Zane — no guests", T);

    for(int r=0;r<RUNS;r++){
        zm_reset(); rng_state=0xbadf00dULL+(uint64_t)r;
        int rem=TREE_NODES; TNode*root=build_tree(&rem,zm_af,zane_children_alloc);
        tree_mint_guests(root, znode_size);
        double t0=now_ns();
        destroy_zane(root);
        T[r]=now_ns()-t0; sink^=(int64_t)rem;
    }
    print_result("Zane — one guest per node", T);

    for(int r=0;r<RUNS;r++){
        zm_reset(); rng_state=0xbadf00dULL+(uint64_t)r;
        int rem=TREE_NODES; TNode*root=build_tree(&rem,zm_af,zane_children_alloc);
        zm_mint_guest(root, znode_size);
        double t0=now_ns();
        destroy_zane(root);
        T[r]=now_ns()-t0; sink^=(int64_t)rem;
    }
    print_result("Zane — single root guest", T);

    for(int r=0;r<RUNS;r++){rng_state=0xbadf00dULL+(uint64_t)r;int rem=TREE_NODES;TNode*root=build_tree(&rem,ma_af,ma_children_alloc);double t0=now_ns();destroy_malloc(root);T[r]=now_ns()-t0;sink^=(int64_t)rem;}
    print_result("malloc cascade destroy", T);

    pool_flush();pool_warm(sizeof(TNode),TREE_NODES);
    for(int b=1;b<=MAX_BRANCH;b++) pool_warm((size_t)b*sizeof(TNode*),TREE_NODES/MAX_BRANCH);
    for(int r=0;r<RUNS;r++){rng_state=0xbadf00dULL+(uint64_t)r;int rem=TREE_NODES;TNode*root=build_tree(&rem,po_af,po_children_alloc);double t0=now_ns();destroy_pool(root);T[r]=now_ns()-t0;sink^=(int64_t)rem;}
    print_result("Pool cascade destroy", T);
}

#define STRESS_CYCLES       200
#define STRESS_MAX_OBJ      3000
#define STRESS_MAX_LISTS    300
#define STRESS_SPAWN_OBJ    40
#define STRESS_KILL_OBJ     25
#define STRESS_LIST_NEW     4
#define STRESS_LIST_FREE    3
#define STRESS_PUSH_OPS     30
#define STRESS_LIST_MAXLEN  16

typedef void*(*BufAllocFn)(size_t);
typedef void (*BufFreeFn)(void*,size_t);

static void *zane_buf_alloc(size_t bytes){ return zd_alloc(bytes, ZM_LINE); }
static void  zane_buf_free (void*p,size_t bytes){ zd_free(p, bytes, ZM_LINE); }
static void *ma_buf_alloc(size_t bytes){ return malloc(bytes); }
static void  ma_buf_free (void*p,size_t bytes){ (void)bytes; free(p); }
static void *po_buf_alloc(size_t bytes){ return pool_alloc(bytes); }
static void  po_buf_free (void*p,size_t bytes){ pool_free(p,bytes); }

typedef struct { Entity *data; int len, cap; size_t block; } SList;

static void slist_open(SList *l, BufAllocFn baf) {
    l->block = ZM_LIST_MIN;
    while (l->block < sizeof(Entity)) l->block <<= 1;
    l->data  = (Entity*)baf(l->block);
    l->len   = 0;
    l->cap   = (int)(l->block / sizeof(Entity));
}
static void slist_push(SList *l, Entity e, BufAllocFn baf, BufFreeFn bff) {
    if (l->len == l->cap) {
        size_t want = l->block * 2;
        Entity *nb = (Entity*)baf(want);
        memcpy(nb, l->data, (size_t)l->len * sizeof(Entity));
        bff(l->data, l->block);
        l->data  = nb;
        l->block = want;
        l->cap   = (int)(want / sizeof(Entity));
    }
    l->data[l->len++] = e;
}

static void stress_run(double T[RUNS], AllocFn af, FreeFn ff,
                       BufAllocFn baf, BufFreeFn bff, int prewarm) {
    if (prewarm) {
        pool_flush();
        pool_warm(sizeof(Entity), STRESS_MAX_OBJ);
        for (size_t b = ZM_LIST_MIN; b <= ZM_LIST_MIN * 4; b <<= 1)
            pool_warm(b, STRESS_MAX_LISTS);
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

            for (int s = 0; s < STRESS_LIST_NEW && list_count < STRESS_MAX_LISTS; s++) {
                int start = (int)(rng() % STRESS_MAX_LISTS);
                for (int i = 0; i < STRESS_MAX_LISTS; i++) {
                    int idx = (start + i) % STRESS_MAX_LISTS;
                    if (!lists[idx].cap) {
                        slist_open(&lists[idx], baf);
                        list_count++; break;
                    }
                }
            }

            if (obj_count > 0 && list_count > 0) {
                for (int p = 0; p < STRESS_PUSH_OPS; p++) {
                    int li = (int)(rng() % STRESS_MAX_LISTS);
                    if (!lists[li].cap || lists[li].len >= STRESS_LIST_MAXLEN) continue;
                    int oi = (int)(rng() % STRESS_MAX_OBJ);
                    if (!objs[oi]) continue;
                    slist_push(&lists[li], *objs[oi], baf, bff);
                }
            }

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

            for (int i = 0; i < STRESS_MAX_LISTS; i++) {
                if (!lists[i].cap) continue;
                for (int j = 0; j < lists[i].len; j++) acc += lists[i].data[j].hp;
            }
            sink ^= acc;

            int killed = 0;
            for (int tries = 0; tries < STRESS_MAX_OBJ && killed < STRESS_KILL_OBJ; tries++) {
                int idx = (int)(rng() % STRESS_MAX_OBJ);
                if (objs[idx]) {
                    ff(objs[idx], sizeof(Entity)); objs[idx] = NULL;
                    obj_count--; killed++;
                }
            }

            int lkilled = 0;
            for (int tries = 0; tries < STRESS_MAX_LISTS && lkilled < STRESS_LIST_FREE; tries++) {
                int idx = (int)(rng() % STRESS_MAX_LISTS);
                if (lists[idx].cap) {
                    bff(lists[idx].data, lists[idx].block);
                    lists[idx].data = NULL; lists[idx].len = lists[idx].cap = 0;
                    lists[idx].block = 0;
                    list_count--; lkilled++;
                }
            }
        }

        for (int i = 0; i < STRESS_MAX_OBJ;  i++) if (objs[i])      { ff(objs[i], sizeof(Entity)); }
        for (int i = 0; i < STRESS_MAX_LISTS; i++) if (lists[i].cap) { bff(lists[i].data, lists[i].block); }

        T[r] = now_ns() - t0;
    }

    free(objs); free(lists);
}

static void test11(void) {
    section("Test 11 -- Fragmentation stress  [200 cycles: spawn+list-create+push+update+kill]");
    double T[RUNS];
    zm_reset(); stress_run(T, zm_alloc_e, zm_free_e, zane_buf_alloc, zane_buf_free, 0);
    print_result("Zane (fixed bump + size stacks)", T);
               stress_run(T, ma_alloc_e, ma_free_e, ma_buf_alloc, ma_buf_free, 0);
    print_result("malloc / free", T);
               stress_run(T, po_alloc_e, po_free_e, po_buf_alloc, po_buf_free, 1);
    print_result("Pool (per-size free-list)", T);
}

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
    section("Test 12 -- Concurrent shard scan  [4 x Array<Entity, 25000> read-only shard sums]");
    double T[RUNS];
    assert((N % BENCH_POOL_WORKERS) == 0);

    Entity *hosted = (Entity*)malloc(N * sizeof(Entity));
    for (int i = 0; i < N; i++) {
        hosted[i].id = i;
        hosted[i].x = i * 1.1;
        hosted[i].y = i * 2.2;
        hosted[i].hp = i % 100 + 1;
    }

    const int shard_len = N / BENCH_POOL_WORKERS;
    const int64_t expected = (int64_t)(N / 100) * 5050;

    { int64_t warm = 0; for (int i = 0; i < N; i++) warm += hosted[i].hp; assert(warm == expected); sink ^= warm; }
    {
        BenchJob run[BENCH_POOL_WORKERS];
        SumJob jobs[BENCH_POOL_WORKERS];
        for (int i = 0; i < BENCH_POOL_WORKERS; i++) {
            jobs[i] = (SumJob){ .base = hosted, .start = i * shard_len, .len = shard_len, .sum = 0 };
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
            for (int i = 0; i < shard_len; i++) acc += hosted[start + i].hp;
        }
        T[r] = now_ns() - t0;
        assert(acc == expected);
        sink ^= acc;
    }
    print_result("Hosted Array shards, sequential", T);

    for (int r = 0; r < RUNS; r++) {
        BenchJob run[BENCH_POOL_WORKERS];
        SumJob jobs[BENCH_POOL_WORKERS];
        double t0 = now_ns();
        for (int i = 0; i < BENCH_POOL_WORKERS; i++) {
            jobs[i] = (SumJob){ .base = hosted, .start = i * shard_len, .len = shard_len, .sum = 0 };
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
    print_result("Hosted Array shards, concurrent (4 workers)", T);

    free(hosted);
}

static void test13(void) {
    section("Test 13 -- Partial-guest repeated scan  [100k hosts, 20% guested, payload-only]");
    double T[RUNS];
    zm_reset();
    size_t osz = sizeof(Entity) + sizeof(ZRef);
    Entity **objs = (Entity**)malloc(N * sizeof(Entity*));
    for (int i = 0; i < N; i++) {
        objs[i] = (Entity*)zm_host(osz);
        objs[i]->hp = i % 100 + 1;
        if (i % 5 == 0) zm_mint_guest(objs[i], osz);
    }
    for (int r = 0; r < RUNS; r++) {
        int64_t acc = 0;
        for (int i = 0; i < N; i++) acc += objs[i]->hp;
        double t0 = now_ns();
        for (int p = 0; p < 8; p++)
            for (int i = 0; i < N; i++) acc += objs[i]->hp;
        T[r] = now_ns() - t0; sink ^= acc;
    }
    print_result("Payload scan (8 passes)", T);
    for (int i = 0; i < N; i++) zm_host_release(objs[i], osz);
    free(objs);
}

static void test14(void) {
    section("Test 14 -- Scan-heavy mixed workload  [10 payload scans : 1 tether resolve pass]");
    double T[RUNS];
    zm_reset();
    size_t osz = sizeof(Entity) + sizeof(ZRef);
    Entity **objs = (Entity**)malloc(N * sizeof(Entity*));
    ZRef *refs = (ZRef*)malloc(N * sizeof(ZRef));
    int nt = 0;
    for (int i = 0; i < N; i++) {
        objs[i] = (Entity*)zm_host(osz);
        objs[i]->hp = i % 100 + 1;
        if (i % 5 == 0) refs[nt++] = zm_mint_guest(objs[i], osz);
    }
    for (int r = 0; r < RUNS; r++) {
        int64_t acc = 0;
        for (int i = 0; i < N; i++) acc += objs[i]->hp;
        double t0 = now_ns();
        for (int u = 0; u < 4; u++) {
            for (int p = 0; p < 10; p++)
                for (int i = 0; i < N; i++) acc += objs[i]->hp;
            uint8_t **dir = zm.dir;
            for (int k = 0; k < nt; k++) {
                uint32_t cs = refs[k];
                uint32_t os = *(uint32_t*)(dir[cs>>ZM_WORDBITS] + ((size_t)(cs&ZM_OFFMASK)<<3));
                acc += ((Entity*)(dir[os>>ZM_WORDBITS] + ((size_t)(os&ZM_OFFMASK)<<3)))->hp;
            }
        }
        T[r] = now_ns() - t0; sink ^= acc;
    }
    print_result("Mixed 10:1 (scan-heavy)", T);
    for (int i = 0; i < N; i++) zm_host_release(objs[i], osz);
    free(objs); free(refs);
}

#define FWD_CHAINS 20000
#define FWD_PASSES 8

static void fwd_build(int depth, ZRef *tethers, ZRef *terminals, size_t osz) {
    for (int c = 0; c < FWD_CHAINS; c++) {
        Entity *cur = (Entity*)zm_host(osz);
        cur->hp = c % 100 + 1;
        ZRef t = zm_mint_guest(cur, osz);
        for (int d = 0; d < depth; d++) {
            Entity *next = (Entity*)zm_host(osz);
            zm_mint_guest(next, osz);
            zm_rehost(cur, next, osz);
            cur = next;
        }
        tethers[c] = t;
        if (terminals) terminals[c] = *zm_backptr(cur, osz);
    }
}

static void fwd_measure(const char *label, int depth, ZRef *tethers, size_t osz) {
    double T[RUNS];
    int64_t expected = 0;
    for (int c = 0; c < FWD_CHAINS; c++) expected += c % 100 + 1;

    zm_reset();
    fwd_build(depth, tethers, NULL, osz);
    { int64_t w = 0; for (int c = 0; c < FWD_CHAINS; c++) w += ((Entity*)zm_deref(tethers[c]))->hp; assert(w == expected); sink ^= w; }

    for (int r = 0; r < RUNS; r++) {
        int64_t acc = 0;
        for (int c = 0; c < FWD_CHAINS; c++) acc += ((Entity*)zm_deref(tethers[c]))->hp;
        double t0 = now_ns();
        for (int p = 0; p < FWD_PASSES; p++)
            for (int c = 0; c < FWD_CHAINS; c++) acc += ((Entity*)zm_deref(tethers[c]))->hp;
        T[r] = now_ns() - t0; sink ^= acc;
    }
    print_result(label, T);
    zm_scope_drain();
}

static void test15(void) {
    section("Test 15 -- Guest resolution across forwarding anchors  [20k guests x 8 passes]");
    size_t osz = sizeof(Entity) + sizeof(ZRef);
    ZRef *tethers = (ZRef*)malloc(FWD_CHAINS * sizeof(ZRef));
    ZRef *scratch = (ZRef*)malloc(FWD_CHAINS * sizeof(ZRef));
    ZRef *terminals = (ZRef*)malloc(FWD_CHAINS * sizeof(ZRef));
    double T[RUNS];

    fwd_measure("Terminal anchor (0 hops)", 0, tethers, osz);
    fwd_measure("1 forwarding hop",         1, tethers, osz);
    fwd_measure("2 forwarding hops",        2, tethers, osz);
    fwd_measure("4 forwarding hops",        4, tethers, osz);

    zm_reset();
    {
        uint64_t a0 = zm.anchors_made, f0 = zm.forwarders_made;
        fwd_build(4, tethers, terminals, osz);
        assert(zm.anchors_made - a0 == (uint64_t)FWD_CHAINS * 5);
        assert(zm.forwarders_made - f0 == (uint64_t)FWD_CHAINS * 4);
        assert(zm.retire_top == FWD_CHAINS * 4);
    }
    memcpy(scratch, tethers, FWD_CHAINS * sizeof(ZRef));
    for (int c = 0; c < FWD_CHAINS; c++) {
        void *via_chain = zm_deref(tethers[c]);
        void *via_comp  = zm_deref_compress(&scratch[c]);
        assert(via_chain == via_comp);
        assert(((ZAnchor*)zm_resolve(scratch[c]))->kind == ZA_PAYLOAD);
        assert(scratch[c] == terminals[c]);
    }
    for (int r = 0; r < RUNS; r++) {
        memcpy(scratch, tethers, FWD_CHAINS * sizeof(ZRef));
        int64_t acc = 0;
        double t0 = now_ns();
        for (int p = 0; p < FWD_PASSES; p++)
            for (int c = 0; c < FWD_CHAINS; c++) acc += ((Entity*)zm_deref_compress(&scratch[c]))->hp;
        T[r] = now_ns() - t0; sink ^= acc;
    }
    print_result("4 hops, compressing as it goes", T);

    for (int r = 0; r < RUNS; r++) {
        int64_t acc = 0;
        for (int c = 0; c < FWD_CHAINS; c++) acc += ((Entity*)zm_deref(terminals[c]))->hp;
        double t0 = now_ns();
        for (int p = 0; p < FWD_PASSES; p++)
            for (int c = 0; c < FWD_CHAINS; c++) acc += ((Entity*)zm_deref(terminals[c]))->hp;
        T[r] = now_ns() - t0; sink ^= acc;
    }
    print_result("Terminal anchor, same footprint", T);

    {
        ZRef before = zm.anchor_free;
        int retired = zm.retire_top;
        zm_scope_drain();
        assert(retired > 0 && zm.retire_top == 0 && zm.anchor_free != before);
        sink ^= (int64_t)retired;
    }

    zm_reset();
    {
        Entity *slot = (Entity*)zm_host(osz);
        slot->hp = 7;
        ZRef guest = zm_mint_guest(slot, osz);
        assert(((Entity*)zm_deref(guest))->hp == 7);
        slot->hp = 9;
        zm_overwrite(slot, osz);
        assert(*zm_backptr(slot, osz) == guest);
        assert(((ZAnchor*)zm_resolve(guest))->kind == ZA_PAYLOAD);
        assert(((Entity*)zm_deref(guest))->hp == 9);
        ZRef freed = zm.anchor_free;
        zm_host_destroy(slot, osz);
        assert(zm.anchor_free == guest && zm.anchor_free != freed);
        sink ^= (int64_t)guest;
    }

    free(tethers); free(scratch); free(terminals);
}

#define REUSE_BLOCKS 2000
#define REUSE_ROUNDS 10
static const size_t REUSE_SIZES[3] = { 128, 256, 512 };

static void test16(void) {
    section("Test 16 -- Dynamic-region block churn  [10 rounds x 2k blocks x 128/256/512B]");
    double T[RUNS];
    void **blocks = (void**)malloc(REUSE_BLOCKS * sizeof(void*));

    for (int r = 0; r < RUNS; r++) {
        zm_reset();
        double t0 = now_ns();
        for (int round = 0; round < REUSE_ROUNDS; round++)
            for (int s = 0; s < 3; s++) {
                for (int i = 0; i < REUSE_BLOCKS; i++) blocks[i] = zd_alloc(REUSE_SIZES[s], ZM_LINE);
                for (int i = 0; i < REUSE_BLOCKS; i++) zd_free(blocks[i], REUSE_SIZES[s], ZM_LINE);
            }
        T[r] = now_ns() - t0; sink ^= (int64_t)(uintptr_t)blocks[0];
    }
    print_result("Zane dynamic region (exact-size stacks)", T);

    for (int r = 0; r < RUNS; r++) {
        zm_reset();
        double t0 = now_ns();
        for (int round = 0; round < REUSE_ROUNDS; round++)
            for (int s = 0; s < 3; s++) {
                for (int i = 0; i < REUSE_BLOCKS; i++) blocks[i] = zbump(&zm.dyn, REUSE_SIZES[s], ZM_LINE);
                for (int i = 0; i < REUSE_BLOCKS; i++) zm_fixed_release(blocks[i], REUSE_SIZES[s]);
            }
        T[r] = now_ns() - t0; sink ^= (int64_t)(uintptr_t)blocks[0];
    }
    print_result("Frontier bump only (no reuse)", T);

    for (int r = 0; r < RUNS; r++) {
        double t0 = now_ns();
        for (int round = 0; round < REUSE_ROUNDS; round++)
            for (int s = 0; s < 3; s++) {
                for (int i = 0; i < REUSE_BLOCKS; i++) blocks[i] = malloc(REUSE_SIZES[s]);
                for (int i = 0; i < REUSE_BLOCKS; i++) free(blocks[i]);
            }
        T[r] = now_ns() - t0; sink ^= (int64_t)(uintptr_t)blocks[0];
    }
    print_result("malloc / free", T);

    pool_flush();
    for (int s = 0; s < 3; s++) pool_warm(REUSE_SIZES[s], REUSE_BLOCKS);
    for (int r = 0; r < RUNS; r++) {
        double t0 = now_ns();
        for (int round = 0; round < REUSE_ROUNDS; round++)
            for (int s = 0; s < 3; s++) {
                for (int i = 0; i < REUSE_BLOCKS; i++) blocks[i] = pool_alloc(REUSE_SIZES[s]);
                for (int i = 0; i < REUSE_BLOCKS; i++) pool_free(blocks[i], REUSE_SIZES[s]);
            }
        T[r] = now_ns() - t0; sink ^= (int64_t)(uintptr_t)blocks[0];
    }
    print_result("Pool (per-size free-list)", T);

    zm_reset();
    {
        void *a = zd_alloc(256, ZM_LINE);
        zd_free(a, 256, ZM_LINE);
        void *same  = zd_alloc(256, ZM_LINE);
        assert(same == a);
        zd_free(same, 256, ZM_LINE);
        void *other = zd_alloc(256, ZM_ALIGN);
        assert(other != a);
        void *bigger = zd_alloc(512, ZM_LINE);
        assert(bigger != a);
        sink ^= (int64_t)(uintptr_t)other ^ (int64_t)(uintptr_t)bigger;
    }

    free(blocks);
}

#define BX_DEPTH 12

typedef struct { int64_t value; uint32_t left, right; uint32_t _pad; ZRef _bp; } HNode;
_Static_assert(sizeof(HNode) == 24, "HNode must be 24 bytes");
typedef struct { int64_t value; uint32_t left, right; } VNode;
_Static_assert(sizeof(VNode) == 16, "VNode must be 16 bytes");
typedef struct MNode { int64_t value; struct MNode *left, *right; } MNode;

static uint32_t bh_build(int depth) {
    HNode *n = (HNode*)zd_alloc(sizeof(HNode), 8);
    uint32_t off = zm_seg(n);
    n->value = depth;
    *zm_backptr(n, sizeof(HNode)) = 0;
    n->left  = depth > 0 ? bh_build(depth - 1) : 0;
    n->right = depth > 0 ? bh_build(depth - 1) : 0;
    return off;
}
static void bh_guest_all(uint32_t off) {
    if (!off) return;
    HNode *n = (HNode*)zm_resolve(off);
    uint32_t l = n->left, r = n->right;
    zm_mint_guest(n, sizeof(HNode));
    bh_guest_all(l);
    bh_guest_all(r);
}
static uint32_t bh_relocate(uint32_t off) {
    if (!off) return 0;
    HNode *src = (HNode*)zm_resolve(off);
    HNode *dst = (HNode*)zd_alloc(sizeof(HNode), 8);
    *zm_backptr(dst, sizeof(HNode)) = 0;
    zm_rehost(src, dst, sizeof(HNode));
    dst->left  = bh_relocate(dst->left);
    dst->right = bh_relocate(dst->right);
    zd_free(src, sizeof(HNode), 8);
    return zm_seg(dst);
}
static int64_t bh_sum(uint32_t off) {
    if (!off) return 0;
    HNode *n = (HNode*)zm_resolve(off);
    return n->value + bh_sum(n->left) + bh_sum(n->right);
}

static uint32_t bv_build(int depth) {
    VNode *n = (VNode*)zd_alloc(sizeof(VNode), 8);
    uint32_t off = zm_seg(n);
    n->value = depth;
    n->left  = depth > 0 ? bv_build(depth - 1) : 0;
    n->right = depth > 0 ? bv_build(depth - 1) : 0;
    return off;
}
static uint32_t bv_deepcopy(uint32_t off) {
    if (!off) return 0;
    VNode *src = (VNode*)zm_resolve(off);
    VNode *dst = (VNode*)zd_alloc(sizeof(VNode), 8);
    dst->value = src->value;
    dst->left  = bv_deepcopy(src->left);
    dst->right = bv_deepcopy(src->right);
    return zm_seg(dst);
}
static int64_t bv_sum(uint32_t off) {
    if (!off) return 0;
    VNode *n = (VNode*)zm_resolve(off);
    return n->value + bv_sum(n->left) + bv_sum(n->right);
}

static MNode *bm_build(int depth) {
    MNode *n = (MNode*)malloc(sizeof(MNode));
    n->value = depth;
    n->left  = depth > 0 ? bm_build(depth - 1) : NULL;
    n->right = depth > 0 ? bm_build(depth - 1) : NULL;
    return n;
}
static MNode *bm_deepcopy(const MNode *src) {
    if (!src) return NULL;
    MNode *dst = (MNode*)malloc(sizeof(MNode));
    dst->value = src->value;
    dst->left  = bm_deepcopy(src->left);
    dst->right = bm_deepcopy(src->right);
    return dst;
}
static void bm_destroy(MNode *n) {
    if (!n) return;
    bm_destroy(n->left); bm_destroy(n->right); free(n);
}

static void test17(void) {
    section("Test 17 -- Boxed members: rehost relocation vs deep value copy  [8191 nodes]");
    double T[RUNS];
    int64_t expected;

    zm_reset();
    { uint32_t root = bv_build(BX_DEPTH); expected = bv_sum(root); }

    for (int r = 0; r < RUNS; r++) {
        zm_reset();
        uint32_t root = bh_build(BX_DEPTH);
        double t0 = now_ns();
        root = bh_relocate(root);
        T[r] = now_ns() - t0;
        assert(bh_sum(root) == expected); sink ^= root;
    }
    print_result("Rehost hosted tree, no guests", T);

    for (int r = 0; r < RUNS; r++) {
        zm_reset();
        uint32_t root = bh_build(BX_DEPTH);
        bh_guest_all(root);
        double t0 = now_ns();
        root = bh_relocate(root);
        T[r] = now_ns() - t0;
        assert(bh_sum(root) == expected); sink ^= root;
    }
    print_result("Rehost hosted tree, every node guested", T);

    for (int r = 0; r < RUNS; r++) {
        zm_reset();
        uint32_t root = bv_build(BX_DEPTH);
        double t0 = now_ns();
        uint32_t copy = bv_deepcopy(root);
        T[r] = now_ns() - t0;
        assert(bv_sum(copy) == expected); sink ^= copy;
    }
    print_result("Deep-copy value tree (place source)", T);

    for (int r = 0; r < RUNS; r++) {
        zm_reset();
        double t0 = now_ns();
        uint32_t root = bv_build(BX_DEPTH);
        T[r] = now_ns() - t0;
        assert(bv_sum(root) == expected); sink ^= root;
    }
    print_result("Construct fresh value tree in place", T);

    for (int r = 0; r < RUNS; r++) {
        MNode *root = bm_build(BX_DEPTH);
        double t0 = now_ns();
        MNode *copy = bm_deepcopy(root);
        T[r] = now_ns() - t0;
        sink ^= (int64_t)(uintptr_t)copy;
        bm_destroy(copy); bm_destroy(root);
    }
    print_result("malloc deep copy", T);
}

int main(void) {
    printf("\n");
    printf("  +===================================================================================================+\n");
    printf("  |  Zane Memory Model Benchmark                                                                      |\n");
    printf("  |  N = %d  .  %d runs each  .  MEDIAN reported (ns, 2 d.p.)                                  |\n", N, RUNS);
    printf("  +===================================================================================================+\n");

    zm_init(); ar_init(); pool_flush(); bench_pool_init(); bench_pool_warm();

    test1(); test2(); test3(); test4(); test5();
    test6(); test7(); test8(); test9(); test10(); test11(); test12();
    test13(); test14(); test15(); test16(); test17();

    bench_pool_shutdown();
    printf("\n  (sink = %lld)\n\n", (long long)sink);
    return 0;
}
