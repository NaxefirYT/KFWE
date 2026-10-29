#include <windows.h>
#include <commctrl.h>
#include <stdint.h>
#include <string.h>
#include <stdlib.h>
#include "fastsub.h"

#define TAG_LIVE 0x48465354u
#define TAG_FREE 0x46465354u
#define KEY 0x5A17C3E9u
#define STORM_MAGIC 0x6F6D
#define ZERO_FLAG 8
#define NOMOVE_FLAG 0x10

typedef struct {
    uint32_t check;
    uint32_t size;
    uint32_t where;
    uint32_t tag;
} hdr_t;

#define NCLASS 96
static uint32_t g_class_size[NCLASS];
static int g_nclass;

static void init_classes(void)
{
    int c = 0;
    for (uint32_t s = 16; s <= 256; s += 16)
        g_class_size[c++] = s;
    for (uint32_t base = 256; base < 0x40000000u && c + 4 <= NCLASS; base *= 2)
        for (int k = 1; k <= 4; k++)
            g_class_size[c++] = base + base / 4 * k;
    g_nclass = c;
}

static int class_of(uint32_t need)
{
    if (need <= 256)
        return need == 0 ? 0 : (int)((need - 1) / 16);
    int lo = 16, hi = g_nclass - 1;
    while (lo < hi) {
        int mid = (lo + hi) / 2;
        if (g_class_size[mid] >= need)
            hi = mid;
        else
            lo = mid + 1;
    }
    return g_class_size[lo] >= need ? lo : -1;
}

#define POOL_BITS 15
#define POOL_MAX (1u << POOL_BITS)
typedef struct {
    uintptr_t file;
    int line;
    int used;
    int fast;
    hdr_t *free[NCLASS];
} pool_t;
static pool_t *g_pools;

static const char *const g_fast_sites[] = { ".D" };

static int is_fast_site(const char *file, int line)
{
    if (line != -2 || !file || IsBadReadPtr(file, 4))
        return 0;
    for (unsigned i = 0; i < sizeof(g_fast_sites) / sizeof(g_fast_sites[0]); i++)
        if (!lstrcmpA(file, g_fast_sites[i]))
            return 1;
    return 0;
}

static uint32_t pool_index(const char *file, int line)
{
    uint32_t h = (uint32_t)(uintptr_t)file * 2654435761u ^ (uint32_t)line * 40503u;
    uint32_t i = (h >> (32 - POOL_BITS)) & (POOL_MAX - 1);
    for (uint32_t probe = 0; probe < POOL_MAX; probe++, i = (i + 1) & (POOL_MAX - 1)) {
        pool_t *p = &g_pools[i];
        if (!p->used) {
            p->used = 1;
            p->file = (uintptr_t)file;
            p->line = line;
            p->fast = is_fast_site(file, line);
            return i;
        }
        if (p->file == (uintptr_t)file && p->line == line)
            return i;
    }
    return 0;
}

#define ARENA_SIZE (1u << 20)
#define BIG_LIMIT (256u * 1024)
static uint8_t *g_arena_cur, *g_arena_end;

static hdr_t *carve(uint32_t total)
{
    if (total > BIG_LIMIT)
        return (hdr_t *)VirtualAlloc(NULL, total, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
    if ((uint32_t)(g_arena_end - g_arena_cur) < total) {
        uint8_t *a = (uint8_t *)VirtualAlloc(NULL, ARENA_SIZE, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
        if (!a)
            return NULL;
        g_arena_cur = a;
        g_arena_end = a + ARENA_SIZE;
    }
    hdr_t *h = (hdr_t *)g_arena_cur;
    g_arena_cur += total;
    return h;
}

typedef void *(__stdcall *alloc_fn)(uint32_t, const char *, int, int);
typedef BOOL (__stdcall *free_fn)(void *, const char *, int, int);
typedef uint32_t (__stdcall *size_fn)(void *, const char *, int);
typedef void *(__stdcall *realloc_fn)(void *, uint32_t, const char *, int, int);

static SRWLOCK g_lock = SRWLOCK_INIT;
static alloc_fn o_alloc;
static free_fn o_free;
static size_fn o_size;
static realloc_fn o_realloc;
static volatile LONG g_allocs, g_foreign_frees;

#define SLACK 8

static int mine_tag(void *p, uint32_t tag)
{
    const uint8_t *u = (const uint8_t *)p;
    if (*(const uint16_t *)(u - 2) == STORM_MAGIC)
        return 0;
    const hdr_t *h = (const hdr_t *)p - 1;
    return h->tag == tag && h->check == ((uint32_t)(uintptr_t)p ^ KEY);
}

static void *alloc_in(uint32_t pool, uint32_t n, int flags)
{
    int c = class_of(n + SLACK);
    if (c < 0)
        return NULL;
    pool_t *p = &g_pools[pool];
    hdr_t *h = p->free[c];
    if (h)
        p->free[c] = (hdr_t *)(uintptr_t)h->size;
    else if (!(h = carve(sizeof(hdr_t) + g_class_size[c])))
        return NULL;
    void *u = h + 1;
    h->size = n;
    h->where = pool << 8 | (uint32_t)c;
    h->check = (uint32_t)(uintptr_t)u ^ KEY;
    h->tag = TAG_LIVE;
    if (flags & ZERO_FLAG)
        memset(u, 0, n);
    g_allocs++;
    return u;
}

static void free_in(hdr_t *h)
{
    pool_t *p = &g_pools[h->where >> 8];
    int c = (int)(h->where & 0xFF);
    h->tag = TAG_FREE;
    h->size = (uint32_t)(uintptr_t)p->free[c];
    p->free[c] = h;
}

static void *__stdcall h_alloc(uint32_t n, const char *file, int line, int flags)
{
    if (n > 0x3FF00000u)
        return o_alloc(n, file, line, flags);
    AcquireSRWLockExclusive(&g_lock);
    uint32_t pool = pool_index(file, line);
    void *u = g_pools[pool].fast ? alloc_in(pool, n, flags) : NULL;
    ReleaseSRWLockExclusive(&g_lock);
    return u ? u : o_alloc(n, file, line, flags);
}

static BOOL __stdcall h_free(void *p, const char *file, int line, int dflt)
{
    if (p && mine_tag(p, TAG_LIVE)) {
        AcquireSRWLockExclusive(&g_lock);
        free_in((hdr_t *)p - 1);
        ReleaseSRWLockExclusive(&g_lock);
        return TRUE;
    }
    if (p && mine_tag(p, TAG_FREE))
        return FALSE;
    if (p)
        InterlockedIncrement(&g_foreign_frees);
    return o_free(p, file, line, dflt);
}

static uint32_t __stdcall h_size(void *p, const char *file, int line)
{
    if (p && mine_tag(p, TAG_LIVE))
        return ((hdr_t *)p - 1)->size;
    return o_size(p, file, line);
}

static void *__stdcall h_realloc(void *p, uint32_t n, const char *file, int line, int flags)
{
    if (!p)
        return h_alloc(n, file, line, flags);
    if (!mine_tag(p, TAG_LIVE) || n > 0x3FF00000u)
        return o_realloc(p, n, file, line, flags);
    hdr_t *h = (hdr_t *)p - 1;
    uint32_t old = h->size;
    int c = (int)(h->where & 0xFF);
    if (n + SLACK <= g_class_size[c]) {
        if (n > old && (flags & ZERO_FLAG))
            memset((uint8_t *)p + old, 0, n - old);
        h->size = n;
        return p;
    }
    if (flags & NOMOVE_FLAG)
        return NULL;
    AcquireSRWLockExclusive(&g_lock);
    void *np = alloc_in(h->where >> 8, n, 0);
    if (np) {
        memcpy(np, p, old < n ? old : n);
        if (n > old && (flags & ZERO_FLAG))
            memset((uint8_t *)np + old, 0, n - old);
        free_in(h);
    }
    ReleaseSRWLockExclusive(&g_lock);
    return np;
}

#ifdef SFH_PROFILE
static char g_logpath[MAX_PATH];
static void log_line(HANDLE f, const char *s);
typedef struct { uint64_t ticks; uint32_t calls, big, reallocs; uint64_t bytes; } prof_t;
static prof_t *g_prof;
static const char *safe_str(const char *s)
{
    if (!s || (uintptr_t)s < 0x10000 || IsBadStringPtrA(s, 300)) return "?";
    return s;
}
static void prof_add(const char *file, int line, uint64_t dt, uint32_t n, int re)
{
    AcquireSRWLockExclusive(&g_lock);
    prof_t *p = &g_prof[pool_index(file, line)];
    p->ticks += dt; p->calls++; p->bytes += n;
    if (n >= 256) p->big++;
    if (re) p->reallocs++;
    ReleaseSRWLockExclusive(&g_lock);
}
static void *__stdcall p_alloc(uint32_t n, const char *file, int line, int flags)
{
    LARGE_INTEGER a, b; QueryPerformanceCounter(&a);
    void *r = o_alloc(n, file, line, flags);
    QueryPerformanceCounter(&b); prof_add(file, line, b.QuadPart - a.QuadPart, n, 0);
    return r;
}
static void *__stdcall p_realloc(void *p, uint32_t n, const char *file, int line, int flags)
{
    LARGE_INTEGER a, b; QueryPerformanceCounter(&a);
    void *r = o_realloc(p, n, file, line, flags);
    QueryPerformanceCounter(&b); prof_add(file, line, b.QuadPart - a.QuadPart, n, 1);
    return r;
}
static BOOL __stdcall p_free(void *p, const char *file, int line, int d) { return o_free(p, file, line, d); }
static uint32_t __stdcall p_size(void *p, const char *file, int line) { return o_size(p, file, line); }
static void prof_dump(void)
{
    char path[MAX_PATH]; lstrcpyA(path, g_logpath);
    int L = lstrlenA(path); if (L > 9) lstrcpyA(path + L - 9, "prof.log");
    HANDLE f = CreateFileA(path, GENERIC_WRITE, FILE_SHARE_READ, NULL, CREATE_ALWAYS, 0, NULL);
    if (f == INVALID_HANDLE_VALUE) return;
    LARGE_INTEGER fq; QueryPerformanceFrequency(&fq);
    char b[700];
    for (int rank = 0; rank < 60; rank++) {
        int best = -1; uint64_t bt = 0;
        for (uint32_t i = 0; i < POOL_MAX; i++)
            if (g_pools[i].used && g_prof[i].ticks > bt && g_prof[i].calls != 0xFFFFFFFF) { bt = g_prof[i].ticks; best = (int)i; }
        if (best < 0) break;
        prof_t *p = &g_prof[best];
        wsprintfA(b, "%6u ms  calls=%u big=%u reallocs=%u avgsize=%u  %s:%d\r\n", (unsigned)(p->ticks * 1000 / fq.QuadPart),
                  p->calls, p->big, p->reallocs, (unsigned)(p->calls ? p->bytes / p->calls : 0), safe_str((const char *)g_pools[best].file), g_pools[best].line);
        log_line(f, b);
        p->calls = 0xFFFFFFFF;
    }
    CloseHandle(f);
}
#define H_ALLOC p_alloc
#define H_FREE p_free
#define H_SIZE p_size
#define H_REALLOC p_realloc
#elif defined(SFH_LIVE)
#define LIVE_BITS 21
#define LIVE_CAP (1u << LIVE_BITS)
static uint32_t *g_lkey, *g_lsize;
static uint16_t *g_lsite;
static int64_t *g_site_bytes, *g_site_prev;
static int32_t *g_site_cnt;
static uint32_t g_lused, g_loverflow;
static SRWLOCK g_llock = SRWLOCK_INIT;
static char g_logpath[MAX_PATH];
static void log_line(HANDLE f, const char *s);

static uint32_t lslot(uint32_t p) { return (p * 2654435761u) >> (32 - LIVE_BITS); }

static void live_add(void *ptr, uint32_t site, uint32_t n)
{
    uint32_t p = (uint32_t)(uintptr_t)ptr;
    AcquireSRWLockExclusive(&g_llock);
    if (g_lused > LIVE_CAP / 4 * 3) {
        g_loverflow++;
    } else {
        uint32_t i = lslot(p);
        while (g_lkey[i] && g_lkey[i] != p)
            i = (i + 1) & (LIVE_CAP - 1);
        if (!g_lkey[i]) {
            g_lused++;
        } else {
            g_site_bytes[g_lsite[i]] -= g_lsize[i];
            g_site_cnt[g_lsite[i]]--;
        }
        g_lkey[i] = p; g_lsize[i] = n; g_lsite[i] = (uint16_t)site;
        g_site_bytes[site] += n;
        g_site_cnt[site]++;
    }
    ReleaseSRWLockExclusive(&g_llock);
}

static void live_del(void *ptr)
{
    uint32_t p = (uint32_t)(uintptr_t)ptr;
    if (!p)
        return;
    AcquireSRWLockExclusive(&g_llock);
    uint32_t i = lslot(p);
    while (g_lkey[i] && g_lkey[i] != p)
        i = (i + 1) & (LIVE_CAP - 1);
    if (g_lkey[i]) {
        g_site_bytes[g_lsite[i]] -= g_lsize[i];
        g_site_cnt[g_lsite[i]]--;
        g_lused--;
        uint32_t j = i;
        for (;;) {
            j = (j + 1) & (LIVE_CAP - 1);
            if (!g_lkey[j])
                break;
            uint32_t k = lslot(g_lkey[j]);
            if ((j > i && (k <= i || k > j)) || (j < i && (k <= i && k > j))) {
                g_lkey[i] = g_lkey[j]; g_lsize[i] = g_lsize[j]; g_lsite[i] = g_lsite[j];
                i = j;
            }
        }
        g_lkey[i] = 0;
    }
    ReleaseSRWLockExclusive(&g_llock);
}

static uint32_t site_of(const char *file, int line)
{
    AcquireSRWLockExclusive(&g_lock);
    uint32_t s = pool_index(file, line);
    ReleaseSRWLockExclusive(&g_lock);
    return s;
}

static void *__stdcall l_alloc(uint32_t n, const char *file, int line, int flags)
{
    void *r = h_alloc(n, file, line, flags);
    if (r)
        live_add(r, site_of(file, line), n);
    return r;
}
static BOOL __stdcall l_free(void *p, const char *file, int line, int d)
{
    live_del(p);
    return h_free(p, file, line, d);
}
static uint32_t __stdcall l_size(void *p, const char *file, int line) { return h_size(p, file, line); }
static void *__stdcall l_realloc(void *p, uint32_t n, const char *file, int line, int flags)
{
    void *r = h_realloc(p, n, file, line, flags);
    if (r) {
        live_del(p);
        live_add(r, site_of(file, line), n);
    }
    return r;
}

static const char *site_name(uintptr_t s)
{
    if (!s || s < 0x10000 || IsBadStringPtrA((const char *)s, 300)) return "?";
    return (const char *)s;
}

static DWORD WINAPI live_dump_thread(void *arg)
{
    (void)arg;
    char path[MAX_PATH], b[700];
    lstrcpyA(path, g_logpath);
    int L = lstrlenA(path);
    if (L > 9) lstrcpyA(path + L - 9, "live.log");
    static uint32_t top[30];
    for (;;) {
        Sleep(30000);
        HANDLE f = CreateFileA(path, FILE_APPEND_DATA, FILE_SHARE_READ, NULL, OPEN_ALWAYS, 0, NULL);
        if (f == INVALID_HANDLE_VALUE)
            continue;
        SYSTEMTIME t;
        GetLocalTime(&t);
        uint64_t pooled = 0;
        AcquireSRWLockExclusive(&g_lock);
        for (uint32_t i = 0; i < POOL_MAX; i++) {
            if (!g_pools[i].used || !g_pools[i].fast) continue;
            for (int c = 0; c < g_nclass; c++)
                for (hdr_t *h = g_pools[i].free[c]; h; h = (hdr_t *)(uintptr_t)h->size)
                    pooled += g_class_size[c];
        }
        ReleaseSRWLockExclusive(&g_lock);
        AcquireSRWLockShared(&g_llock);
        int64_t total = 0;
        for (uint32_t i = 0; i < POOL_MAX; i++)
            total += g_site_bytes[i];
        wsprintfA(b, "=== %02d:%02d:%02d live=%dMB in %u blocks, overflow=%u, sfh pooled free=%dMB\r\n", t.wHour, t.wMinute, t.wSecond,
                  (int)(total >> 20), g_lused, g_loverflow, (int)(pooled >> 20));
        log_line(f, b);
        int n = 0;
        for (; n < 30; n++) {
            int64_t best = 0; int bi = -1;
            for (uint32_t i = 0; i < POOL_MAX; i++) {
                if (g_site_bytes[i] <= best) continue;
                int taken = 0;
                for (int k = 0; k < n; k++) if (top[k] == i) taken = 1;
                if (!taken) { best = g_site_bytes[i]; bi = (int)i; }
            }
            if (bi < 0) break;
            top[n] = (uint32_t)bi;
            wsprintfA(b, "%8dKB %8dKB %7d  %s:%d\r\n", (int)(g_site_bytes[bi] >> 10), (int)((g_site_bytes[bi] - g_site_prev[bi]) >> 10),
                      g_site_cnt[bi], site_name(g_pools[bi].file), g_pools[bi].line);
            log_line(f, b);
        }
        log_line(f, "  growth:\r\n");
        for (int k = 0; k < 10; k++) {
            int64_t best = 512 << 10; int bi = -1;
            for (uint32_t i = 0; i < POOL_MAX; i++) {
                int64_t d = g_site_bytes[i] - g_site_prev[i];
                if (d <= best) continue;
                int taken = 0;
                for (int q = 0; q < n; q++) if (top[q] == i) taken = 1;
                if (!taken) { best = d; bi = (int)i; }
            }
            if (bi < 0) break;
            top[n < 30 ? n++ : 29] = (uint32_t)bi;
            wsprintfA(b, "%8dKB %8dKB %7d  %s:%d\r\n", (int)(g_site_bytes[bi] >> 10), (int)(best >> 10), g_site_cnt[bi],
                      site_name(g_pools[bi].file), g_pools[bi].line);
            log_line(f, b);
        }
        memcpy(g_site_prev, g_site_bytes, sizeof(int64_t) * POOL_MAX);
        ReleaseSRWLockShared(&g_llock);
        CloseHandle(f);
    }
    return 0;
}

static int live_init(void)
{
    g_lkey = (uint32_t *)VirtualAlloc(NULL, LIVE_CAP * 4, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
    g_lsize = (uint32_t *)VirtualAlloc(NULL, LIVE_CAP * 4, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
    g_lsite = (uint16_t *)VirtualAlloc(NULL, LIVE_CAP * 2, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
    g_site_bytes = (int64_t *)VirtualAlloc(NULL, POOL_MAX * 8, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
    g_site_prev = (int64_t *)VirtualAlloc(NULL, POOL_MAX * 8, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
    g_site_cnt = (int32_t *)VirtualAlloc(NULL, POOL_MAX * 4, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
    return g_lkey && g_lsize && g_lsite && g_site_bytes && g_site_prev && g_site_cnt;
}
#define H_ALLOC l_alloc
#define H_FREE l_free
#define H_SIZE l_size
#define H_REALLOC l_realloc
#else
#define H_ALLOC h_alloc
#define H_FREE h_free
#define H_SIZE h_size
#define H_REALLOC h_realloc
#endif

static int check_prologue(const uint8_t *fn)
{
    return fn[0] == 0x83 && fn[1] == 0x3D && fn[6] == 0x00 && fn[7] == 0x75;
}

static void *make_trampoline(uint8_t *fn)
{
    uint8_t *t = (uint8_t *)VirtualAlloc(NULL, 32, MEM_COMMIT | MEM_RESERVE, PAGE_EXECUTE_READWRITE);
    if (!t)
        return NULL;
    memcpy(t, fn, 7);
    uint8_t *jne_target = fn + 9 + (int8_t)fn[8];
    t[7] = 0x0F; t[8] = 0x85;
    *(int32_t *)(t + 9) = (int32_t)(jne_target - (t + 13));
    t[13] = 0xE9;
    *(int32_t *)(t + 14) = (int32_t)((fn + 9) - (t + 18));
    FlushInstructionCache(GetCurrentProcess(), t, 18);
    return t;
}

static int patch_jmp(uint8_t *fn, void *target)
{
    DWORD old;
    if (!VirtualProtect(fn, 5, PAGE_EXECUTE_READWRITE, &old))
        return 0;
    *(int32_t *)(fn + 1) = (int32_t)((uint8_t *)target - (fn + 5));
    fn[0] = 0xE9;
    VirtualProtect(fn, 5, old, &old);
    FlushInstructionCache(GetCurrentProcess(), fn, 5);
    return 1;
}

#ifndef SFH_PROFILE
static char g_logpath[MAX_PATH];
#endif
static volatile LONG g_crashes;

static void log_line(HANDLE f, const char *s)
{
    DWORD w;
    WriteFile(f, s, lstrlenA(s), &w, NULL);
}

static void describe(char *out, uintptr_t a)
{
    HMODULE m = NULL;
    char name[MAX_PATH] = "?";
    if (GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT, (LPCSTR)a, &m) && m) {
        GetModuleFileNameA(m, name, sizeof(name));
        const char *b = name;
        for (const char *c = name; *c; c++)
            if (*c == '\\' || *c == '/')
                b = c + 1;
        wsprintfA(out, "%08X %s+%X", (unsigned)a, b, (unsigned)(a - (uintptr_t)m));
    } else {
        wsprintfA(out, "%08X", (unsigned)a);
    }
}

static int is_code(uintptr_t a)
{
    MEMORY_BASIC_INFORMATION mbi;
    if (a < 0x10000 || !VirtualQuery((LPCVOID)a, &mbi, sizeof(mbi)))
        return 0;
    return mbi.Type == MEM_IMAGE && (mbi.Protect & (PAGE_EXECUTE | PAGE_EXECUTE_READ | PAGE_EXECUTE_READWRITE | PAGE_EXECUTE_WRITECOPY));
}

static LONG CALLBACK on_exception(PEXCEPTION_POINTERS ep)
{
    DWORD code = ep->ExceptionRecord->ExceptionCode;
    if (code != 0xC0000005 && code != 0xC0000374 && code != 0xC0000008 && code != 0xC000001D)
        return EXCEPTION_CONTINUE_SEARCH;
    if (InterlockedIncrement(&g_crashes) > 3 || !g_logpath[0])
        return EXCEPTION_CONTINUE_SEARCH;
    HANDLE f = CreateFileA(g_logpath, FILE_APPEND_DATA, FILE_SHARE_READ, NULL, OPEN_ALWAYS, 0, NULL);
    if (f == INVALID_HANDLE_VALUE)
        return EXCEPTION_CONTINUE_SEARCH;
    char buf[512], d[300];
    CONTEXT *c = ep->ContextRecord;
    describe(d, (uintptr_t)ep->ExceptionRecord->ExceptionAddress);
    wsprintfA(buf, "=== exception %08X at %s  info=%08X,%08X  thread=%u\r\n", (unsigned)code, d,
              (unsigned)ep->ExceptionRecord->ExceptionInformation[0], (unsigned)ep->ExceptionRecord->ExceptionInformation[1],
              (unsigned)GetCurrentThreadId());
    log_line(f, buf);
    wsprintfA(buf, "eax=%08X ebx=%08X ecx=%08X edx=%08X esi=%08X edi=%08X ebp=%08X esp=%08X\r\n",
              (unsigned)c->Eax, (unsigned)c->Ebx, (unsigned)c->Ecx, (unsigned)c->Edx, (unsigned)c->Esi, (unsigned)c->Edi,
              (unsigned)c->Ebp, (unsigned)c->Esp);
    log_line(f, buf);
    wsprintfA(buf, "allocs=%d foreign_frees=%d\r\nstack (code addresses):\r\n", (int)g_allocs, (int)g_foreign_frees);
    log_line(f, buf);
    uint32_t *sp = (uint32_t *)c->Esp;
    MEMORY_BASIC_INFORMATION mbi;
    if (VirtualQuery(sp, &mbi, sizeof(mbi))) {
        uint32_t *end = (uint32_t *)((uint8_t *)mbi.BaseAddress + mbi.RegionSize);
        int shown = 0;
        for (int i = 0; i < 2048 && sp + i < end && shown < 40; i++) {
            if (is_code(sp[i])) {
                describe(d, sp[i]);
                wsprintfA(buf, "  [esp+%03X] %s\r\n", i * 4, d);
                log_line(f, buf);
                shown++;
            }
        }
    }
    CloseHandle(f);
    return EXCEPTION_CONTINUE_SEARCH;
}

static void init_crash_log(void)
{
    HMODULE self = NULL;
    GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT, (LPCSTR)&on_exception, &self);
    char p[MAX_PATH];
    DWORD n = GetModuleFileNameA(self, p, MAX_PATH);
    int cut = 0;
    for (int i = (int)n - 1; i >= 0 && cut < 3; i--)
        if (p[i] == '\\' || p[i] == '/') {
            p[i] = 0;
            cut++;
        }
    if (cut == 3) {
        wsprintfA(g_logpath, "%s\\logs\\stormfastheap_crash.log", p);
        AddVectoredExceptionHandler(1, on_exception);
    }
}

typedef struct {
    const char *name;
    uint32_t addr;
    uint8_t len;
    uint8_t orig[16];
    uint8_t repl[16];
} code_patch_t;

static const code_patch_t g_code_patches[] = {
    { "grow-0x144-array", 0x0051A8B9, 12,
      { 0xC7, 0x41, 0x0C, 0x08, 0x00, 0x00, 0x00, 0xB8, 0x08, 0x00, 0x00, 0x00 },
      { 0x48, 0x0F, 0xBD, 0xC8, 0xB8, 0x02, 0x00, 0x00, 0x00, 0xD3, 0xE0, 0x90 } },
};
static volatile LONG g_patches_applied;

static void apply_code_patches(void)
{
    if ((uintptr_t)GetModuleHandleA(NULL) != 0x400000)
        return;
    for (unsigned i = 0; i < sizeof(g_code_patches) / sizeof(g_code_patches[0]); i++) {
        const code_patch_t *cp = &g_code_patches[i];
        uint8_t *at = (uint8_t *)(uintptr_t)cp->addr;
        if (IsBadReadPtr(at, cp->len) || memcmp(at, cp->orig, cp->len))
            continue;
        DWORD old;
        if (!VirtualProtect(at, cp->len, PAGE_EXECUTE_READWRITE, &old))
            continue;
        memcpy(at, cp->repl, cp->len);
        VirtualProtect(at, cp->len, old, &old);
        FlushInstructionCache(GetCurrentProcess(), at, cp->len);
        g_patches_applied |= 1 << i;
    }
}

typedef int(__thiscall *lookup_fn)(void *self, const char *name, int flag);
static lookup_fn o_lookup;
static volatile LONG g_batch;
static volatile LONG g_batch_id;
static volatile uint32_t g_saved_ret;
static volatile LONG g_idx_hits, g_idx_fallbacks, g_idx_mismatch, g_idx_builds;

typedef struct {
    uint32_t self, blob, blen, batch;
    uint32_t cap;
    uint64_t *keys;
    uint32_t *pos;
} vidx_t;
#define VIDX_SLOTS 8192
static vidx_t *g_vidx;

static int is_lower(uint8_t c) { return (int8_t)c >= 'a' && c <= 'z'; }
static int is_ident(uint8_t c) { return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '_'; }
static uint64_t fnv_step(uint64_t h, uint8_t c) { return (h ^ c) * 1099511628211ull; }
#define FNV0 1469598103934665603ull

static int name_prefix(const uint8_t *s)
{
    if (s[0] == 'u' && s[1] == 'd' && s[2] == 'g' && s[3] == '_')
        return 4;
    if ((s[0] | 0x20) == 'g' && (s[1] | 0x20) == 'g' && s[2] == '_')
        return 3;
    return 0;
}

static void vidx_insert(vidx_t *v, uint64_t key, uint32_t p)
{
    if (!key)
        key = 1;
    uint32_t m = v->cap - 1, i = (uint32_t)(key ^ (key >> 32)) & m;
    while (v->keys[i] && v->keys[i] != key)
        i = (i + 1) & m;
    if (!v->keys[i]) {
        v->keys[i] = key;
        v->pos[i] = p;
    }
}

static void vidx_free(vidx_t *v)
{
    HANDLE ph = GetProcessHeap();
    if (v->keys) HeapFree(ph, 0, v->keys);
    if (v->pos) HeapFree(ph, 0, v->pos);
    memset(v, 0, sizeof(*v));
}

static int vidx_build(vidx_t *v, const uint8_t *b, uint32_t len)
{
    uint32_t n = 0;
    for (uint32_t p = 0; p + 3 <= len; p++) {
        if ((p && is_lower(b[p - 1])) || !name_prefix(b + p))
            continue;
        for (uint32_t q = p; q < len && is_ident(b[q]); q++)
            n++;
    }
    uint32_t cap = 64;
    while (cap < n * 2 + 16)
        cap <<= 1;
    HANDLE ph = GetProcessHeap();
    v->keys = (uint64_t *)HeapAlloc(ph, HEAP_ZERO_MEMORY, (SIZE_T)cap * 8);
    v->pos = (uint32_t *)HeapAlloc(ph, 0, (SIZE_T)cap * 4);
    if (!v->keys || !v->pos) {
        vidx_free(v);
        return 0;
    }
    v->cap = cap;
    for (uint32_t p = 0; p + 3 <= len; p++) {
        if ((p && is_lower(b[p - 1])) || !name_prefix(b + p))
            continue;
        uint64_t h = FNV0;
        for (uint32_t q = p; q < len && is_ident(b[q]); q++) {
            h = fnv_step(h, b[q]);
            uint8_t next = q + 1 < len ? b[q + 1] : 0;
            if (!is_lower(next))
                vidx_insert(v, h, p);
        }
    }
    return 1;
}

static int has_border(const uint8_t *s, uint32_t L)
{
    for (uint32_t k = 1; k < L; k++)
        if (!memcmp(s, s + L - k, k))
            return 1;
    return 0;
}

static int __thiscall h_lookup(void *self, const char *name, int flag)
{
    const uint8_t *o = (const uint8_t *)self;
    if (!g_batch || !flag || !*(const uint32_t *)(o + 0x28) || !*(const uint32_t *)(o + 0x34) || !name)
        return o_lookup(self, name, flag);
    const uint8_t *blob = *(const uint8_t *const *)(o + 0x38);
    uint32_t blen = *(const uint32_t *)(o + 0x34);
    const uint8_t *nm = (const uint8_t *)name;
    uint32_t L = 0;
    while (L < 200 && nm[L])
        L++;
    if (!blob || L < 3 || L >= 200)
        goto fallback;
    static uint8_t last[200];
    static uint32_t lastL;
    static int last_ok;
    static uint64_t last_key;
    if (L != lastL || memcmp(last, nm, L)) {
        int ok = name_prefix(nm) != 0;
        for (uint32_t i = 0; ok && i < L; i++)
            ok = is_ident(nm[i]);
        if (ok && has_border(nm, L))
            ok = 0;
        uint64_t k = FNV0;
        for (uint32_t i = 0; i < L; i++)
            k = fnv_step(k, nm[i]);
        memcpy(last, nm, L);
        lastL = L;
        last_ok = ok;
        last_key = k ? k : 1;
    }
    if (!last_ok)
        goto fallback;

    uint32_t cur = (uint32_t)g_batch_id, sk = (uint32_t)(uintptr_t)self;
    uint32_t slot = (sk * 2654435761u) >> (32 - 13);
    vidx_t *v = NULL;
    for (uint32_t probe = 0; probe < VIDX_SLOTS; probe++, slot = (slot + 1) & (VIDX_SLOTS - 1)) {
        vidx_t *c = &g_vidx[slot];
        if (c->batch != cur || c->self == sk) {
            v = c;
            break;
        }
    }
    if (!v)
        goto fallback;
    if (v->self != sk || v->blob != (uint32_t)(uintptr_t)blob || v->blen != blen || v->batch != cur) {
        vidx_free(v);
        g_idx_builds++;
        uint32_t len = 0;
        while (len < blen && blob[len])
            len++;
        v->self = (uint32_t)(uintptr_t)self;
        v->blob = (uint32_t)(uintptr_t)blob;
        v->blen = blen;
        v->batch = (uint32_t)g_batch_id;
        if (!vidx_build(v, blob, len)) {
            v->batch = 0;
            goto fallback;
        }
    }
    uint64_t key = last_key;
    uint32_t m = v->cap - 1, i = (uint32_t)(key ^ (key >> 32)) & m;
    while (v->keys[i] && v->keys[i] != key)
        i = (i + 1) & m;
    int r;
    if (!v->keys[i])
        r = 0;
    else if (!memcmp(blob + v->pos[i], nm, L))
        r = 1;
    else
        goto fallback;
    g_idx_hits++;
#ifdef SFH_VERIFY
    if (o_lookup(self, name, flag) != r)
        g_idx_mismatch++;
#endif
    return r;
fallback:
    g_idx_fallbacks++;
    return o_lookup(self, name, flag);
}

static void apply_var_index_hooks(void)
{
    static const uint8_t lookup_orig[8] = { 0x55, 0x8B, 0xEC, 0x51, 0x53, 0x57, 0x8B, 0xF9 };
    uint8_t *fn = (uint8_t *)0x005CB700;
    uint8_t *site = (uint8_t *)0x0052527D;
    if ((uintptr_t)GetModuleHandleA(NULL) != 0x400000 || IsBadReadPtr(fn, 8) || IsBadReadPtr(site, 5))
        return;
    if (memcmp(fn, lookup_orig, 8) || site[0] != 0xE8 || site + 5 + *(int32_t *)(site + 1) != (uint8_t *)0x0051F170)
        return;
    g_vidx = (vidx_t *)VirtualAlloc(NULL, sizeof(vidx_t) * VIDX_SLOTS, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
    uint8_t *t = (uint8_t *)VirtualAlloc(NULL, 64, MEM_COMMIT | MEM_RESERVE, PAGE_EXECUTE_READWRITE);
    if (!g_vidx || !t)
        return;
    memcpy(t, fn, 6);
    t[6] = 0xE9;
    *(int32_t *)(t + 7) = (int32_t)((fn + 6) - (t + 11));
    o_lookup = (lookup_fn)(void *)t;
    uint8_t *s = t + 16, *w = s;
    *w++ = 0xFF; *w++ = 0x05; *(uint32_t *)w = (uint32_t)(uintptr_t)&g_batch_id; w += 4;
    *w++ = 0xC7; *w++ = 0x05; *(uint32_t *)w = (uint32_t)(uintptr_t)&g_batch; w += 4; *(uint32_t *)w = 1; w += 4;
    *w++ = 0x8F; *w++ = 0x05; *(uint32_t *)w = (uint32_t)(uintptr_t)&g_saved_ret; w += 4;
    *w++ = 0xE8; *(int32_t *)w = (int32_t)((uint8_t *)0x0051F170 - (w + 4)); w += 4;
    *w++ = 0xC7; *w++ = 0x05; *(uint32_t *)w = (uint32_t)(uintptr_t)&g_batch; w += 4; *(uint32_t *)w = 0; w += 4;
    *w++ = 0xFF; *w++ = 0x35; *(uint32_t *)w = (uint32_t)(uintptr_t)&g_saved_ret; w += 4;
    *w++ = 0xC3;
    FlushInstructionCache(GetCurrentProcess(), t, 64);

    DWORD old;
    if (!VirtualProtect(site, 5, PAGE_EXECUTE_READWRITE, &old))
        return;
    *(int32_t *)(site + 1) = (int32_t)(s - (site + 5));
    VirtualProtect(site, 5, old, &old);
    FlushInstructionCache(GetCurrentProcess(), site, 5);
    if (patch_jmp(fn, (void *)h_lookup))
        g_patches_applied |= 1 << 8;
}

typedef int (__fastcall *find_fn)(void *self, void *edx, uint32_t id, int exact);
static find_fn o_find;
static volatile LONG g_find_hits, g_find_miss, g_find_builds, g_find_orig;

#define FT_SLOTS 8
typedef struct {
    void *self; uint8_t *data; int count; DWORD tick; DWORD used;
    uint32_t mask; uint32_t *keys; int32_t *idx;
} ftab_t;
static ftab_t g_ft[FT_SLOTS];

static uint32_t fhash(uint32_t x) { x ^= x >> 16; x *= 0x7FEB352Du; x ^= x >> 15; x *= 0x846CA68Bu; x ^= x >> 16; return x; }

static int ft_build(ftab_t *t, void *self, uint8_t *data, int count)
{
    uint32_t cap = 16;
    while (cap < (uint32_t)count * 2) cap <<= 1;
    if (!t->keys || t->mask + 1 < cap) {
        if (t->keys) { HeapFree(GetProcessHeap(), 0, t->keys); HeapFree(GetProcessHeap(), 0, t->idx); }
        t->keys = (uint32_t *)HeapAlloc(GetProcessHeap(), 0, cap * 4);
        t->idx = (int32_t *)HeapAlloc(GetProcessHeap(), 0, cap * 4);
        if (!t->keys || !t->idx) { t->keys = NULL; t->self = NULL; return 0; }
        t->mask = cap - 1;
    }
    memset(t->idx, 0xFF, (t->mask + 1) * 4);
    for (int i = 0; i < count; i++) {
        uint32_t k = *(const uint32_t *)(data + i * 0x18 + 4);
        uint32_t h = fhash(k) & t->mask;
        while (t->idx[h] >= 0 && t->keys[h] != k) h = (h + 1) & t->mask;
        if (t->idx[h] < 0) { t->keys[h] = k; t->idx[h] = i; }
    }
    t->self = self; t->data = data; t->count = count;
    g_find_builds++;
    return 1;
}

static int __fastcall h_find(void *self, void *edx, uint32_t id, int exact)
{
    const uint8_t *o = (const uint8_t *)self;
    int count = *(const int *)(o + 0x18);
    uint8_t *data = *(uint8_t *const *)(o + 0x1C);
    if (!exact || count < 64 || !data) { g_find_orig++; return o_find(self, edx, id, exact); }
    DWORD now = GetTickCount();
    ftab_t *t = NULL, *lru = &g_ft[0];
    for (int s = 0; s < FT_SLOTS; s++) {
        if (g_ft[s].self == self) { t = &g_ft[s]; break; }
        if (g_ft[s].used < lru->used) lru = &g_ft[s];
    }
    if (!t) { t = lru; t->self = NULL; }
    if (t->self != self || t->data != data || t->count != count || now - t->tick > 200)
        if (!ft_build(t, self, data, count)) { g_find_orig++; return o_find(self, edx, id, exact); }
    t->tick = now; t->used = now;
    uint32_t h = fhash(id) & t->mask;
    while (t->idx[h] >= 0) {
        if (t->keys[h] == id) {
            int i = t->idx[h];
            if (i < count && *(const uint32_t *)(data + i * 0x18 + 4) == id) { g_find_hits++; return i; }
            t->self = NULL; g_find_orig++;
            return o_find(self, edx, id, exact);
        }
        h = (h + 1) & t->mask;
    }
    g_find_miss++;
    return -1;
}

static void apply_object_editor_hooks(void)
{
    static const uint8_t find_orig[14] = { 0x55, 0x8B, 0xEC, 0x83, 0xEC, 0x14, 0x53, 0x56, 0x57, 0x8B, 0xF9, 0x8B, 0x47, 0x18 };
    uint8_t *fn = (uint8_t *)0x005B4FD0;
    if ((uintptr_t)GetModuleHandleA(NULL) != 0x400000 || IsBadReadPtr(fn, 0x80) || memcmp(fn, find_orig, sizeof find_orig))
        return;
    if (memcmp(fn + 0x2D, "\x39\x54\x31\x04", 4) || memcmp(fn + 0x6D, "\x83\xC6\x18", 3))
        return;
    uint8_t *t = (uint8_t *)VirtualAlloc(NULL, 32, MEM_COMMIT | MEM_RESERVE, PAGE_EXECUTE_READWRITE);
    if (!t)
        return;
    memcpy(t, fn, 6);
    t[6] = 0xE9;
    *(int32_t *)(t + 7) = (int32_t)((fn + 6) - (t + 11));
    FlushInstructionCache(GetCurrentProcess(), t, 11);
    o_find = (find_fn)(void *)t;
    if (patch_jmp(fn, (void *)h_find))
        g_patches_applied |= 1 << 9;
}

typedef uint32_t(__fastcall *nth_fn)(void *self, void *edx, int i, int filter);
typedef int(__fastcall *cnt_fn)(void *self, void *edx, int filter);
typedef void(__fastcall *rebuild_fn)(void *self, void *edx);
static nth_fn o_nth;
static cnt_fn o_cnt;
static rebuild_fn o_rebuild;
static volatile LONG g_rb_hits, g_rb_builds;

static uint32_t __fastcall h_nth(void *self, void *edx, int i, int filter)
{
    if (filter != -1 && filter != 0)
        return o_nth(self, edx, i, filter);
    const uint8_t *t = (const uint8_t *)self;
    if (i < 0 || i >= *(const int *)(t + 0x18))
        return 0;
    return *(const uint32_t *)(*(uint8_t *const *)(t + 0x1C) + i * 0x18 + 4);
}

static int __fastcall h_cnt(void *self, void *edx, int filter)
{
    if (filter != -1 && filter != 0)
        return o_cnt(self, edx, filter);
    int n = *(const int *)((const uint8_t *)self + 0x18);
    return n > 0 ? n : 0;
}

typedef struct { void *self; uint32_t *ids; int n, cap, out_n; void *out_p; DWORD used; } rb_cache_t;
static rb_cache_t g_rb[8];
static uint32_t *g_rb_tmp;
static int g_rb_tmpcap;

static int rb_grow(uint32_t **buf, int *cap, int need)
{
    if (need <= *cap)
        return 1;
    int nc = *cap ? *cap : 256;
    while (nc < need) nc *= 2;
    uint32_t *nb = *buf ? (uint32_t *)HeapReAlloc(GetProcessHeap(), 0, *buf, nc * 4) : (uint32_t *)HeapAlloc(GetProcessHeap(), 0, nc * 4);
    if (!nb)
        return 0;
    *buf = nb;
    *cap = nc;
    return 1;
}

static int rb_collect(const uint8_t *s)
{
    int n = 0;
    for (int k = 0; k < 2; k++) {
        const uint8_t *t = *(uint8_t *const *)(s + 0x1DC - k * 8);
        if (!*(void *const *)(s + 0x1D8 - k * 8) || !t)
            continue;
        int cnt = *(const int *)(t + 0x18);
        const uint8_t *d = *(uint8_t *const *)(t + 0x1C);
        if (cnt <= 0)
            continue;
        if (!d || !rb_grow(&g_rb_tmp, &g_rb_tmpcap, n + cnt + 1))
            return -1;
        g_rb_tmp[n++] = 0xFFFFFFFFu - k;
        for (int i = 0; i < cnt; i++)
            g_rb_tmp[n++] = *(const uint32_t *)(d + i * 0x18 + 4);
    }
    return n;
}

static void __fastcall h_rebuild(void *self, void *edx)
{
    const uint8_t *s = (const uint8_t *)self;
    int n = rb_collect(s);
    rb_cache_t *c = NULL, *lru = &g_rb[0];
    for (int i = 0; i < 8; i++) {
        if (g_rb[i].self == self) { c = &g_rb[i]; break; }
        if (g_rb[i].used < lru->used) lru = &g_rb[i];
    }
    if (n >= 0 && c && c->n == n && !memcmp(c->ids, g_rb_tmp, n * 4) &&
        *(const int *)(s + 0x1E4) == c->out_n && *(void *const *)(s + 0x1E8) == c->out_p) {
        c->used = GetTickCount();
        g_rb_hits++;
        return;
    }
    o_rebuild(self, edx);
    g_rb_builds++;
    if (!c)
        c = lru;
    c->self = NULL;
    if (n < 0 || !rb_grow(&c->ids, &c->cap, n))
        return;
    memcpy(c->ids, g_rb_tmp, n * 4);
    c->n = n;
    c->out_n = *(const int *)(s + 0x1E4);
    c->out_p = *(void *const *)(s + 0x1E8);
    c->used = GetTickCount();
    c->self = self;
}

static void *copy_tramp(uint8_t *fn, int n)
{
    uint8_t *t = (uint8_t *)VirtualAlloc(NULL, 32, MEM_COMMIT | MEM_RESERVE, PAGE_EXECUTE_READWRITE);
    if (!t)
        return NULL;
    memcpy(t, fn, n);
    t[n] = 0xE9;
    *(int32_t *)(t + n + 1) = (int32_t)((fn + n) - (t + n + 5));
    FlushInstructionCache(GetCurrentProcess(), t, n + 5);
    return t;
}

static void apply_custom_list_hooks(void)
{
    uint8_t *f_match = (uint8_t *)0x005B6630, *f_nth = (uint8_t *)0x005B66C0, *f_cnt = (uint8_t *)0x005B6660, *f_rb = (uint8_t *)0x00516A50;
    static const uint8_t match[15] = { 0x55, 0x8B, 0xEC, 0x8B, 0x45, 0x0C, 0x83, 0xF8, 0xFF, 0x74, 0x19, 0x85, 0xC0, 0x74, 0x15 };
    static const uint8_t nth[18] = { 0x55, 0x8B, 0xEC, 0x83, 0xEC, 0x08, 0x53, 0x56, 0x8B, 0xF1, 0x8B, 0x46, 0x18, 0x33, 0xDB, 0x57, 0x33, 0xFF };
    static const uint8_t cnt[10] = { 0x55, 0x8B, 0xEC, 0x51, 0x57, 0x8B, 0xF9, 0x8B, 0x47, 0x18 };
    static const uint8_t rb[34] = { 0x55, 0x8B, 0xEC, 0x83, 0xEC, 0x14, 0x53, 0x56, 0x57, 0x8D, 0xB9, 0xE0, 0x01, 0x00, 0x00,
                                    0x8D, 0x99, 0xDC, 0x01, 0x00, 0x00, 0x89, 0x4D, 0xF0, 0xC7, 0x47, 0x04, 0x00, 0x00, 0x00, 0x00,
                                    0x89, 0x5D, 0xFC };
    if ((uintptr_t)GetModuleHandleA(NULL) != 0x400000 || IsBadReadPtr(f_rb, 0x40) || IsBadReadPtr(f_match, 0x140))
        return;
    int nth_ok = !memcmp(f_match, match, sizeof match) && !memcmp(f_nth, nth, sizeof nth) &&
                 f_nth[0x2D] == 0xE8 && f_nth + 0x32 + *(int32_t *)(f_nth + 0x2E) == f_match &&
                 !memcmp(f_nth + 0x70, "\x8B\x44\xC1\x04", 4);
    int cnt_ok = !memcmp(f_match, match, sizeof match) && !memcmp(f_cnt, cnt, sizeof cnt) &&
                 f_cnt[0x2D] == 0xE8 && f_cnt + 0x32 + *(int32_t *)(f_cnt + 0x2E) == f_match;
    if (nth_ok && cnt_ok) {
        o_nth = (nth_fn)copy_tramp(f_nth, 6);
        o_cnt = (cnt_fn)copy_tramp(f_cnt, 5);
        if (o_nth && o_cnt && patch_jmp(f_nth, (void *)h_nth) && patch_jmp(f_cnt, (void *)h_cnt))
            g_patches_applied |= 1 << 13;
    }
    if (!memcmp(f_rb, rb, sizeof rb) && !memcmp(f_rb + 0x22, "\xC7\x45\xF4\x02\x00\x00\x00", 7) &&
        f_rb[0x47] == 0xE8 && f_rb + 0x4C + *(int32_t *)(f_rb + 0x48) == f_cnt &&
        f_rb[0x65] == 0xE8 && f_rb + 0x6A + *(int32_t *)(f_rb + 0x66) == f_nth) {
        o_rebuild = (rebuild_fn)copy_tramp(f_rb, 6);
        if (o_rebuild && patch_jmp(f_rb, (void *)h_rebuild))
            g_patches_applied |= 1 << 14;
    }
}

static volatile LONG g_resize_skipped;
static HWND g_rs_hwnd;
static int g_rs_w, g_rs_h;

typedef void(__attribute__((thiscall)) * combo_setsize_fn)(void *self, int w, int h);

static void __stdcall combo_resize(void *obj, int w, int h)
{
    HWND hwnd = *(HWND *)((uint8_t *)obj + 0x14);
    int cap = *(int *)((uint8_t *)obj + 0x38);
    if (cap > 0 && cap <= 4096 && h < cap)
        h = cap;
    if (hwnd == g_rs_hwnd && w == g_rs_w && h == g_rs_h) {
        InterlockedIncrement(&g_resize_skipped);
        return;
    }
    g_rs_hwnd = hwnd; g_rs_w = w; g_rs_h = h;
    ((combo_setsize_fn)0x0042D5F0)(obj, w, h);
}

static void apply_combo_resize_patch(void)
{
    static const uint8_t orig[8] = { 0x8B, 0x55, 0xFC, 0x56, 0x52, 0x8B, 0xCF, 0xE8 };
    uint8_t *site = (uint8_t *)0x0042E7D9;
    if ((uintptr_t)GetModuleHandleA(NULL) != 0x400000 || IsBadReadPtr(site, 12) || memcmp(site, orig, 8) ||
        site + 12 + *(int32_t *)(site + 8) != (uint8_t *)0x0042D5F0 || memcmp(site - 0x0F, "\x8D\x45\xF8\x50", 4))
        return;
    uint8_t *t = (uint8_t *)VirtualAlloc(NULL, 32, MEM_COMMIT | MEM_RESERVE, PAGE_EXECUTE_READWRITE);
    if (!t) return;
    uint8_t *w = t;
    *w++ = 0x8B; *w++ = 0x55; *w++ = 0xFC; *w++ = 0x56; *w++ = 0x52; *w++ = 0x57;
    *w++ = 0xE8; *(int32_t *)w = (int32_t)((uint8_t *)combo_resize - (w + 4)); w += 4;
    *w++ = 0xE9; *(int32_t *)w = (int32_t)((site + 12) - (w + 4)); w += 4;
    FlushInstructionCache(GetCurrentProcess(), t, 32);
    DWORD old;
    if (!VirtualProtect(site, 12, PAGE_EXECUTE_READWRITE, &old)) return;
    site[0] = 0xE9; *(int32_t *)(site + 1) = (int32_t)(t - (site + 5));
    memset(site + 5, 0x90, 7);
    VirtualProtect(site, 12, old, &old);
    FlushInstructionCache(GetCurrentProcess(), site, 12);
    g_patches_applied |= 1 << 10;
}

#define CWP_MAX 4
#define CWP_SUBCLASS_ID 0x4B464357
typedef struct { HOOKPROC proc; int active, on_create, nmsgs; UINT msgs[4]; } cwp_emu_t;
static cwp_emu_t g_cwp[CWP_MAX];
static int g_ncwp, g_cwp_subclass;
static DWORD g_cwp_tid;
static HHOOK g_cwp_cbt;
static volatile LONG g_cwp_delivered;
typedef HHOOK(WINAPI *swh_fn)(int, HOOKPROC, HINSTANCE, DWORD);
typedef BOOL(WINAPI *uwh_fn)(HHOOK);
static swh_fn o_swh;
static uwh_fn o_uwh;

static const uint8_t sig_darkmode_cwp[31] = {
    0x83, 0x7C, 0x24, 0x04, 0x00, 0x56, 0x8B, 0x74, 0x24, 0x10, 0x7C, 0x24, 0x85, 0xF6, 0x74, 0x20,
    0x8B, 0x46, 0x08, 0x83, 0xE8, 0x01, 0x74, 0x07, 0x2D, 0x0F, 0x01, 0x00, 0x00, 0x75, 0x11 };
static const uint8_t sig_tesh_cwp[37] = {
    0x55, 0x8B, 0xEC, 0x83, 0x7D, 0x08, 0x00, 0x56, 0x8B, 0x75, 0x10, 0x0F, 0x8C, 0x8C, 0x00, 0x00,
    0x00, 0x8B, 0x46, 0x08, 0x83, 0xF8, 0x4E, 0x77, 0x5D, 0x74, 0x40, 0x83, 0xF8, 0x10, 0x74, 0x20,
    0x83, 0xF8, 0x47, 0x75, 0x78 };
static const uint8_t sig_tesh_cwp2[7] = { 0x3D, 0x33, 0x01, 0x00, 0x00, 0x75, 0x20 };

static int cwp_known(HOOKPROC fn, cwp_emu_t *e)
{
    const uint8_t *p = (const uint8_t *)fn;
    if (IsBadReadPtr(p, 0x80))
        return 0;
    if (!memcmp(p, sig_darkmode_cwp, sizeof sig_darkmode_cwp)) {
        e->on_create = 1;
        return 1;
    }
    if (!memcmp(p, sig_tesh_cwp, sizeof sig_tesh_cwp) && !memcmp(p + 0x76, sig_tesh_cwp2, sizeof sig_tesh_cwp2)) {
        static const UINT m[4] = { WM_WINDOWPOSCHANGED, WM_CLOSE, WM_NOTIFY, WM_CTLCOLOREDIT };
        memcpy(e->msgs, m, sizeof m);
        e->nmsgs = 4;
        return 1;
    }
    return 0;
}

static void cwp_deliver(HWND h, UINT m, WPARAM w, LPARAM l, int create)
{
    CWPSTRUCT c = { l, w, m, h };
    for (int i = 0; i < g_ncwp; i++) {
        cwp_emu_t *e = &g_cwp[i];
        int want = create ? e->on_create : 0;
        for (int k = 0; !create && k < e->nmsgs; k++)
            want |= e->msgs[k] == m;
        if (e->active && want) {
            e->proc(HC_ACTION, 1, (LPARAM)&c);
            g_cwp_delivered++;
        }
    }
}

static LRESULT CALLBACK cwp_subclass(HWND h, UINT m, WPARAM w, LPARAM l, UINT_PTR id, DWORD_PTR ref)
{
    (void)ref;
    (void)id;
    if (m == WM_WINDOWPOSCHANGED || m == WM_CLOSE || m == WM_NOTIFY || m == WM_CTLCOLOREDIT)
        cwp_deliver(h, m, w, l, 0);
    return fs_def(h, m, w, l);
}

static LRESULT CALLBACK cwp_cbt(int code, WPARAM w, LPARAM l)
{
    if (code == HCBT_CREATEWND) {
        HWND h = (HWND)w;
        cwp_deliver(h, WM_CREATE, 0, (LPARAM)((CBT_CREATEWNDW *)l)->lpcs, 1);
        if (g_cwp_subclass)
            fs_set(h, cwp_subclass, CWP_SUBCLASS_ID, 0);
    }
    return CallNextHookEx(g_cwp_cbt, code, w, l);
}

static BOOL CALLBACK cwp_enum_child(HWND h, LPARAM l)
{
    (void)l;
    fs_set(h, cwp_subclass, CWP_SUBCLASS_ID, 0);
    return TRUE;
}

static BOOL CALLBACK cwp_enum_top(HWND h, LPARAM l)
{
    cwp_enum_child(h, l);
    EnumChildWindows(h, cwp_enum_child, 0);
    return TRUE;
}

static HHOOK WINAPI h_set_hook(int id, HOOKPROC fn, HINSTANCE mod, DWORD tid)
{
    cwp_emu_t e = { 0 };
    if (id == WH_CALLWNDPROC && fn && tid == GetCurrentThreadId() && (!g_cwp_tid || tid == g_cwp_tid) &&
        g_ncwp < CWP_MAX && cwp_known(fn, &e)) {
        if (!g_cwp_cbt) {
            g_cwp_cbt = o_swh(WH_CBT, cwp_cbt, NULL, tid);
            g_cwp_tid = tid;
        }
        if (g_cwp_cbt) {
            HMODULE pin;
            GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_PIN, (LPCWSTR)(void *)fn, &pin);
            if (e.on_create && pin && fs_patch_iat(pin) >= 2)
                g_patches_applied |= 1 << 12;
            e.proc = fn;
            e.active = 1;
            g_cwp[g_ncwp] = e;
            if (e.nmsgs && !g_cwp_subclass) {
                g_cwp_subclass = 1;
                EnumThreadWindows(tid, cwp_enum_top, 0);
            }
            return (HHOOK)&g_cwp[g_ncwp++];
        }
    }
    return o_swh(id, fn, mod, tid);
}

static BOOL WINAPI h_unhook(HHOOK h)
{
    for (int i = 0; i < g_ncwp; i++)
        if (h == (HHOOK)&g_cwp[i]) {
            g_cwp[i].active = 0;
            return TRUE;
        }
    return o_uwh(h);
}

static void *hotpatch(void *fn, void *hook)
{
    uint8_t *p = (uint8_t *)fn;
    if (!p || IsBadReadPtr(p - 5, 8) || p[0] != 0x8B || p[1] != 0xFF)
        return NULL;
    for (int i = 1; i <= 5; i++)
        if (p[-i] != 0xCC && p[-i] != 0x90)
            return NULL;
    DWORD old;
    if (!VirtualProtect(p - 5, 7, PAGE_EXECUTE_READWRITE, &old))
        return NULL;
    p[-5] = 0xE9;
    *(int32_t *)(p - 4) = (int32_t)((uint8_t *)hook - p);
    *(volatile uint16_t *)p = 0xF9EB;
    VirtualProtect(p - 5, 7, old, &old);
    FlushInstructionCache(GetCurrentProcess(), p - 5, 7);
    return p + 2;
}

static void apply_cwp_hook_filter(void)
{
    HMODULE u = GetModuleHandleA("user32.dll");
    void *s = u ? (void *)GetProcAddress(u, "SetWindowsHookExW") : NULL;
    void *x = u ? (void *)GetProcAddress(u, "UnhookWindowsHookEx") : NULL;
    const uint8_t *ps = (const uint8_t *)s, *px = (const uint8_t *)x;
    if (!ps || !px || IsBadReadPtr(ps - 5, 8) || IsBadReadPtr(px - 5, 8) || ps[0] != 0x8B || ps[1] != 0xFF || px[0] != 0x8B || px[1] != 0xFF)
        return;
    o_uwh = (uwh_fn)hotpatch(x, (void *)h_unhook);
    if (!o_uwh)
        return;
    o_swh = (swh_fn)hotpatch(s, (void *)h_set_hook);
    if (o_swh)
        g_patches_applied |= 1 << 11;
}

#define NEG_SLOTS 16384
#define NEG_TTL 3000
static struct { uint64_t key; DWORD tick, err, gen; } g_neg[NEG_SLOTS];
static SRWLOCK g_neg_lock = SRWLOCK_INIT;
static volatile LONG g_neg_gen = 1, g_neg_hits, g_neg_adds;

static uint64_t neg_mix(uint64_t h, unsigned c)
{
    if (c == '/')
        c = '\\';
    else if (c >= 'A' && c <= 'Z')
        c += 32;
    return (h ^ c) * 1099511628211ull;
}

static uint64_t neg_cwd(void)
{
    WCHAR d[MAX_PATH];
    DWORD n = GetCurrentDirectoryW(MAX_PATH, d);
    uint64_t h = 1469598103934665603ull;
    for (DWORD i = 0; i < n && i < MAX_PATH; i++)
        h = neg_mix(h, d[i]);
    return neg_mix(h, '\\');
}

static int neg_abs(unsigned c0, unsigned c1, unsigned c2)
{
    return (c1 == ':' && (c2 == '\\' || c2 == '/')) || ((c0 == '\\' || c0 == '/') && (c1 == '\\' || c1 == '/'));
}

static uint64_t neg_key_a(const char *p)
{
    const unsigned char *u = (const unsigned char *)p;
    uint64_t h = u[0] && neg_abs(u[0], u[1], u[1] ? u[2] : 0) ? 1469598103934665603ull : neg_cwd();
    for (; *u; u++)
        h = neg_mix(h, *u);
    return h | 1;
}

static uint64_t neg_key_w(const WCHAR *p)
{
    uint64_t h = p[0] && neg_abs(p[0], p[1], p[1] ? p[2] : 0) ? 1469598103934665603ull : neg_cwd();
    for (; *p; p++)
        h = neg_mix(h, *p);
    return h | 1;
}

static int neg_find(uint64_t k, DWORD *err)
{
    DWORD now = GetTickCount(), gen = (DWORD)g_neg_gen;
    int hit = 0;
    AcquireSRWLockShared(&g_neg_lock);
    for (unsigned i = 0; i < 8; i++) {
        unsigned s = (unsigned)(k + i) & (NEG_SLOTS - 1);
        if (g_neg[s].key == k) {
            if (g_neg[s].gen == gen && now - g_neg[s].tick < NEG_TTL) {
                *err = g_neg[s].err;
                hit = 1;
            }
            break;
        }
    }
    ReleaseSRWLockShared(&g_neg_lock);
    if (hit)
        InterlockedIncrement(&g_neg_hits);
    return hit;
}

static void neg_add(uint64_t k, DWORD err)
{
    DWORD now = GetTickCount(), gen = (DWORD)g_neg_gen;
    AcquireSRWLockExclusive(&g_neg_lock);
    unsigned best = (unsigned)k & (NEG_SLOTS - 1);
    for (unsigned i = 0; i < 8; i++) {
        unsigned s = (unsigned)(k + i) & (NEG_SLOTS - 1);
        if (g_neg[s].key == k || g_neg[s].gen != gen || now - g_neg[s].tick >= NEG_TTL) {
            best = s;
            break;
        }
        if (now - g_neg[s].tick > now - g_neg[best].tick)
            best = s;
    }
    g_neg[best].key = k;
    g_neg[best].tick = now;
    g_neg[best].err = err;
    g_neg[best].gen = gen;
    ReleaseSRWLockExclusive(&g_neg_lock);
    InterlockedIncrement(&g_neg_adds);
}

static int neg_readonly(DWORD acc, DWORD disp)
{
    return disp == OPEN_EXISTING && !(acc & (GENERIC_WRITE | GENERIC_ALL | FILE_WRITE_DATA | FILE_APPEND_DATA |
                                              FILE_WRITE_ATTRIBUTES | FILE_WRITE_EA | DELETE | WRITE_DAC | WRITE_OWNER));
}

static int neg_missing(DWORD e)
{
    return e == ERROR_FILE_NOT_FOUND || e == ERROR_PATH_NOT_FOUND;
}

#define DIR_SLOTS 2048
#define DIR_MAX_NAMES 16384
typedef struct { uint64_t key; DWORD tick, gen; int missing, cap; uint64_t *names; } dir_ent;
static dir_ent g_dirs[DIR_SLOTS];
static volatile LONG g_dir_hits, g_dir_lists;

static uint64_t dir_hash(const WCHAR *s, int n)
{
    WCHAR buf[MAX_PATH * 2];
    uint64_t h = 1469598103934665603ull;
    if (n <= 0 || n >= MAX_PATH * 2)
        return 0;
    memcpy(buf, s, n * sizeof(WCHAR));
    CharLowerBuffW(buf, n);
    for (int i = 0; i < n; i++)
        h = (h ^ (buf[i] == '/' ? '\\' : buf[i])) * 1099511628211ull;
    return h | 1;
}

static int set_has(const uint64_t *set, int cap, uint64_t k)
{
    for (unsigned i = (unsigned)k & (cap - 1);; i = (i + 1) & (cap - 1)) {
        if (!set[i])
            return 0;
        if (set[i] == k)
            return 1;
    }
}

static void set_put(uint64_t *set, int cap, uint64_t k)
{
    unsigned i = (unsigned)k & (cap - 1);
    while (set[i] && set[i] != k)
        i = (i + 1) & (cap - 1);
    set[i] = k;
}

static int dir_lookup(uint64_t dk, uint64_t nk, DWORD *err)
{
    DWORD now = GetTickCount(), gen = (DWORD)g_neg_gen;
    int r = -1;
    AcquireSRWLockShared(&g_neg_lock);
    for (unsigned i = 0; i < 8; i++) {
        dir_ent *e = &g_dirs[((unsigned)dk + i) & (DIR_SLOTS - 1)];
        if (e->key != dk)
            continue;
        if (e->gen == gen && now - e->tick < NEG_TTL) {
            if (e->missing) {
                *err = ERROR_PATH_NOT_FOUND;
                r = 1;
            } else if (!set_has(e->names, e->cap, nk)) {
                *err = ERROR_FILE_NOT_FOUND;
                r = 1;
            } else
                r = 0;
        }
        break;
    }
    ReleaseSRWLockShared(&g_neg_lock);
    return r;
}

static void dir_store(uint64_t dk, int missing, uint64_t *names, int cap, DWORD gen)
{
    DWORD now = GetTickCount();
    uint64_t *drop = NULL;
    AcquireSRWLockExclusive(&g_neg_lock);
    dir_ent *best = &g_dirs[(unsigned)dk & (DIR_SLOTS - 1)];
    for (unsigned i = 0; i < 8; i++) {
        dir_ent *e = &g_dirs[((unsigned)dk + i) & (DIR_SLOTS - 1)];
        if (e->key == dk || !e->key || e->gen != (DWORD)g_neg_gen || now - e->tick >= NEG_TTL) {
            best = e;
            break;
        }
        if (now - e->tick > now - best->tick)
            best = e;
    }
    drop = best->names;
    best->key = dk;
    best->tick = now;
    best->gen = gen;
    best->missing = missing;
    best->names = names;
    best->cap = cap;
    ReleaseSRWLockExclusive(&g_neg_lock);
    free(drop);
}

static void dir_add_name(uint64_t *set, int cap, const WCHAR *nm)
{
    int n = (int)wcslen(nm);
    if (n)
        set_put(set, cap, dir_hash(nm, n));
}

static int dir_list(const WCHAR *dir, int dlen, uint64_t dk)
{
    WCHAR pat[MAX_PATH * 2 + 4];
    WIN32_FIND_DATAW fd;
    DWORD gen = (DWORD)g_neg_gen;
    if (dlen + 3 >= (int)(sizeof pat / sizeof *pat))
        return 0;
    memcpy(pat, dir, dlen * sizeof(WCHAR));
    pat[dlen] = '\\';
    pat[dlen + 1] = '*';
    pat[dlen + 2] = 0;
    InterlockedIncrement(&g_dir_lists);
    HANDLE h = FindFirstFileExW(pat, FindExInfoStandard, &fd, FindExSearchNameMatch, NULL, 2);
    if (h == INVALID_HANDLE_VALUE) {
        DWORD e = GetLastError();
        if (e == ERROR_PATH_NOT_FOUND || e == ERROR_FILE_NOT_FOUND) {
            dir_store(dk, 1, NULL, 0, gen);
            return 1;
        }
        return 0;
    }
    int cap = 64, n = 0;
    uint64_t *set = calloc(cap, sizeof *set);
    do {
        if (!set)
            break;
        if ((n + 2) * 2 > cap) {
            if (cap >= DIR_MAX_NAMES * 2) {
                free(set);
                set = NULL;
                break;
            }
            uint64_t *big = calloc(cap * 2, sizeof *big);
            if (!big) {
                free(set);
                set = NULL;
                break;
            }
            for (int i = 0; i < cap; i++)
                if (set[i])
                    set_put(big, cap * 2, set[i]);
            free(set);
            set = big;
            cap *= 2;
        }
        dir_add_name(set, cap, fd.cFileName);
        if (fd.cAlternateFileName[0])
            dir_add_name(set, cap, fd.cAlternateFileName);
        n += 2;
    } while (FindNextFileW(h, &fd));
    FindClose(h);
    if (!set)
        return 0;
    dir_store(dk, 0, set, cap, gen);
    return 1;
}

static int dir_check_w(const WCHAR *p, DWORD *err)
{
    WCHAR full[MAX_PATH * 2];
    if (!p || !p[0] || ((p[0] == '\\' || p[0] == '/') && (p[1] == '\\' || p[1] == '/')))
        return 0;
    DWORD n = GetFullPathNameW(p, MAX_PATH * 2, full, NULL);
    if (!n || n >= MAX_PATH * 2 || full[1] != ':' || full[2] != '\\')
        return 0;
    for (DWORD i = 3; i < n; i++)
        if (full[i] == '*' || full[i] == '?' || full[i] == ':')
            return 0;
    int last = (int)n - 1;
    while (last > 2 && full[last] != '\\')
        last--;
    if (last < 2 || last == (int)n - 1)
        return 0;
    int dlen = last == 2 ? 2 : last;
    uint64_t dk = dir_hash(full, dlen), nk = dir_hash(full + last + 1, (int)n - last - 1);
    if (!dk || !nk)
        return 0;
    int r = dir_lookup(dk, nk, err);
    if (r < 0 && dir_list(full, dlen, dk))
        r = dir_lookup(dk, nk, err);
    if (r == 1)
        InterlockedIncrement(&g_dir_hits);
    return r == 1;
}

static int dir_check_a(const char *p, DWORD *err)
{
    WCHAR w[MAX_PATH * 2];
    if (!p || !MultiByteToWideChar(CP_ACP, 0, p, -1, w, MAX_PATH * 2))
        return 0;
    return dir_check_w(w, err);
}

static HANDLE WINAPI h_create_a(LPCSTR p, DWORD acc, DWORD share, LPSECURITY_ATTRIBUTES sa, DWORD disp, DWORD fl, HANDLE tpl)
{
    DWORD err;
    if (!p || !neg_readonly(acc, disp)) {
        InterlockedIncrement(&g_neg_gen);
        return CreateFileA(p, acc, share, sa, disp, fl, tpl);
    }
    uint64_t k = neg_key_a(p);
    if (dir_check_a(p, &err) || neg_find(k, &err)) {
        SetLastError(err);
        return INVALID_HANDLE_VALUE;
    }
    HANDLE h = CreateFileA(p, acc, share, sa, disp, fl, tpl);
    if (h == INVALID_HANDLE_VALUE && neg_missing(err = GetLastError())) {
        neg_add(k, err);
        SetLastError(err);
    }
    return h;
}

static HANDLE WINAPI h_create_w(LPCWSTR p, DWORD acc, DWORD share, LPSECURITY_ATTRIBUTES sa, DWORD disp, DWORD fl, HANDLE tpl)
{
    DWORD err;
    if (!p || !neg_readonly(acc, disp)) {
        InterlockedIncrement(&g_neg_gen);
        return CreateFileW(p, acc, share, sa, disp, fl, tpl);
    }
    uint64_t k = neg_key_w(p);
    if (dir_check_w(p, &err) || neg_find(k, &err)) {
        SetLastError(err);
        return INVALID_HANDLE_VALUE;
    }
    HANDLE h = CreateFileW(p, acc, share, sa, disp, fl, tpl);
    if (h == INVALID_HANDLE_VALUE && neg_missing(err = GetLastError())) {
        neg_add(k, err);
        SetLastError(err);
    }
    return h;
}

static DWORD WINAPI h_attr_a(LPCSTR p)
{
    DWORD err;
    uint64_t k = p ? neg_key_a(p) : 0;
    if (k && (dir_check_a(p, &err) || neg_find(k, &err))) {
        SetLastError(err);
        return INVALID_FILE_ATTRIBUTES;
    }
    DWORD r = GetFileAttributesA(p);
    if (r == INVALID_FILE_ATTRIBUTES && k && neg_missing(err = GetLastError())) {
        neg_add(k, err);
        SetLastError(err);
    }
    return r;
}

static BOOL WINAPI h_attrex_w(LPCWSTR p, GET_FILEEX_INFO_LEVELS lv, LPVOID out)
{
    DWORD err;
    uint64_t k = p ? neg_key_w(p) : 0;
    if (k && (dir_check_w(p, &err) || neg_find(k, &err))) {
        SetLastError(err);
        return FALSE;
    }
    BOOL r = GetFileAttributesExW(p, lv, out);
    if (!r && k && neg_missing(err = GetLastError())) {
        neg_add(k, err);
        SetLastError(err);
    }
    return r;
}

static BOOL WINAPI h_mkdir_a(LPCSTR p, LPSECURITY_ATTRIBUTES sa)
{
    BOOL r = CreateDirectoryA(p, sa);
    InterlockedIncrement(&g_neg_gen);
    return r;
}

static BOOL WINAPI h_rmdir_a(LPCSTR p)
{
    BOOL r = RemoveDirectoryA(p);
    InterlockedIncrement(&g_neg_gen);
    return r;
}

static BOOL WINAPI h_del_a(LPCSTR p)
{
    BOOL r = DeleteFileA(p);
    InterlockedIncrement(&g_neg_gen);
    return r;
}

static BOOL WINAPI h_copy_a(LPCSTR a, LPCSTR b, BOOL fail)
{
    BOOL r = CopyFileA(a, b, fail);
    InterlockedIncrement(&g_neg_gen);
    return r;
}

static BOOL WINAPI h_move_a(LPCSTR a, LPCSTR b)
{
    BOOL r = MoveFileA(a, b);
    InterlockedIncrement(&g_neg_gen);
    return r;
}

static void *WINAPI h_open_xmidi(DWORD flags)
{
    (void)flags;
    return NULL;
}

static int iat_hook(HMODULE mod, const char *name, void *addr, void *to)
{
    uint8_t *b = (uint8_t *)mod;
    if (!b || !to)
        return 0;
    IMAGE_NT_HEADERS *nt = (IMAGE_NT_HEADERS *)(b + ((IMAGE_DOS_HEADER *)b)->e_lfanew);
    IMAGE_DATA_DIRECTORY dd = nt->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_IMPORT];
    int n = 0;
    if (!dd.VirtualAddress)
        return 0;
    for (IMAGE_IMPORT_DESCRIPTOR *d = (IMAGE_IMPORT_DESCRIPTOR *)(b + dd.VirtualAddress); d->Name; d++) {
        IMAGE_THUNK_DATA32 *iat = (IMAGE_THUNK_DATA32 *)(b + d->FirstThunk);
        IMAGE_THUNK_DATA32 *nm = d->OriginalFirstThunk ? (IMAGE_THUNK_DATA32 *)(b + d->OriginalFirstThunk) : NULL;
        for (; iat->u1.Function; iat++, nm = nm ? nm + 1 : NULL) {
            int match = addr && (void *)(uintptr_t)iat->u1.Function == addr;
            if (!match && nm && !IMAGE_SNAP_BY_ORDINAL32(nm->u1.Ordinal))
                match = !strcmp((const char *)((IMAGE_IMPORT_BY_NAME *)(b + nm->u1.AddressOfData))->Name, name);
            DWORD old;
            if (match && VirtualProtect(&iat->u1.Function, 4, PAGE_READWRITE, &old)) {
                iat->u1.Function = (DWORD)(uintptr_t)to;
                VirtualProtect(&iat->u1.Function, 4, old, &old);
                n++;
            }
        }
    }
    return n;
}

static void apply_startup_io(void)
{
    HMODULE k32 = GetModuleHandleA("kernel32.dll");
    HMODULE mods[3] = { GetModuleHandleA(NULL), GetModuleHandleA("storm.dll"), GetModuleHandleA("virtual_mpq.dll") };
    int n = 0;
    for (int i = 0; i < 3; i++) {
        if (!mods[i])
            continue;
        n += iat_hook(mods[i], "CreateFileA", (void *)GetProcAddress(k32, "CreateFileA"), (void *)h_create_a);
        n += iat_hook(mods[i], "CreateFileW", (void *)GetProcAddress(k32, "CreateFileW"), (void *)h_create_w);
        n += iat_hook(mods[i], "GetFileAttributesA", (void *)GetProcAddress(k32, "GetFileAttributesA"), (void *)h_attr_a);
        n += iat_hook(mods[i], "GetFileAttributesExW", (void *)GetProcAddress(k32, "GetFileAttributesExW"), (void *)h_attrex_w);
        n += iat_hook(mods[i], "CreateDirectoryA", (void *)GetProcAddress(k32, "CreateDirectoryA"), (void *)h_mkdir_a);
        n += iat_hook(mods[i], "RemoveDirectoryA", (void *)GetProcAddress(k32, "RemoveDirectoryA"), (void *)h_rmdir_a);
        n += iat_hook(mods[i], "DeleteFileA", (void *)GetProcAddress(k32, "DeleteFileA"), (void *)h_del_a);
        n += iat_hook(mods[i], "CopyFileA", (void *)GetProcAddress(k32, "CopyFileA"), (void *)h_copy_a);
        n += iat_hook(mods[i], "MoveFileA", (void *)GetProcAddress(k32, "MoveFileA"), (void *)h_move_a);
    }
    if (n)
        g_patches_applied |= 1 << 15;
    HMODULE mss = GetModuleHandleA("mss32.dll");
    void *xm = mss ? (void *)GetProcAddress(mss, "_AIL_open_XMIDI_driver@4") : NULL;
    if (xm && iat_hook(GetModuleHandleA(NULL), "_AIL_open_XMIDI_driver@4", xm, (void *)h_open_xmidi))
        g_patches_applied |= 1 << 16;
}

__declspec(dllexport) int Initialize(void)
{
    static int done;
    if (done)
        return done;

    HMODULE storm = GetModuleHandleA("storm.dll");
    if (!storm)
        return done = -1;

    uint8_t *f_alloc = (uint8_t *)GetProcAddress(storm, (LPCSTR)401);
    uint8_t *f_free = (uint8_t *)GetProcAddress(storm, (LPCSTR)403);
    uint8_t *f_size = (uint8_t *)GetProcAddress(storm, (LPCSTR)404);
    uint8_t *f_realloc = (uint8_t *)GetProcAddress(storm, (LPCSTR)405);
    if (!f_alloc || !f_free || !f_size || !f_realloc)
        return done = -2;
    if (!check_prologue(f_alloc) || !check_prologue(f_free) || !check_prologue(f_size) || !check_prologue(f_realloc))
        return done = -3;

    init_classes();
    g_pools = (pool_t *)VirtualAlloc(NULL, sizeof(pool_t) * POOL_MAX, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
    if (!g_pools)
        return done = -4;
#ifdef SFH_PROFILE
    g_prof = (prof_t *)VirtualAlloc(NULL, sizeof(prof_t) * POOL_MAX, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
    if (!g_prof)
        return done = -4;
#endif

    o_alloc = (alloc_fn)make_trampoline(f_alloc);
    o_free = (free_fn)make_trampoline(f_free);
    o_size = (size_fn)make_trampoline(f_size);
    o_realloc = (realloc_fn)make_trampoline(f_realloc);
    if (!o_alloc || !o_free || !o_size || !o_realloc)
        return done = -5;

    init_crash_log();
#ifndef SFH_PROFILE
    apply_code_patches();
    apply_var_index_hooks();
    apply_object_editor_hooks();
    apply_custom_list_hooks();
    apply_combo_resize_patch();
    apply_cwp_hook_filter();
    apply_startup_io();
#endif
#ifdef SFH_LIVE
    if (!live_init())
        return done = -4;
    HANDLE dump = CreateThread(NULL, 0, live_dump_thread, NULL, 0, NULL);
    if (dump)
        CloseHandle(dump);
#endif

    if (!patch_jmp(f_free, (void *)H_FREE) || !patch_jmp(f_size, (void *)H_SIZE) ||
        !patch_jmp(f_realloc, (void *)H_REALLOC) || !patch_jmp(f_alloc, (void *)H_ALLOC))
        return done = -6;

    HMODULE self;
    GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_PIN, (LPCSTR)&Initialize, &self);
    return done = 1;
}

__declspec(dllexport) int Stats(int which)
{
#ifdef SFH_PROFILE
    if (which == 2) {
        prof_dump();
        return 0;
    }
#endif
    if (which == 3)
        return g_patches_applied;
    if (which == 4)
        return g_idx_hits;
    if (which == 5)
        return g_idx_fallbacks;
    if (which == 6)
        return g_idx_mismatch;
    if (which == 7)
        return g_idx_builds;
    if (which == 8)
        return g_find_hits;
    if (which == 9)
        return g_find_miss;
    if (which == 10)
        return g_find_builds;
    if (which == 11)
        return g_find_orig;
    if (which == 12)
        return g_resize_skipped;
    if (which == 13)
        return g_ncwp;
    if (which == 14)
        return g_cwp_delivered;
    if (which == 15)
        return fs_wins;
    if (which == 16)
        return g_rb_hits;
    if (which == 17)
        return g_rb_builds;
    if (which == 18)
        return g_neg_hits;
    if (which == 19)
        return g_neg_adds;
    if (which == 20)
        return g_dir_hits;
    if (which == 21)
        return g_dir_lists;
    return which == 0 ? g_allocs : g_foreign_frees;
}

BOOL WINAPI DllMain(HINSTANCE h, DWORD reason, LPVOID r)
{
    (void)h; (void)reason; (void)r;
    return TRUE;
}
