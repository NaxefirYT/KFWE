#ifndef FASTSUB_H
#define FASTSUB_H
#include <windows.h>
#include <commctrl.h>
#include <stdint.h>
#include <stdlib.h>

typedef struct { SUBCLASSPROC proc; UINT_PTR id; DWORD_PTR ref; } fs_entry;
typedef struct fs_win { HWND hwnd; WNDPROC orig; int unicode, n, cap, busy, dead; fs_entry *e; } fs_win;
typedef struct fs_frame { fs_win *w; int idx; LPARAM orig_l; struct fs_frame *up; } fs_frame;

static fs_win **fs_tab;
static unsigned fs_cap, fs_used;
static DWORD fs_tls = TLS_OUT_OF_INDEXES;
static LRESULT(WINAPI *fs_comctl_def)(HWND, UINT, WPARAM, LPARAM);
static volatile LONG fs_wins;

#define FS_TOMB ((fs_win *)1)
static unsigned fs_hash(HWND h) { uint32_t x = (uint32_t)(uintptr_t)h; x ^= x >> 16; x *= 0x7FEB352Du; x ^= x >> 15; return x; }

static fs_win *fs_find(HWND h)
{
    if (!fs_tab)
        return NULL;
    for (unsigned i = fs_hash(h) & (fs_cap - 1);; i = (i + 1) & (fs_cap - 1)) {
        fs_win *w = fs_tab[i];
        if (!w)
            return NULL;
        if (w != FS_TOMB && w->hwnd == h)
            return w;
    }
}

static int fs_insert(fs_win *w)
{
    if (!fs_tab || (fs_used + 1) * 2 > fs_cap) {
        unsigned ncap = fs_cap ? fs_cap * 2 : 1024;
        fs_win **nt = (fs_win **)calloc(ncap, sizeof *nt);
        if (!nt)
            return 0;
        unsigned used = 0;
        for (unsigned i = 0; i < fs_cap; i++)
            if (fs_tab[i] && fs_tab[i] != FS_TOMB) {
                unsigned k = fs_hash(fs_tab[i]->hwnd) & (ncap - 1);
                while (nt[k]) k = (k + 1) & (ncap - 1);
                nt[k] = fs_tab[i];
                used++;
            }
        free(fs_tab);
        fs_tab = nt;
        fs_cap = ncap;
        fs_used = used;
    }
    unsigned k = fs_hash(w->hwnd) & (fs_cap - 1);
    while (fs_tab[k] && fs_tab[k] != FS_TOMB) k = (k + 1) & (fs_cap - 1);
    if (!fs_tab[k])
        fs_used++;
    fs_tab[k] = w;
    return 1;
}

static void fs_erase(fs_win *w)
{
    for (unsigned i = fs_hash(w->hwnd) & (fs_cap - 1); fs_tab[i]; i = (i + 1) & (fs_cap - 1))
        if (fs_tab[i] == w) {
            fs_tab[i] = FS_TOMB;
            return;
        }
}

static void fs_release(fs_win *w)
{
    if (w->dead && !w->busy) {
        free(w->e);
        free(w);
        InterlockedDecrement(&fs_wins);
    }
}

static LRESULT fs_next(fs_frame *f, HWND h, UINT m, WPARAM wp, LPARAM lp)
{
    fs_win *w = f->w;
    int save = f->idx;
    for (int i = save - 1; i >= 0; i--)
        if (w->e[i].proc) {
            f->idx = i;
            LRESULT r = w->e[i].proc(h, m, wp, lp, w->e[i].id, w->e[i].ref);
            f->idx = save;
            return r;
        }
    if (m == WM_SETTINGCHANGE)
        lp = f->orig_l;
    return w->unicode ? CallWindowProcW(w->orig, h, m, wp, lp) : CallWindowProcA(w->orig, h, m, wp, lp);
}

static LRESULT fs_dispatch(fs_win *w, HWND h, UINT m, WPARAM wp, LPARAM lp, LPARAM orig_l)
{
    fs_frame f = { w, w->n, orig_l, (fs_frame *)TlsGetValue(fs_tls) };
    TlsSetValue(fs_tls, &f);
    w->busy++;
    LRESULT r = fs_next(&f, h, m, wp, lp);
    w->busy--;
    TlsSetValue(fs_tls, f.up);
    if (m == WM_NCDESTROY && !w->dead) {
        w->dead = 1;
        fs_erase(w);
    }
    fs_release(w);
    return r;
}

static LRESULT CALLBACK fs_master_w(HWND h, UINT m, WPARAM wp, LPARAM lp)
{
    fs_win *w = fs_find(h);
    return w ? fs_dispatch(w, h, m, wp, lp, lp) : DefWindowProcW(h, m, wp, lp);
}

static LRESULT CALLBACK fs_master_a(HWND h, UINT m, WPARAM wp, LPARAM lp)
{
    fs_win *w = fs_find(h);
    if (!w)
        return DefWindowProcA(h, m, wp, lp);
    if (m == WM_SETTINGCHANGE && lp) {
        WCHAR buf[128];
        if (!MultiByteToWideChar(CP_ACP, 0, (LPCSTR)lp, -1, buf, 128))
            buf[0] = 0;
        return fs_dispatch(w, h, m, wp, (LPARAM)buf, lp);
    }
    return fs_dispatch(w, h, m, wp, lp, lp);
}

static BOOL WINAPI fs_set(HWND h, SUBCLASSPROC p, UINT_PTR id, DWORD_PTR ref)
{
    if (!p || !IsWindow(h) || GetWindowThreadProcessId(h, NULL) != GetCurrentThreadId())
        return FALSE;
    if (fs_tls == TLS_OUT_OF_INDEXES && (fs_tls = TlsAlloc()) == TLS_OUT_OF_INDEXES)
        return FALSE;
    fs_win *w = fs_find(h);
    if (!w) {
        w = (fs_win *)calloc(1, sizeof *w);
        if (!w)
            return FALSE;
        w->hwnd = h;
        w->unicode = IsWindowUnicode(h);
        if (!fs_insert(w)) {
            free(w);
            return FALSE;
        }
        w->orig = w->unicode ? (WNDPROC)SetWindowLongPtrW(h, GWLP_WNDPROC, (LONG_PTR)fs_master_w)
                             : (WNDPROC)SetWindowLongPtrA(h, GWLP_WNDPROC, (LONG_PTR)fs_master_a);
        if (!w->orig) {
            fs_erase(w);
            free(w);
            return FALSE;
        }
        InterlockedIncrement(&fs_wins);
    }
    for (int i = 0; i < w->n; i++)
        if (w->e[i].proc == p && w->e[i].id == id) {
            w->e[i].ref = ref;
            return TRUE;
        }
    if (w->n == w->cap) {
        int nc = w->cap ? w->cap * 2 : 4;
        fs_entry *ne = (fs_entry *)realloc(w->e, nc * sizeof *ne);
        if (!ne)
            return FALSE;
        w->e = ne;
        w->cap = nc;
    }
    w->e[w->n++] = (fs_entry){ p, id, ref };
    return TRUE;
}

static BOOL WINAPI fs_remove(HWND h, SUBCLASSPROC p, UINT_PTR id)
{
    fs_win *w = fs_find(h);
    for (int i = 0; w && i < w->n; i++)
        if (w->e[i].proc == p && w->e[i].id == id) {
            w->e[i].proc = NULL;
            return TRUE;
        }
    return FALSE;
}

static LRESULT WINAPI fs_def(HWND h, UINT m, WPARAM wp, LPARAM lp)
{
    for (fs_frame *f = fs_tls == TLS_OUT_OF_INDEXES ? NULL : (fs_frame *)TlsGetValue(fs_tls); f; f = f->up)
        if (f->w->hwnd == h)
            return fs_next(f, h, m, wp, lp);
    return fs_comctl_def ? fs_comctl_def(h, m, wp, lp) : 0;
}

static __attribute__((unused)) int fs_patch_iat(HMODULE mod)
{
    uint8_t *b = (uint8_t *)mod;
    IMAGE_NT_HEADERS *nt = (IMAGE_NT_HEADERS *)(b + ((IMAGE_DOS_HEADER *)b)->e_lfanew);
    IMAGE_DATA_DIRECTORY dd = nt->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_IMPORT];
    int n = 0;
    if (!dd.VirtualAddress)
        return 0;
    for (IMAGE_IMPORT_DESCRIPTOR *d = (IMAGE_IMPORT_DESCRIPTOR *)(b + dd.VirtualAddress); d->Name; d++) {
        if (lstrcmpiA((const char *)(b + d->Name), "comctl32.dll") || !d->OriginalFirstThunk)
            continue;
        IMAGE_THUNK_DATA32 *names = (IMAGE_THUNK_DATA32 *)(b + d->OriginalFirstThunk);
        IMAGE_THUNK_DATA32 *iat = (IMAGE_THUNK_DATA32 *)(b + d->FirstThunk);
        for (; names->u1.Ordinal; names++, iat++) {
            int ord = 0;
            if (IMAGE_SNAP_BY_ORDINAL32(names->u1.Ordinal))
                ord = IMAGE_ORDINAL32(names->u1.Ordinal);
            else {
                const char *nm = (const char *)((IMAGE_IMPORT_BY_NAME *)(b + names->u1.AddressOfData))->Name;
                ord = !strcmp(nm, "SetWindowSubclass") ? 410 : !strcmp(nm, "RemoveWindowSubclass") ? 412 : !strcmp(nm, "DefSubclassProc") ? 413 : 0;
            }
            void *to = ord == 410 ? (void *)fs_set : ord == 412 ? (void *)fs_remove : ord == 413 ? (void *)fs_def : NULL;
            if (!to)
                continue;
            if (ord == 413)
                fs_comctl_def = (LRESULT(WINAPI *)(HWND, UINT, WPARAM, LPARAM))(uintptr_t)iat->u1.Function;
            DWORD old;
            if (VirtualProtect(&iat->u1.Function, 4, PAGE_READWRITE, &old)) {
                iat->u1.Function = (DWORD)(uintptr_t)to;
                VirtualProtect(&iat->u1.Function, 4, old, &old);
                n++;
            }
        }
    }
    return n;
}
#endif
