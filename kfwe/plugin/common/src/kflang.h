#pragma once
#include <windows.h>
#include <stdlib.h>

typedef struct {
    WCHAR *k, *v;
} KflPair;

static KflPair *kfl_tab;
static unsigned kfl_mask;
static INIT_ONCE kfl_once = INIT_ONCE_STATIC_INIT;

static unsigned kfl_hash(const WCHAR *s)
{
    unsigned h = 2166136261u;
    while (*s)
        h = (h ^ *s++) * 16777619u;
    return h;
}

static int kfl_root(WCHAR *dir)
{
    HMODULE self;
    if (!GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT, (LPCWSTR)kfl_root, &self) ||
        !GetModuleFileNameW(self, dir, MAX_PATH))
        return 0;
    for (int up = 0; up < 4; up++) {
        WCHAR *s = wcsrchr(dir, L'\\'), probe[MAX_PATH];
        if (!s)
            return 0;
        *s = 0;
        wsprintfW(probe, L"%s\\share\\locale", dir);
        if (GetFileAttributesW(probe) != INVALID_FILE_ATTRIBUTES)
            return 1;
    }
    return 0;
}

static WCHAR *kfl_load(const WCHAR *path)
{
    HANDLE f = CreateFileW(path, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, 0, NULL);
    if (f == INVALID_HANDLE_VALUE)
        return NULL;
    DWORD n = GetFileSize(f, NULL), got = 0;
    char *b = n < (64u << 20) ? malloc(n + 1) : NULL;
    WCHAR *w = NULL;
    if (b && ReadFile(f, b, n, &got, NULL)) {
        int skip = got >= 3 && !memcmp(b, "\xEF\xBB\xBF", 3) ? 3 : 0;
        int wn = MultiByteToWideChar(CP_UTF8, 0, b + skip, got - skip, NULL, 0);
        if ((w = malloc((wn + 1) * sizeof(WCHAR))))
            w[MultiByteToWideChar(CP_UTF8, 0, b + skip, got - skip, w, wn)] = 0;
    }
    free(b);
    CloseHandle(f);
    return w;
}

static void kfl_unescape(WCHAR *s)
{
    WCHAR *o = s;
    for (; *s; s++)
        if (*s == L'\\' && (s[1] == L'n' || s[1] == L't' || s[1] == L's' || s[1] == L'\\'))
            *o++ = *++s == L'n' ? L'\n' : *s == L't' ? L'\t' : *s == L's' ? L' ' : L'\\';
        else
            *o++ = *s;
    *o = 0;
}

static WCHAR *kfl_trim(WCHAR *s)
{
    while (*s == L' ' || *s == L'\t')
        s++;
    WCHAR *e = s + lstrlenW(s);
    while (e > s && (e[-1] == L' ' || e[-1] == L'\t' || e[-1] == L'\r'))
        *--e = 0;
    return s;
}

static int kfl_language(WCHAR *code, int cap)
{
    WCHAR dir[MAX_PATH], path[MAX_PATH];
    if (!kfl_root(dir))
        return 0;
    wsprintfW(path, L"%s\\share\\locale\\default", dir);
    WCHAR *t = kfl_load(path);
    if (!t)
        return 0;
    WCHAR *c = kfl_trim(t);
    WCHAR *nl = wcschr(c, L'\n');
    if (nl)
        *nl = 0;
    c = kfl_trim(c);
    lstrcpynW(code, c, cap);
    free(t);
    return code[0] && lstrcmpiW(code, L"ru");
}

static BOOL CALLBACK kfl_init(PINIT_ONCE once, PVOID param, PVOID *ctx)
{
    WCHAR dir[MAX_PATH], code[32], path[MAX_PATH];
    (void)once, (void)param, (void)ctx;
    if (!kfl_language(code, 32) || !kfl_root(dir))
        return TRUE;
    wsprintfW(path, L"%s\\share\\locale\\%s\\kfwe.txt", dir, code);
    WCHAR *text = kfl_load(path);
    if (!text)
        return TRUE;
    unsigned n = 0;
    for (WCHAR *p = text; *p; p++)
        n += *p == L'\n';
    for (kfl_mask = 63; kfl_mask < n * 2; kfl_mask = kfl_mask * 2 + 1)
        ;
    kfl_tab = calloc(kfl_mask + 1, sizeof(KflPair));
    if (!kfl_tab)
        return TRUE;
    for (WCHAR *line = text, *next; line; line = next) {
        next = wcschr(line, L'\n');
        if (next)
            *next++ = 0;
        WCHAR *s = kfl_trim(line), *eq = *s == L'#' ? NULL : wcsstr(s, L" = ");
        if (!eq)
            continue;
        *eq = 0;
        WCHAR *k = kfl_trim(s), *v = kfl_trim(eq + 3);
        if (!*k || !*v)
            continue;
        kfl_unescape(k);
        kfl_unescape(v);
        unsigned i = kfl_hash(k) & kfl_mask;
        while (kfl_tab[i].k && lstrcmpW(kfl_tab[i].k, k))
            i = (i + 1) & kfl_mask;
        kfl_tab[i].k = k;
        kfl_tab[i].v = v;
    }
    return TRUE;
}

static const WCHAR *TR(const WCHAR *s)
{
    InitOnceExecuteOnce(&kfl_once, kfl_init, NULL, NULL);
    if (!kfl_tab || !s)
        return s;
    for (unsigned i = kfl_hash(s) & kfl_mask; kfl_tab[i].k; i = (i + 1) & kfl_mask)
        if (!lstrcmpW(kfl_tab[i].k, s))
            return kfl_tab[i].v;
    return s;
}
