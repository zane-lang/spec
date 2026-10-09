

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

#define BENCH_MAX_TESTS  32
#define BENCH_MAX_ROWS   8
#define BENCH_LABEL_MAX  96
#define BENCH_TITLE_MAX  160

typedef struct {
    char   label[BENCH_LABEL_MAX];
    double samples[RUNS];
    int    nsamples;
    int    eliminated;
} BenchRow;

typedef struct {
    char     id[16];
    char     title[BENCH_TITLE_MAX];
    BenchRow rows[BENCH_MAX_ROWS];
    int      nrows;
} BenchTest;

static BenchTest bench_tests[BENCH_MAX_TESTS];
static int       bench_ntests = 0;

static void record_test(const char *id, const char *title) {
    assert(bench_ntests < BENCH_MAX_TESTS);
    BenchTest *t = &bench_tests[bench_ntests++];
    snprintf(t->id, sizeof(t->id), "%s", id);
    snprintf(t->title, sizeof(t->title), "%s", title);
    t->nrows = 0;
}

static BenchRow *bench_open_row(const char *label) {
    assert(bench_ntests > 0);
    BenchTest *t = &bench_tests[bench_ntests - 1];
    assert(t->nrows < BENCH_MAX_ROWS);
    BenchRow *row = &t->rows[t->nrows++];
    snprintf(row->label, sizeof(row->label), "%s", label);
    row->nsamples = 0;
    row->eliminated = 0;
    return row;
}

static void record_row(const char *label, double T[RUNS]) {
    BenchRow *row = bench_open_row(label);
    for (int i = 0; i < RUNS; i++) row->samples[i] = T[i];
    row->nsamples = RUNS;
}

static void record_row_eliminated(const char *label) {
    bench_open_row(label)->eliminated = 1;
}

static void json_string(FILE *f, const char *str) {
    fputc('"', f);
    for (const unsigned char *c = (const unsigned char *)str; *c; c++) {
        if (*c == '"' || *c == '\\') fprintf(f, "\\%c", *c);
        else if (*c < 0x20)          fprintf(f, "\\u%04x", *c);
        else                         fputc(*c, f);
    }
    fputc('"', f);
}

#define WORKER_COUNT   4
#define WORKER_MAX_JOBS  32

typedef void (*WorkerJobFn)(void *);

typedef struct {
    WorkerJobFn fn;
    void      *arg;
} WorkerJob;

typedef struct {
    WorkerJob jobs[WORKER_MAX_JOBS];
    int      head;
    int      tail;
} WorkerQueue;

typedef struct {
    pthread_t       threads[WORKER_COUNT];
    WorkerQueue      queues[WORKER_COUNT];
    pthread_mutex_t mu;
    pthread_cond_t  has_work;
    pthread_cond_t  idle;
    int             pending_jobs;
    int             stop;
} WorkerPool;

static WorkerPool workers;

static void workers_queue_reset(WorkerQueue *q) { q->head = 0; q->tail = 0; }

static void workers_queue_push(WorkerQueue *q, WorkerJob job) {
    assert((q->tail - q->head + 1) <= WORKER_MAX_JOBS);
    q->jobs[q->tail % WORKER_MAX_JOBS] = job;
    q->tail++;
}

static int workers_queue_pop_back(WorkerQueue *q, WorkerJob *job) {
    if (q->tail == q->head) return 0;
    q->tail--;
    *job = q->jobs[q->tail % WORKER_MAX_JOBS];
    return 1;
}

static int workers_queue_pop_front(WorkerQueue *q, WorkerJob *job) {
    if (q->tail == q->head) return 0;
    *job = q->jobs[q->head % WORKER_MAX_JOBS];
    q->head++;
    return 1;
}

static int workers_take_job(int worker_id, WorkerJob *job) {
    if (workers_queue_pop_back(&workers.queues[worker_id], job)) return 1;
    for (int step = 1; step < WORKER_COUNT; step++) {
        int victim = (worker_id + step) % WORKER_COUNT;
        if (workers_queue_pop_front(&workers.queues[victim], job)) return 1;
    }
    return 0;
}

static void *workers_thread(void *arg) {
    int worker_id = (int)(intptr_t)arg;
    WorkerJob job;

    pthread_mutex_lock(&workers.mu);
    for (;;) {
        while (!workers.stop && !workers_take_job(worker_id, &job)) {
            pthread_cond_wait(&workers.has_work, &workers.mu);
        }
        if (workers.stop) {
            pthread_mutex_unlock(&workers.mu);
            return NULL;
        }

        pthread_mutex_unlock(&workers.mu);
        job.fn(job.arg);
        pthread_mutex_lock(&workers.mu);

        workers.pending_jobs--;
        if (workers.pending_jobs == 0) pthread_cond_signal(&workers.idle);
        pthread_cond_broadcast(&workers.has_work);
    }
}

static void workers_init(void) {
    memset(&workers, 0, sizeof(workers));
    assert(pthread_mutex_init(&workers.mu, NULL) == 0);
    assert(pthread_cond_init(&workers.has_work, NULL) == 0);
    assert(pthread_cond_init(&workers.idle, NULL) == 0);
    for (int i = 0; i < WORKER_COUNT; i++) {
        workers_queue_reset(&workers.queues[i]);
        assert(pthread_create(&workers.threads[i], NULL, workers_thread, (void*)(intptr_t)i) == 0);
    }
}

static void workers_run(WorkerJob *jobs, int njobs) {
    assert(njobs > 0 && njobs <= WORKER_MAX_JOBS);

    pthread_mutex_lock(&workers.mu);
    while (workers.pending_jobs != 0) pthread_cond_wait(&workers.idle, &workers.mu);

    for (int i = 0; i < WORKER_COUNT; i++) workers_queue_reset(&workers.queues[i]);
    for (int i = 0; i < njobs; i++) workers_queue_push(&workers.queues[i % WORKER_COUNT], jobs[i]);
    workers.pending_jobs = njobs;

    pthread_cond_broadcast(&workers.has_work);
    while (workers.pending_jobs != 0) pthread_cond_wait(&workers.idle, &workers.mu);
    pthread_mutex_unlock(&workers.mu);
}

static void workers_noop_job(void *arg) { (void)arg; }

static void workers_warm(void) {
    WorkerJob jobs[WORKER_COUNT];
    for (int i = 0; i < WORKER_COUNT; i++) jobs[i] = (WorkerJob){ .fn = workers_noop_job, .arg = NULL };
    workers_run(jobs, WORKER_COUNT);
}

static void workers_shutdown(void) {
    pthread_mutex_lock(&workers.mu);
    workers.stop = 1;
    pthread_cond_broadcast(&workers.has_work);
    pthread_mutex_unlock(&workers.mu);

    for (int i = 0; i < WORKER_COUNT; i++) assert(pthread_join(workers.threads[i], NULL) == 0);
    pthread_cond_destroy(&workers.idle);
    pthread_cond_destroy(&workers.has_work);
    pthread_mutex_destroy(&workers.mu);
}

#define ZM_ALIGN      8
#define ZM_MAXSZ      512
#define ZM_NC         (ZM_MAXSZ / ZM_ALIGN)
#define ZM_CHUNKBITS  20
#define ZM_CHUNK      (1UL << ZM_CHUNKBITS)
#define ZM_NCHUNKS    (REGION_SIZE / ZM_CHUNK)
#define ZM_LINE       64
#define ZM_LIST_MIN   128

#define ZD_NIL        NULL
#define ZD_STACKS     1024

typedef void *ZRef;

typedef struct { size_t off; int live; uint8_t *cbase; } ZBump;
typedef struct {
    size_t size, align;
    void *head;
    void **small;
    size_t small_count, small_capacity;
} ZSizeStack;

static struct {
    uint8_t   *base;
    size_t     next_chunk;
    uint8_t   *fixed_base;
    size_t     fixed_off;
    ZBump      dyn;
    ZSizeStack stacks[ZD_STACKS];
} zm;

static uint8_t *zm_take_chunk(void) {
    assert(zm.next_chunk < ZM_NCHUNKS);
    return zm.base + zm.next_chunk++ * ZM_CHUNK;
}

static void *zm_bump(ZBump *b, size_t size, size_t align) {
    assert(size <= ZM_CHUNK);
    if (!b->live) { b->cbase = zm_take_chunk(); b->off = 0; b->live = 1; }
    size_t off = (b->off + (align - 1)) & ~(align - 1);
    if (off + size > ZM_CHUNK) {
        b->cbase = zm_take_chunk();
        off = 0;
    }
    void *p = b->cbase + off;
    b->off = off + size;
    return p;
}

static inline void *zm_fixed_alloc(size_t s) {
    s = (s + ZM_ALIGN - 1) & ~(size_t)(ZM_ALIGN - 1);
    size_t off = zm.fixed_off;
    assert(s <= REGION_SIZE - off);
    zm.fixed_off = off + s;
    return zm.fixed_base + off;
}
static void zm_fixed_release(void *p, size_t s) { (void)p; (void)s; }
static void zd_release(void *p, size_t s) { (void)p; (void)s; }

static ZSizeStack *zd_stack(size_t size, size_t align) {
    uint32_t h = ((uint32_t)size * 0x9E3779B1u) ^ ((uint32_t)align * 0x85EBCA6Bu);
    uint32_t i = h & (ZD_STACKS - 1);
    for (uint32_t probe = 0; probe < ZD_STACKS; probe++) {
        ZSizeStack *s = &zm.stacks[(i + probe) & (ZD_STACKS - 1)];
        if (s->align == 0) {
            s->size = size; s->align = align; s->head = ZD_NIL;
            return s;
        }
        if (s->size == size && s->align == align) return s;
    }
    assert(0);
    return NULL;
}

static void *zd_span(size_t bytes) {
    size_t n = (bytes + ZM_CHUNK - 1) / ZM_CHUNK;
    uint8_t *first = zm_take_chunk();
    for (size_t i = 1; i < n; i++) (void)zm_take_chunk();
    return first;
}

static void *zd_pop(ZSizeStack *s) {
    if (s->size < sizeof(void *))
        return s->small_count ? s->small[--s->small_count] : NULL;
    if (s->head == ZD_NIL) return NULL;
    void *p = s->head;
    memcpy(&s->head, p, sizeof s->head);
    return p;
}

static void *zd_try_stack(size_t size, size_t align) {
    return zd_pop(zd_stack(size, align));
}

static void *zd_take(ZSizeStack *s) {
    void *p = zd_pop(s);
    if (p) return p;
    if (s->size > ZM_CHUNK) return zd_span(s->size);
    return zm_bump(&zm.dyn, s->size, s->align);
}
static void zd_give(ZSizeStack *s, void *p) {
    if (s->size < sizeof(void *)) {
        if (s->small_count == s->small_capacity) {
            size_t capacity = s->small_capacity ? s->small_capacity * 2 : 16;
            assert(capacity > s->small_capacity && capacity <= SIZE_MAX / sizeof *s->small);
            void **entries = realloc(s->small, capacity * sizeof *entries);
            assert(entries);
            s->small = entries;
            s->small_capacity = capacity;
        }
        s->small[s->small_count++] = p;
        return;
    }
    memcpy(p, &s->head, sizeof s->head);
    s->head = p;
}

static void *zd_alloc(size_t size, size_t align) {
    void *p = zd_try_stack(size, align);
    if (p) return p;
    if (size > ZM_CHUNK) return zd_span(size);
    return zm_bump(&zm.dyn, size, align);
}
static void zd_free(void *p, size_t size, size_t align) {
    zd_give(zd_stack(size, align), p);
}

static int zd_grow_in_place(void *block, size_t old_bytes, size_t new_bytes) {
    if (new_bytes > ZM_CHUNK) return 0;
    if (!zm.dyn.live) return 0;
    if ((uint8_t*)block + old_bytes != zm.dyn.cbase + zm.dyn.off) return 0;
    size_t extra = new_bytes - old_bytes;
    if (zm.dyn.off + extra > ZM_CHUNK) return 0;
    zm.dyn.off += extra;
    return 1;
}

static inline void *zm_own(size_t obj_size) { return zm_fixed_alloc(obj_size); }
static void zm_own_release(void *obj, size_t obj_size) { (void)obj; (void)obj_size; }

static inline ZRef zm_mint_ref(void *obj) { return obj; }
static inline void *zm_deref(ZRef r) { return r; }

static void zm_reset(void) {
    zm.next_chunk = 0;
    zm.dyn.live = 0;
    zm.fixed_off = zm.dyn.off = 0;
    for (int i = 0; i < ZD_STACKS; i++) {
        zm.stacks[i].head = ZD_NIL;
        zm.stacks[i].small_count = 0;
    }
}

static void zm_init(void) {
    zm.base = mmap(NULL, REGION_SIZE, PROT_READ | PROT_WRITE,
                   MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    assert(zm.base != MAP_FAILED);
    for (size_t i = 0; i < REGION_SIZE; i += 4096) zm.base[i] = 0;
    zm.fixed_base = mmap(NULL, REGION_SIZE, PROT_READ | PROT_WRITE,
                         MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    assert(zm.fixed_base != MAP_FAILED);
    for (size_t i = 0; i < REGION_SIZE; i += 4096) zm.fixed_base[i] = 0;
    zm_reset();
}

static struct { uint8_t *base; size_t top; } ar;

static void ar_init(void) {
    ar.base = mmap(NULL, REGION_SIZE, PROT_READ | PROT_WRITE,
                   MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    assert(ar.base != MAP_FAILED);
    for (size_t i = 0; i < REGION_SIZE; i += 4096) ar.base[i] = 0;
    ar.top = 0;
}
static inline void   ar_reset(void)       { ar.top = 0; }
static inline void  *ar_alloc(size_t s)   { s=(s+7)&~(size_t)7; void *p=ar.base+ar.top; ar.top+=s; return p; }

static inline size_t pool_round(size_t s) { return (s + ZM_ALIGN-1) & ~(size_t)(ZM_ALIGN-1); }
static inline int    pool_class(size_t s)   { return (int)(s / ZM_ALIGN) - 1; }

typedef struct PNode { struct PNode *next; } PNode;
static PNode *pool_heads[ZM_NC];

static void pool_flush(void) {
    for (int c = 0; c < ZM_NC; c++) {
        for (PNode *n = pool_heads[c], *nx; n; n = nx) { nx = n->next; free(n); }
        pool_heads[c] = NULL;
    }
}
static void *pool_alloc(size_t s) {
    s=pool_round(s); int c=pool_class(s);
    if (pool_heads[c]) { void *p=pool_heads[c]; pool_heads[c]=pool_heads[c]->next; return p; }
    return malloc(s);
}
static void pool_free(void *p, size_t s) {
    s=pool_round(s); int c=pool_class(s);
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
    record_test("Test 1", "Sequential alloc + sequential free  [32 bytes x 100k]");
    double T[RUNS];
    void **ptrs = (void**)malloc(N * sizeof(void *));

    for (int r=0;r<RUNS;r++) { zm_reset(); double t0=now_ns(); for(int i=0;i<N;i++) ptrs[i]=zm_own(32); for(int i=0;i<N;i++) zm_own_release(ptrs[i],32); T[r]=now_ns()-t0; }
    record_row("Per-object fixed bump (baseline)", T);

    for (int r = 0; r < RUNS; r++) {
        zm_reset();
        double t0 = now_ns();
        for (int i = 0; i < N; i += 4) {
            uint8_t *frame = zm_fixed_alloc(4 * 32);
            ptrs[i] = frame;
            ptrs[i + 1] = frame + 32;
            ptrs[i + 2] = frame + 64;
            ptrs[i + 3] = frame + 96;
        }
        for (int i = 0; i < N; i++) zm_own_release(ptrs[i], 32);
        sink ^= (int64_t)(uintptr_t)ptrs[N - 1];
        size_t end = zm.fixed_off;
        zm.fixed_off = 0;
        T[r] = now_ns() - t0;
        assert(end == N * 32 && zm.fixed_off == 0);
        for (int i = 0; i < N; i++) assert(ptrs[i] == zm.fixed_base + i * 32);
    }
    record_row("Zane (four-owner scope frames)", T);

    for (int r=0;r<RUNS;r++) { double t0=now_ns(); for(int i=0;i<N;i++) ptrs[i]=malloc(32); for(int i=0;i<N;i++) free(ptrs[i]); T[r]=now_ns()-t0; }
    record_row("malloc / free", T);

    for (int r=0;r<RUNS;r++) { ar_reset(); double t0=now_ns(); for(int i=0;i<N;i++) ptrs[i]=ar_alloc(32); sink^=(int64_t)(uintptr_t)ptrs[N-1]; ar_reset(); T[r]=now_ns()-t0; }
    record_row("Arena (bump + O(1) reset)", T);

    pool_flush(); pool_warm(32,N);
    for (int r=0;r<RUNS;r++) { double t0=now_ns(); for(int i=0;i<N;i++) ptrs[i]=pool_alloc(32); for(int i=0;i<N;i++) pool_free(ptrs[i],32); T[r]=now_ns()-t0; }
    record_row("Pool (per-size free-list)", T);

    free(ptrs);
}

static void test2(void) {
    record_test("Test 2", "Random-order free only  [32B x 100k  |  alloc+shuffle NOT timed]");
    double T[RUNS];
    void **ptrs = (void**)malloc(N * sizeof(void *));

    record_row_eliminated("Zane (fixed-region bump)");

    for(int r=0;r<RUNS;r++){rng_state=0xfeed0000ULL+(uint64_t)r;for(int i=0;i<N;i++)ptrs[i]=malloc(32);shuf_ptrs(ptrs,N);double t0=now_ns();for(int i=0;i<N;i++)free(ptrs[i]);T[r]=now_ns()-t0;}
    record_row("malloc / free", T);

    pool_flush();pool_warm(32,N);
    for(int r=0;r<RUNS;r++){rng_state=0xfeed0000ULL+(uint64_t)r;for(int i=0;i<N;i++)ptrs[i]=pool_alloc(32);shuf_ptrs(ptrs,N);double t0=now_ns();for(int i=0;i<N;i++)pool_free(ptrs[i],32);T[r]=now_ns()-t0;}
    record_row("Pool (per-size free-list)", T);

    free(ptrs);
}

static const size_t MIXED_SIZES[] = { 8, 16, 32, 64 };
#define NMS 4

static void test3(void) {
    record_test("Test 3", "Mixed sizes (8/16/32/64B) alloc + random-order free  [100k total]");
    double T[RUNS];
    PS    *pairs = (PS*)malloc(N * sizeof(PS));
    size_t *szseq = (size_t*)malloc(N * sizeof(size_t));
    for(int i=0;i<N;i++) szseq[i]=MIXED_SIZES[i%NMS];

    for(int r=0;r<RUNS;r++){zm_reset();rng_state=0xbabe0000ULL+(uint64_t)r;double t0=now_ns();for(int i=0;i<N;i++){pairs[i].p=zm_fixed_alloc(szseq[i]);pairs[i].s=szseq[i];}shuf_ps(pairs,N);for(int i=0;i<N;i++)zm_fixed_release(pairs[i].p,pairs[i].s);T[r]=now_ns()-t0;}
    record_row("Per-object fixed bump (baseline)", T);

    for (int r = 0; r < RUNS; r++) {
        zm_reset();
        rng_state = 0xbabe0000ULL + (uint64_t)r;
        double t0 = now_ns();
        for (int i = 0; i < N; i += 4) {
            uint8_t *frame = zm_fixed_alloc(128);
            pairs[i] = (PS){frame, 8};
            pairs[i + 1] = (PS){frame + 8, 16};
            pairs[i + 2] = (PS){frame + 24, 32};
            pairs[i + 3] = (PS){frame + 56, 64};
        }
        shuf_ps(pairs, N);
        for (int i = 0; i < N; i++) zm_fixed_release(pairs[i].p, pairs[i].s);
        sink ^= (int64_t)(uintptr_t)pairs[N - 1].p;
        size_t end = zm.fixed_off;
        zm.fixed_off = 0;
        T[r] = now_ns() - t0;
        assert(end == (N / 4) * 128 && zm.fixed_off == 0);
        for (int i = 0; i < N; i++) {
            size_t offset = (uint8_t *)pairs[i].p - zm.fixed_base;
            size_t slot = offset % 128;
            assert(offset < end);
            assert((slot == 0 && pairs[i].s == 8) || (slot == 8 && pairs[i].s == 16) ||
                   (slot == 24 && pairs[i].s == 32) || (slot == 56 && pairs[i].s == 64));
        }
    }
    record_row("Zane (four-owner scope frames)", T);

    for(int r=0;r<RUNS;r++){rng_state=0xbabe0000ULL+(uint64_t)r;double t0=now_ns();for(int i=0;i<N;i++){pairs[i].p=malloc(szseq[i]);pairs[i].s=szseq[i];}shuf_ps(pairs,N);for(int i=0;i<N;i++)free(pairs[i].p);T[r]=now_ns()-t0;}
    record_row("malloc / free", T);

    {void **ap=(void**)malloc(N*sizeof(void*));for(int r=0;r<RUNS;r++){ar_reset();double t0=now_ns();for(int i=0;i<N;i++)ap[i]=ar_alloc(szseq[i]);sink^=(int64_t)(uintptr_t)ap[N-1];ar_reset();T[r]=now_ns()-t0;}record_row("Arena (bulk reset)",T);free(ap);}

    pool_flush();for(int s=0;s<NMS;s++)pool_warm(MIXED_SIZES[s],N/NMS);
    for(int r=0;r<RUNS;r++){rng_state=0xbabe0000ULL+(uint64_t)r;double t0=now_ns();for(int i=0;i<N;i++){pairs[i].p=pool_alloc(szseq[i]);pairs[i].s=szseq[i];}shuf_ps(pairs,N);for(int i=0;i<N;i++)pool_free(pairs[i].p,pairs[i].s);T[r]=now_ns()-t0;}
    record_row("Pool (per-size free-list)", T);

    free(pairs);free(szseq);
}

static void test4(void) {
    record_test("Test 4", "Iteration: inline (owned) vs pointer-chase  [32B Entity x 100k]");
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
    record_row("Inline array  (Array<Entity, 100000>)", T);

    for(int r=0;r<RUNS;r++){int64_t acc=0;double t0=now_ns();for(int i=0;i<N;i++)acc+=sp[i]->hp;T[r]=now_ns()-t0;sink^=acc;}
    record_row("Pointer array, sequential", T);

    for(int r=0;r<RUNS;r++){int64_t acc=0;double t0=now_ns();for(int i=0;i<N;i++)acc+=sh[i]->hp;T[r]=now_ns()-t0;sink^=acc;}
    record_row("Pointer array, shuffled", T);

    for(int r=0;r<RUNS;r++){int64_t acc=0;double t0=now_ns();for(UChunk*c=ul.head;c;c=c->next)for(int j=0;j<c->len;j++)acc+=c->data[j].hp;T[r]=now_ns()-t0;sink^=acc;}
    record_row("UList (chunk=8, linked)",T);

    for(int r=0;r<RUNS;r++){int64_t acc=0;double t0=now_ns();for(int i=0;i<cc.len;i++)acc+=cchunked_get(&cc,i)->hp;T[r]=now_ns()-t0;sink^=acc;}
    record_row("CChunked (chunk=64, ptr-array)",T);

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
                                   : (Entity*)zm_bump(&zm.dyn, want, ZM_LINE);
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
    record_test("Test 5", "List backing-store growth  [push 100k x 32B Entity items]");
    double T[RUNS];
    Entity tmpl={42,1.5,2.5,99,0};

    for(int r=0;r<RUNS;r++){zm_reset();ZList l;zlist_init(&l);double t0=now_ns();for(int i=0;i<N;i++)zlist_push(&l,tmpl);T[r]=now_ns()-t0;sink^=(int64_t)l.len;}
    record_row("Zane List (128B start, doubling)", T);

    for(int r=0;r<RUNS;r++){CVec v={NULL,0,0};double t0=now_ns();for(int i=0;i<N;i++)cvec_push(&v,tmpl);T[r]=now_ns()-t0;sink^=(int64_t)v.len;free(v.base);}
    record_row("C realloc vector", T);

    for(int r=0;r<RUNS;r++){UList ul;ulist_init(&ul);double t0=now_ns();for(int i=0;i<N;i++)ulist_push(&ul,tmpl);T[r]=now_ns()-t0;sink^=(int64_t)ul.total;ulist_free_all(&ul);}
    record_row("UList (chunk=8, no realloc)", T);

    for(int r=0;r<RUNS;r++){CChunked cc;cchunked_init(&cc);double t0=now_ns();for(int i=0;i<N;i++)cchunked_push(&cc,tmpl);T[r]=now_ns()-t0;sink^=(int64_t)cc.len;cchunked_free_all(&cc);}
    record_row("CChunked (chunk=64, ptr-array)", T);
}

static void test6(void) {
    record_test("Test 6", "Reference access via native address vs direct pointer  [100k accesses]");
    double T[RUNS];

    zm_reset();
    Entity **objs   = (Entity**)malloc(N * sizeof(Entity*));
    Entity **direct = (Entity**)malloc(N * sizeof(Entity*));
    ZRef  *refs   = (ZRef*)malloc(N * sizeof(ZRef));

    for(int i=0;i<N;i++){
        objs[i] = (Entity*)zm_own(sizeof(Entity));
        objs[i]->hp = i%100+1;
        refs[i] = zm_mint_ref(objs[i]);
        direct[i] = objs[i];
    }

    for(int r=0;r<RUNS;r++){
        for(int i=0;i<N;i++) sink^=(int64_t)direct[i]->hp;
        int64_t acc=0; double t0=now_ns();
        for(int i=0;i<N;i++) acc+=direct[i]->hp;
        T[r]=now_ns()-t0; sink^=acc;
    }
    record_row("Direct pointer (baseline)", T);

    for(int r=0;r<RUNS;r++){
        for(int i=0;i<N;i++) sink^=(int64_t)((Entity*)zm_deref(refs[i]))->hp;
        int64_t acc=0; double t0=now_ns();
        for(int i=0;i<N;i++) acc+=((Entity*)zm_deref(refs[i]))->hp;
        T[r]=now_ns()-t0; sink^=acc;
    }
    record_row("Native reference", T);

    for(int i=0;i<N;i++) assert(zm_deref(refs[i]) == (void*)direct[i]);
    for(int i=0;i<N;i++) zm_own_release(objs[i], sizeof(Entity));
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

static void *zane_obj_alloc(size_t s){return zm_own(s);}
static void  zane_obj_free (void*p,size_t s){zm_own_release(p,s);}
static void *ma_obj_alloc(size_t s){return malloc(s);}
static void  ma_obj_free (void*p,size_t s){(void)s;free(p);}
static void *po_obj_alloc(size_t s){return pool_alloc(s);}
static void  po_obj_free (void*p,size_t s){pool_free(p,s);}
static void *ar_obj_alloc(size_t s){return ar_alloc(s);}
static void  ar_obj_free (void*p,size_t s){(void)p;(void)s;}

static void game_loop_run(double T[RUNS], AllocFn af, FreeFn ff, int prewarm) {
    if (prewarm) { pool_flush(); pool_warm(sizeof(Entity), MAX_ENTITIES); }
    for (int r=0; r<RUNS; r++) {
        if (af == zane_obj_alloc) zm_reset();
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
    record_test("Test 7", "Game loop  [500 frames: 30 spawns + 20+ kills + update per frame]");
    double T[RUNS];
    zm_reset(); game_loop_run(T, zane_obj_alloc, zane_obj_free, 0); record_row("Zane (fixed-region bump)", T);
             game_loop_run(T, ma_obj_alloc, ma_obj_free, 0); record_row("malloc / free", T);
             game_loop_run(T, po_obj_alloc, po_obj_free, 1); record_row("Pool (per-size free-list)", T);
}

#define PART_FRAMES   500
#define MAX_PARTICLES 6000
#define BURST_SPAWN   60

_Static_assert((size_t)PART_FRAMES * BURST_SPAWN * sizeof(Particle) <= REGION_SIZE,
               "Arena must hold all particles spawned during one run");

typedef struct { Particle **slots; int count,cap; } PPool;
static void pp_init(PPool*p,int cap){p->slots=(Particle**)calloc((size_t)cap,sizeof(Particle*));p->count=0;p->cap=cap;}
static void pp_free(PPool*p){free(p->slots);}
static void pp_add(PPool*p,Particle*e){for(int i=0;i<p->cap;i++)if(!p->slots[i]){p->slots[i]=e;p->count++;return;}}

#define PART_SHARD_CAP ((MAX_PARTICLES + WORKER_COUNT - 1) / WORKER_COUNT)

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
        if (!prewarm && af==zane_obj_alloc) zm_reset();
        if (af==ar_obj_alloc) ar_reset();
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
        if (af==ar_obj_alloc) ar_reset();
        T[r]=now_ns()-t0; pp_free(&pp);
    }
}

static void particle_run_parallel(double T[RUNS], AllocFn af, FreeFn ff, int prewarm) {
    if (prewarm) { pool_flush(); pool_warm(sizeof(Particle), MAX_PARTICLES); }
    for (int r = 0; r < RUNS; r++) {
        if (!prewarm && af == zane_obj_alloc) zm_reset();
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

            WorkerJob jobs[WORKER_COUNT];
            ParticleShardJob shard_jobs[WORKER_COUNT];
            int base = MAX_PARTICLES / WORKER_COUNT;
            int rem = MAX_PARTICLES % WORKER_COUNT;
            int start = 0;
            for (int i = 0; i < WORKER_COUNT; i++) {
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
            workers_run(jobs, WORKER_COUNT);

            double ax = 0.0;
            for (int i = 0; i < WORKER_COUNT; i++) {
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


static void test8(void) {
    record_test("Test 8", "Particle system  [500 frames, 60 spawns/frame, TTL 10-30, update all alive]");
    double T[RUNS];
    zm_reset(); particle_run(T, zane_obj_alloc, zane_obj_free, 0); record_row("Zane (fixed-region bump)", T);
              particle_run_parallel(T, zane_obj_alloc, zane_obj_free, 0); record_row("Zane + work-stealing update", T);
              particle_run(T, ma_obj_alloc, ma_obj_free, 0); record_row("malloc / free", T);
              particle_run(T, po_obj_alloc, po_obj_free, 1); record_row("Pool (per-size free-list)", T);
              particle_run(T, ar_obj_alloc, ar_obj_free, 0); record_row("Arena (bump + end-of-run reset)", T);
}

static void test9(void) {
    record_test("Test 9", "Checkerboard fragmentation + refill  [alloc 100k, free evens, alloc 50k (timed)]");
    double T[RUNS];
    void **ptrs=(void**)malloc(N*sizeof(void*));

    for(int r=0;r<RUNS;r++){
        zm_reset();
        for(int i=0;i<N;i++){ptrs[i]=zm_own(32);((Entity*)ptrs[i])->hp=i;}
        for(int i=0;i<N;i+=2) zm_own_release(ptrs[i],32);
        double t0=now_ns();
        for(int i=0;i<N/2;i++) ptrs[i]=zm_own(32);
        T[r]=now_ns()-t0; sink^=(int64_t)(uintptr_t)ptrs[0];
    }
    record_row("Zane -- refill (fixed-region bump)", T);

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
        record_row("malloc -- refill fragmented heap", T);
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
        record_row("Pool -- refill from free-list", T);
        free(refill);
    }

    free(ptrs);
}

#define TREE_NODES 4000
#define MAX_BRANCH 6

_Static_assert((size_t)TREE_NODES * (sizeof(TNode) + sizeof(TNode*)) <= REGION_SIZE,
               "Arena must hold the tree nodes and child lists");

typedef void*(*ChildAllocFn)(int);
typedef void (*ChildFreeFn)(void*,int);

static ZSizeStack *zane_children_stack = NULL;
static void *zane_children_alloc(int n){ (void)n; return zd_take(zane_children_stack); }
static void  zane_children_free(void*p,int n){ (void)n; zd_give(zane_children_stack, p); }
static void *ma_children_alloc(int n){ return malloc((size_t)n*sizeof(TNode*)); }
static void  ma_children_free(void*p,int n){ (void)n; free(p); }
static void *po_children_alloc(int n){ return pool_alloc((size_t)n*sizeof(TNode*)); }
static void  po_children_free(void*p,int n){ pool_free(p,(size_t)n*sizeof(TNode*)); }
static void *ar_children_alloc(int n){ return ar_alloc((size_t)n*sizeof(TNode*)); }

static TNode *build_tree(int n, AllocFn af, ChildAllocFn caf) {
    if (n <= 0) return NULL;
    TNode *node = (TNode*)af(sizeof(TNode));
    node->value = (int64_t)(rng() % 1000);
    int rest = n - 1;
    if (rest == 0) { node->nchildren = 0; node->children = NULL; return node; }
    int k = 1 + (int)(rng() % (MAX_BRANCH - 1));
    if (k > rest) k = rest;
    node->nchildren = k;
    node->children = (TNode**)caf(k);
    for (int i = 0; i < k; i++) {
        int slots_left = k - i;
        int share = (slots_left == 1) ? rest
                                      : 1 + (int)(rng() % (rest - slots_left + 1));
        rest -= share;
        node->children[i] = build_tree(share, af, caf);
    }
    return node;
}

static int64_t destroy_zane(TNode *n) {
    if(!n) return 0;
    int64_t sum = n->value;
    for(int i=0;i<n->nchildren;i++) sum += destroy_zane(n->children[i]);
    if(n->children) zane_children_free(n->children,n->nchildren);
    zm_own_release(n, sizeof(TNode));
    return sum;
}

static int64_t destroy_malloc(TNode *n) {
    if(!n) return 0;
    int64_t sum = n->value;
    for(int i=0;i<n->nchildren;i++) sum += destroy_malloc(n->children[i]);
    if(n->children) ma_children_free(n->children,n->nchildren);
    free(n);
    return sum;
}

static int64_t destroy_pool(TNode *n) {
    if(!n) return 0;
    int64_t sum = n->value;
    for(int i=0;i<n->nchildren;i++) sum += destroy_pool(n->children[i]);
    if(n->children) po_children_free(n->children,n->nchildren);
    pool_free(n,sizeof(TNode));
    return sum;
}

static int64_t destroy_arena(TNode *n) {
    if(!n) return 0;
    int64_t sum = n->value;
    for(int i=0;i<n->nchildren;i++) sum += destroy_arena(n->children[i]);
    return sum;
}

static int64_t tree_sum(TNode *n, int *count) {
    if(!n) return 0;
    (*count)++;
    int64_t sum = n->value;
    for(int i=0;i<n->nchildren;i++) sum += tree_sum(n->children[i], count);
    return sum;
}

typedef int64_t (*TreeDestroyFn)(TNode*);

static void tree_run(double T[RUNS], AllocFn af, ChildAllocFn caf, TreeDestroyFn df) {
    for (int r=0;r<RUNS;r++) {
        if (af==zane_obj_alloc) zm_reset();
        if (af==ar_obj_alloc) ar_reset();
        rng_state=0xbadf00dULL+(uint64_t)r;
        TNode *root=build_tree(TREE_NODES,af,caf);
        int count=0;
        int64_t expected=tree_sum(root, &count);
        assert(count==TREE_NODES);
        double t0=now_ns();
        int64_t sum=df(root);
        if (af==ar_obj_alloc) ar_reset();
        T[r]=now_ns()-t0;
        assert(sum==expected);
        sink^=sum;
    }
}


static void test10(void) {
    record_test("Test 10", "Ownership tree teardown  [~4000 nodes, cascade post-order destroy]");
    double T[RUNS];
    zane_children_stack = zd_stack(ZM_LIST_MIN, ZM_LINE);

    tree_run(T,zane_obj_alloc,zane_children_alloc,destroy_zane);
    record_row("Zane cascade destroy", T);

    tree_run(T,ma_obj_alloc,ma_children_alloc,destroy_malloc);
    record_row("malloc cascade destroy", T);

    pool_flush();pool_warm(sizeof(TNode),TREE_NODES);
    for(int b=1;b<MAX_BRANCH;b++) pool_warm((size_t)b*sizeof(TNode*),TREE_NODES/MAX_BRANCH);
    tree_run(T,po_obj_alloc,po_children_alloc,destroy_pool);
    record_row("Pool cascade destroy", T);

    tree_run(T,ar_obj_alloc,ar_children_alloc,destroy_arena);
    record_row("Arena cascade visit + bulk reset", T);
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
        if (af == zane_obj_alloc) zm_reset();
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
    record_test("Test 11", "Fragmentation stress  [200 cycles: spawn+list-create+push+update+kill]");
    double T[RUNS];
    zm_reset(); stress_run(T, zane_obj_alloc, zane_obj_free, zane_buf_alloc, zane_buf_free, 0);
    record_row("Zane (fixed bump + size stacks)", T);
               stress_run(T, ma_obj_alloc, ma_obj_free, ma_buf_alloc, ma_buf_free, 0);
    record_row("malloc / free", T);
               stress_run(T, po_obj_alloc, po_obj_free, po_buf_alloc, po_buf_free, 1);
    record_row("Pool (per-size free-list)", T);
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
    record_test("Test 12", "Concurrent shard scan  [4 x Array<Entity, 25000> read-only shard sums]");
    double T[RUNS];
    assert((N % WORKER_COUNT) == 0);

    Entity *owned = (Entity*)malloc(N * sizeof(Entity));
    for (int i = 0; i < N; i++) {
        owned[i].id = i;
        owned[i].x = i * 1.1;
        owned[i].y = i * 2.2;
        owned[i].hp = i % 100 + 1;
    }

    const int shard_len = N / WORKER_COUNT;
    const int64_t expected = (int64_t)(N / 100) * 5050;

    { int64_t warm = 0; for (int i = 0; i < N; i++) warm += owned[i].hp; assert(warm == expected); sink ^= warm; }
    {
        WorkerJob run[WORKER_COUNT];
        SumJob jobs[WORKER_COUNT];
        for (int i = 0; i < WORKER_COUNT; i++) {
            jobs[i] = (SumJob){ .base = owned, .start = i * shard_len, .len = shard_len, .sum = 0 };
            run[i] = (WorkerJob){ .fn = sum_entity_shard, .arg = &jobs[i] };
        }
        workers_run(run, WORKER_COUNT);
        int64_t warm = 0;
        for (int i = 0; i < WORKER_COUNT; i++) {
            warm += jobs[i].sum;
        }
        assert(warm == expected);
        sink ^= warm;
    }

    for (int r = 0; r < RUNS; r++) {
        int64_t acc = 0;
        double t0 = now_ns();
        for (int shard = 0; shard < WORKER_COUNT; shard++) {
            int start = shard * shard_len;
            for (int i = 0; i < shard_len; i++) acc += owned[start + i].hp;
        }
        T[r] = now_ns() - t0;
        assert(acc == expected);
        sink ^= acc;
    }
    record_row("Owned Array shards, sequential", T);

    for (int r = 0; r < RUNS; r++) {
        WorkerJob run[WORKER_COUNT];
        SumJob jobs[WORKER_COUNT];
        double t0 = now_ns();
        for (int i = 0; i < WORKER_COUNT; i++) {
            jobs[i] = (SumJob){ .base = owned, .start = i * shard_len, .len = shard_len, .sum = 0 };
            run[i] = (WorkerJob){ .fn = sum_entity_shard, .arg = &jobs[i] };
        }
        workers_run(run, WORKER_COUNT);
        int64_t acc = 0;
        for (int i = 0; i < WORKER_COUNT; i++) {
            acc += jobs[i].sum;
        }
        T[r] = now_ns() - t0;
        assert(acc == expected);
        sink ^= acc;
    }
    record_row("Owned Array shards, concurrent (4 workers)", T);

    free(owned);
}

#define REUSE_BLOCKS 2000
#define REUSE_ROUNDS 10
static const size_t REUSE_SIZES[3] = { 128, 256, 512 };

static void check_exact_reuse(size_t size, size_t align) {
    zm_reset();
    ZSizeStack *s = zd_stack(size, align);
    uint8_t *a = zd_take(s), *b = zd_alloc(size, align), *live = zd_take(s);
    assert(b == a + size && live == b + size);
    memset(b, 0x6b, size);
    memset(live, 0xa5, size);
    zd_give(s, a);
    for (size_t i = 0; i < size; i++) assert(b[i] == 0x6b && live[i] == 0xa5);
    zd_free(b, size, align);
    for (size_t i = 0; i < size; i++) assert(live[i] == 0xa5);
    assert(zd_try_stack(size, align) == b);
    assert(zd_take(s) == a);
    assert(zd_try_stack(size, align) == NULL);
    zd_free(a, size, align);
    zm_reset();
    assert(zd_try_stack(size, align) == NULL);
}

static void check_many_small_blocks(void) {
    zm_reset();
    ZSizeStack *s = zd_stack(1, 1);
    void *blocks[40];
    for (size_t i = 0; i < 40; i++) blocks[i] = zd_take(s);
    uint8_t *live = zd_alloc(1, 1);
    *live = 0xa5;
    for (size_t i = 0; i < 40; i++) zd_free(blocks[i], 1, 1);
    assert(*live == 0xa5);
    for (size_t i = 40; i > 0; i--) assert(zd_take(s) == blocks[i - 1]);
    assert(zd_try_stack(1, 1) == NULL);
}

static void test13(void) {
    record_test("Test 13", "Dynamic-region block churn  [10 rounds x 2k blocks x 128/256/512B]");
    double T[RUNS];
    void **blocks = (void**)malloc(REUSE_BLOCKS * sizeof(void*));
    ZSizeStack *st[3];
    for (int s = 0; s < 3; s++) st[s] = zd_stack(REUSE_SIZES[s], ZM_LINE);

    for (int r = 0; r < RUNS; r++) {
        zm_reset();
        double t0 = now_ns();
        for (int round = 0; round < REUSE_ROUNDS; round++)
            for (int s = 0; s < 3; s++) {
                for (int i = 0; i < REUSE_BLOCKS; i++) blocks[i] = zd_take(st[s]);
                for (int i = 0; i < REUSE_BLOCKS; i++) zd_give(st[s], blocks[i]);
            }
        T[r] = now_ns() - t0; sink ^= (int64_t)(uintptr_t)blocks[0];
    }
    record_row("Boxed payload (static size class)", T);

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
    record_row("Backing store (runtime size class)", T);

    for (int r = 0; r < RUNS; r++) {
        zm_reset();
        double t0 = now_ns();
        for (int round = 0; round < REUSE_ROUNDS; round++)
            for (int s = 0; s < 3; s++) {
                for (int i = 0; i < REUSE_BLOCKS; i++) blocks[i] = zm_bump(&zm.dyn, REUSE_SIZES[s], ZM_LINE);
                for (int i = 0; i < REUSE_BLOCKS; i++) zd_release(blocks[i], REUSE_SIZES[s]);
            }
        T[r] = now_ns() - t0; sink ^= (int64_t)(uintptr_t)blocks[0];
    }
    record_row("Frontier bump only (no reuse)", T);

    for (int r = 0; r < RUNS; r++) {
        double t0 = now_ns();
        for (int round = 0; round < REUSE_ROUNDS; round++)
            for (int s = 0; s < 3; s++) {
                for (int i = 0; i < REUSE_BLOCKS; i++) blocks[i] = malloc(REUSE_SIZES[s]);
                for (int i = 0; i < REUSE_BLOCKS; i++) free(blocks[i]);
            }
        T[r] = now_ns() - t0; sink ^= (int64_t)(uintptr_t)blocks[0];
    }
    record_row("malloc / free", T);

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
    record_row("Pool (per-size free-list)", T);

    for (size_t size = 1; size < sizeof(void *); size++) check_exact_reuse(size, 1);
    check_exact_reuse(12, 4);
    check_many_small_blocks();

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

typedef struct MNode { int64_t value; struct MNode *left, *right; } MNode;
typedef struct { int64_t value; ZRef left, right; } HNode;
_Static_assert(sizeof(HNode) == sizeof(MNode), "owned node uses native pointers");
typedef struct { int64_t value; ZRef left, right; } VNode;
_Static_assert(sizeof(VNode) == sizeof(MNode), "value node uses native pointers");

static ZSizeStack *bh_stack = NULL;
static ZSizeStack *bv_stack = NULL;

static ZRef bh_build(int depth) {
    HNode *n = (HNode*)zd_take(bh_stack);
    ZRef off = n;
    n->value = depth;
    n->left  = depth > 0 ? bh_build(depth - 1) : 0;
    n->right = depth > 0 ? bh_build(depth - 1) : 0;
    return off;
}
static ZRef bh_relocate(ZRef off) {
    if (!off) return 0;
    HNode *src = (HNode*)off;
    HNode *dst = (HNode*)zd_take(bh_stack);
    *dst = *src;
    dst->left  = bh_relocate(dst->left);
    dst->right = bh_relocate(dst->right);
    zd_give(bh_stack, src);
    return dst;
}
static int64_t bh_sum(ZRef off) {
    if (!off) return 0;
    HNode *n = (HNode*)off;
    return n->value + bh_sum(n->left) + bh_sum(n->right);
}

static ZRef bv_build(int depth) {
    VNode *n = (VNode*)zd_take(bv_stack);
    ZRef off = n;
    n->value = depth;
    n->left  = depth > 0 ? bv_build(depth - 1) : 0;
    n->right = depth > 0 ? bv_build(depth - 1) : 0;
    return off;
}
static ZRef bv_deepcopy(ZRef off) {
    if (!off) return 0;
    VNode *src = (VNode*)off;
    VNode *dst = (VNode*)zd_take(bv_stack);
    dst->value = src->value;
    dst->left  = bv_deepcopy(src->left);
    dst->right = bv_deepcopy(src->right);
    return dst;
}
static int64_t bv_sum(ZRef off) {
    if (!off) return 0;
    VNode *n = (VNode*)off;
    return n->value + bv_sum(n->left) + bv_sum(n->right);
}

typedef struct { int64_t boost; } STurbo;
typedef struct { int64_t power; ZRef turbo; } SEngine;

static ZRef se_build(ZSizeStack *es, ZSizeStack *ts, int64_t power, int64_t boost) {
    SEngine *e = (SEngine*)zd_take(es);
    STurbo  *t = (STurbo*)zd_take(ts);
    t->boost = boost;
    e->power = power;
    e->turbo = t;
    return e;
}
static void se_overwrite(ZSizeStack *es, ZSizeStack *ts, ZRef dst_off, ZRef src_off) {
    SEngine *dst = (SEngine*)dst_off;
    SEngine *src = (SEngine*)src_off;
    *(STurbo*)dst->turbo = *(STurbo*)src->turbo;
    dst->power = src->power;
    zd_give(ts, src->turbo);
    zd_give(es, src);
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

static void test14(void) {
    record_test("Test 14", "Boxed members: roaming escape vs deep value copy  [8191 nodes]");
    double T[RUNS];
    int64_t expected;

    bh_stack = zd_stack(sizeof(HNode), 8);
    bv_stack = zd_stack(sizeof(VNode), 8);
    zm_reset();
    { ZRef root = bv_build(BX_DEPTH); expected = bv_sum(root); }

    for (int r = 0; r < RUNS; r++) {
        zm_reset();
        ZRef root = bh_build(BX_DEPTH);
        double t0 = now_ns();
        root = bh_relocate(root);
        T[r] = now_ns() - t0;
        assert(bh_sum(root) == expected); sink ^= (int64_t)(uintptr_t)root;
    }
    record_row("Escape roaming owned tree (relocate)", T);

    for (int r = 0; r < RUNS; r++) {
        zm_reset();
        ZRef root = bv_build(BX_DEPTH);
        double t0 = now_ns();
        ZRef copy = bv_deepcopy(root);
        T[r] = now_ns() - t0;
        assert(bv_sum(copy) == expected); sink ^= (int64_t)(uintptr_t)copy;
    }
    record_row("Deep-copy value tree (place source)", T);

    for (int r = 0; r < RUNS; r++) {
        zm_reset();
        double t0 = now_ns();
        ZRef root = bv_build(BX_DEPTH);
        T[r] = now_ns() - t0;
        assert(bv_sum(root) == expected); sink ^= (int64_t)(uintptr_t)root;
    }
    record_row("Construct fresh value tree in place", T);

    for (int r = 0; r < RUNS; r++) {
        MNode *root = bm_build(BX_DEPTH);
        double t0 = now_ns();
        MNode *copy = bm_deepcopy(root);
        T[r] = now_ns() - t0;
        sink ^= (int64_t)(uintptr_t)copy;
        bm_destroy(copy); bm_destroy(root);
    }
    record_row("malloc deep copy", T);

    zm_reset();
    {
        ZSizeStack *es = zd_stack(sizeof(SEngine), 8);
        ZSizeStack *ts = zd_stack(sizeof(STurbo), 8);
        ZRef *car_engine = (ZRef*)zm_own(sizeof(ZRef));
        *car_engine = se_build(es, ts, 1, 10);
        ZRef engine = *car_engine;
        ZRef turbo  = ((SEngine*)zm_deref(engine))->turbo;
        ZRef spare = se_build(es, ts, 2, 20);
        ZRef spare_turbo = ((SEngine*)spare)->turbo;
        se_overwrite(es, ts, *car_engine, spare);
        assert(*car_engine == engine);
        assert(((SEngine*)zm_deref(engine))->power == 2);
        assert(((SEngine*)zm_deref(engine))->turbo == turbo);
        assert(((STurbo*)zm_deref(turbo))->boost == 20);
        assert(zd_take(es) == spare);
        assert(zd_take(ts) == spare_turbo);
        sink ^= (int64_t)(uintptr_t)engine ^ (int64_t)(uintptr_t)turbo;
    }
}

static void emit_json(FILE *f) {
    fprintf(f, "{\n  \"schema\": 1,\n");
    fprintf(f, "  \"config\": {\n");
    fprintf(f, "    \"n\": %d,\n", N);
    fprintf(f, "    \"runs\": %d,\n", RUNS);
    fprintf(f, "    \"chunk_bytes\": %lu,\n", (unsigned long)ZM_CHUNK);
    fprintf(f, "    \"region_bytes\": %lu,\n", (unsigned long)REGION_SIZE);
    fprintf(f, "    \"addressing\": \"native\",\n");
    fprintf(f, "    \"scope_frames\": true,\n");
    fprintf(f, "    \"tree_teardown_checksum\": true\n");
    fprintf(f, "  },\n  \"tests\": [\n");

    for (int i = 0; i < bench_ntests; i++) {
        BenchTest *t = &bench_tests[i];
        fprintf(f, "    {\n      \"id\": ");   json_string(f, t->id);
        fprintf(f, ",\n      \"title\": ");    json_string(f, t->title);
        fprintf(f, ",\n      \"rows\": [\n");
        for (int r = 0; r < t->nrows; r++) {
            BenchRow *row = &t->rows[r];
            fprintf(f, "        { \"label\": ");
            json_string(f, row->label);
            if (row->eliminated) {
                fprintf(f, ", \"eliminated\": true");
            } else {
                fprintf(f, ", \"samples_ns\": [");
                for (int k = 0; k < row->nsamples; k++)
                    fprintf(f, "%s%.2f", k ? ", " : "", row->samples[k]);
                fprintf(f, "]");
            }
            fprintf(f, " }%s\n", r + 1 < t->nrows ? "," : "");
        }
        fprintf(f, "      ]\n    }%s\n", i + 1 < bench_ntests ? "," : "");
    }
    fprintf(f, "  ]\n}\n");
}

int main(void) {
    zm_init(); ar_init(); pool_flush(); workers_init(); workers_warm();

    test1(); test2(); test3(); test4(); test5();
    test6(); test7(); test8(); test9(); test10(); test11(); test12();
    test13(); test14();

    workers_shutdown();
    for (int i = 0; i < ZD_STACKS; i++) free(zm.stacks[i].small);
    emit_json(stdout);
    fprintf(stderr, "sink = %lld\n", (long long)sink);
    return 0;
}
