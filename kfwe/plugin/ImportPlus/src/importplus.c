#define WIN32_LEAN_AND_MEAN
#define COBJMACROS
#include <windows.h>
#include <windowsx.h>
#include <commctrl.h>
#include <commdlg.h>
#include <shlobj.h>
#include <wincodec.h>
#include <stdint.h>
#include <stdlib.h>
#include <wchar.h>
#include "fastsub.h"

#define IDC_WE_TOOLBAR 10
#define IDC_WE_LIST 11
#define CMD_EXPORT 10
#define CMD_PROPS 256
#define CMD_REPLACE 257
#define CMD_DELETE 258
#define CMD_COPYPATH 0x7E20
#define PROP L"KFImportPlus"
#define VIEW_CLASS L"KFImportView"
#define TIMER_REFRESH 1
#define TIMER_SEL 2
#define TIMER_THUMB 3
#define TIMER_AUTOSCROLL 4

#define C_BG RGB(0x17, 0x18, 0x1C)
#define C_PANEL RGB(0x1E, 0x1F, 0x24)
#define C_CARD RGB(0x2B, 0x2D, 0x33)
#define C_INPUT RGB(0x2C, 0x2C, 0x2C)
#define C_HOVER RGB(0x36, 0x39, 0x44)
#define C_LINE RGB(0x2A, 0x2C, 0x33)
#define C_ACCENT RGB(0x7C, 0x5C, 0xFF)
#define C_SEL RGB(0x35, 0x2E, 0x5E)
#define C_TEXT RGB(0xE4, 0xE6, 0xEB)
#define C_DIM RGB(0xA4, 0xA9, 0xB6)
#define C_FAINT RGB(0x7D, 0x82, 0x90)
#define C_DANGER RGB(0xF2, 0x5F, 0x5C)
#define ARGB_OF(c, a) ((DWORD)(a) << 24 | (DWORD)GetRValue(c) << 16 | (DWORD)GetGValue(c) << 8 | GetBValue(c))

enum { K_ALL, K_ICON, K_TEX, K_MODEL, K_SOUND, K_OTHER, K_N };
static const wchar_t *k_name[K_N] = { L"Все файлы", L"Иконки", L"Текстуры", L"Модели", L"Звуки", L"Прочее" };
static const COLORREF k_color[K_N] = { RGB(0x7C, 0x5C, 0xFF), RGB(0xF2, 0xC1, 0x4E), RGB(0x4F, 0xC1, 0xE9), RGB(0xA7, 0x8B, 0xFA), RGB(0xF5, 0x9E, 0x0B), RGB(0x94, 0xA3, 0xB8) };
#define IS_IMAGE(r) ((r)->kind == K_TEX || (r)->kind == K_ICON)

typedef void GpGraphics, GpBrush, GpPen, GpPath, GpImage;
typedef DWORD ARGB;
typedef struct { UINT32 ver; void *cb; BOOL nothread, nocodecs; } GdipInput;
int WINAPI GdiplusStartup(ULONG_PTR *, const GdipInput *, void *);
int WINAPI GdipCreateFromHDC(HDC, GpGraphics **);
int WINAPI GdipDeleteGraphics(GpGraphics *);
int WINAPI GdipSetSmoothingMode(GpGraphics *, int);
int WINAPI GdipSetPixelOffsetMode(GpGraphics *, int);
int WINAPI GdipSetInterpolationMode(GpGraphics *, int);
int WINAPI GdipCreateSolidFill(ARGB, GpBrush **);
int WINAPI GdipDeleteBrush(GpBrush *);
int WINAPI GdipCreatePen1(ARGB, float, int, GpPen **);
int WINAPI GdipDeletePen(GpPen *);
int WINAPI GdipSetPenStartCap(GpPen *, int);
int WINAPI GdipSetPenEndCap(GpPen *, int);
int WINAPI GdipCreatePath(int, GpPath **);
int WINAPI GdipDeletePath(GpPath *);
int WINAPI GdipAddPathArc(GpPath *, float, float, float, float, float, float);
int WINAPI GdipClosePathFigure(GpPath *);
int WINAPI GdipFillPath(GpGraphics *, GpBrush *, GpPath *);
int WINAPI GdipDrawPath(GpGraphics *, GpPen *, GpPath *);
int WINAPI GdipDrawLine(GpGraphics *, GpPen *, float, float, float, float);
int WINAPI GdipDrawEllipse(GpGraphics *, GpPen *, float, float, float, float);
int WINAPI GdipFillRectangleI(GpGraphics *, GpBrush *, int, int, int, int);
int WINAPI GdipCreateBitmapFromScan0(int, int, int, int, BYTE *, GpImage **);
int WINAPI GdipGetImageGraphicsContext(GpImage *, GpGraphics **);
int WINAPI GdipDisposeImage(GpImage *);
int WINAPI GdipDrawImageRectI(GpGraphics *, GpImage *, int, int, int, int);
int WINAPI GdipSetClipPath(GpGraphics *, GpPath *, int);
int WINAPI GdipResetClip(GpGraphics *);
#define PF_PARGB 0xE200B

typedef struct Thumb { wchar_t *key; GpImage *img; int w, h, state; struct Thumb *next; } Thumb;
typedef struct { wchar_t *name, *type, *size, *path, *low, *ext; int kb, we, kind, dirlen; } Row;
typedef struct { wchar_t *prefix, *label; int parent, count, total, depth, kids, expanded; } Fold;
typedef struct { int kind, idx; RECT rc; } SbItem;
typedef struct { int id; const wchar_t *label; int danger; RECT rc; } Btn;

typedef struct {
    HWND dlg, welv, view, edit;
    Row *rows;
    int nrows, kcount[K_N];
    Fold *fold;
    int nfold, capfold, *forder;
    int *vis, nvis;
    uint8_t *sel;
    int anchor, focus;
    int sort_col, sort_desc, kind_f, fold_f, grid;
    int list_y, sb_y;
    int drag_scroll, drag_off;
    int press, marq, marq_x0, marq_y0, marq_x1, marq_y1;
    uint8_t *sel_base;
    POINT mouse;
    int hover_kind, hover_idx;
    SbItem *sb;
    int nsb;
    Btn btn[8];
    int nbtn;
    RECT r_side, r_main, r_prev, r_top, r_list, r_head, r_status, r_search, r_vlist, r_vgrid, r_thumb;
    int prev_row;
    GpImage *prev_img;
    int prev_w, prev_h;
    wchar_t prev_fmt[64];
    Thumb *thumbs[1024];
    HDC bb_dc;
    HBITMAP bb_bmp;
    HGDIOBJ bb_old;
    int bb_w, bb_h;
} Imp;

static HINSTANCE g_inst;
static ULONG_PTR g_gdip;
static HFONT f_ui, f_bold, f_small, f_title, f_badge, f_section;
static int g_inner, g_auto;
static char g_save_to[MAX_PATH];
static int g_save_set;

static BOOL(WINAPI *o_save)(OPENFILENAMEA *);
static int(WINAPI *o_msgbox)(HWND, LPCSTR, LPCSTR, UINT);

static BOOL WINAPI h_save(OPENFILENAMEA *o)
{
    if (g_save_set) {
        g_save_set = 0;
        lstrcpynA(o->lpstrFile, g_save_to, (int)o->nMaxFile);
        return TRUE;
    }
    return o_save(o);
}

static int WINAPI h_msgbox(HWND h, LPCSTR t, LPCSTR c, UINT f)
{
    if (g_auto) {
        UINT k = f & MB_TYPEMASK;
        return k == MB_YESNO || k == MB_YESNOCANCEL ? IDYES : k == MB_RETRYCANCEL ? IDCANCEL : IDOK;
    }
    return o_msgbox(h, t, c, f);
}

static void *iat_patch(HMODULE mod, const char *dll, const char *fn, void *hook)
{
    uint8_t *b = (uint8_t *)mod;
    IMAGE_NT_HEADERS *nt = (IMAGE_NT_HEADERS *)(b + ((IMAGE_DOS_HEADER *)b)->e_lfanew);
    IMAGE_DATA_DIRECTORY dd = nt->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_IMPORT];
    if (!dd.VirtualAddress) return NULL;
    for (IMAGE_IMPORT_DESCRIPTOR *d = (IMAGE_IMPORT_DESCRIPTOR *)(b + dd.VirtualAddress); d->Name; d++) {
        if (lstrcmpiA((const char *)(b + d->Name), dll) || !d->OriginalFirstThunk) continue;
        IMAGE_THUNK_DATA32 *nm = (IMAGE_THUNK_DATA32 *)(b + d->OriginalFirstThunk), *iat = (IMAGE_THUNK_DATA32 *)(b + d->FirstThunk);
        for (; nm->u1.Ordinal; nm++, iat++) {
            if (IMAGE_SNAP_BY_ORDINAL32(nm->u1.Ordinal) || strcmp((const char *)((IMAGE_IMPORT_BY_NAME *)(b + nm->u1.AddressOfData))->Name, fn))
                continue;
            void *old = (void *)(uintptr_t)iat->u1.Function;
            DWORD prot;
            if (!VirtualProtect(&iat->u1.Function, 4, PAGE_READWRITE, &prot)) return NULL;
            iat->u1.Function = (DWORD)(uintptr_t)hook;
            VirtualProtect(&iat->u1.Function, 4, prot, &prot);
            return old;
        }
    }
    return NULL;
}

static wchar_t *lv_text(HWND lv, int i, int col)
{
    wchar_t buf[1024];
    LVITEMW it = { 0 };
    it.iSubItem = col;
    it.pszText = buf;
    it.cchTextMax = 1024;
    buf[0] = 0;
    SendMessageW(lv, LVM_GETITEMTEXTW, i, (LPARAM)&it);
    return _wcsdup(buf);
}

static void fold_case(wchar_t *s)
{
    CharLowerBuffW(s, (DWORD)wcslen(s));
    for (; *s; s++) {
        if (*s == L'ё') *s = L'е';
        if (*s == L'/') *s = L'\\';
    }
}

static int kind_of(const wchar_t *ext)
{
    static const wchar_t *tex[] = { L"blp", L"tga", L"dds", L"bmp", L"jpg", L"png" }, *mdl[] = { L"mdx", L"mdl" }, *snd[] = { L"wav", L"mp3", L"flac", L"ogg" };
    for (int i = 0; i < 6; i++) if (!_wcsicmp(ext, tex[i])) return K_TEX;
    for (int i = 0; i < 2; i++) if (!_wcsicmp(ext, mdl[i])) return K_MODEL;
    for (int i = 0; i < 4; i++) if (!_wcsicmp(ext, snd[i])) return K_SOUND;
    return K_OTHER;
}

static void free_rows(Imp *im)
{
    for (int i = 0; i < im->nrows; i++) {
        Row *r = &im->rows[i];
        free(r->name); free(r->type); free(r->size); free(r->path); free(r->low);
    }
    free(im->rows);
    free(im->sel);
    im->rows = NULL;
    im->sel = NULL;
    im->nrows = 0;
    for (int i = 0; i < im->nfold; i++) { free(im->fold[i].prefix); free(im->fold[i].label); }
    im->nfold = 0;
}

static int find_fold(Imp *im, const wchar_t *p)
{
    for (int i = 0; i < im->nfold; i++)
        if (!wcscmp(im->fold[i].prefix, p)) return i;
    return -1;
}

static int add_fold(Imp *im, const wchar_t *p, const wchar_t *orig, int len)
{
    wchar_t *key = malloc((len + 1) * sizeof *key);
    wcsncpy(key, p, len);
    key[len] = 0;
    int f = find_fold(im, key);
    if (f >= 0) { free(key); return f; }
    int k = len - 2;
    while (k >= 0 && p[k] != L'\\') k--;
    int parent = k >= 0 ? add_fold(im, p, orig, k + 1) : -1;
    if (im->nfold == im->capfold) {
        im->capfold = im->capfold ? im->capfold * 2 : 64;
        im->fold = realloc(im->fold, im->capfold * sizeof *im->fold);
    }
    Fold *fd = &im->fold[im->nfold];
    memset(fd, 0, sizeof *fd);
    fd->prefix = key;
    fd->parent = parent;
    fd->depth = parent >= 0 ? im->fold[parent].depth + 1 : 0;
    int a = k + 1, n = len - 1 - a;
    fd->label = malloc((n + 1) * sizeof *fd->label);
    wcsncpy(fd->label, orig + a, n);
    fd->label[n] = 0;
    if (parent >= 0) im->fold[parent].kids++;
    return im->nfold++;
}

static Imp *g_sort_im;
static int cmp_fold_q(const void *a, const void *b) { return wcscmp(g_sort_im->fold[*(const int *)a].prefix, g_sort_im->fold[*(const int *)b].prefix); }

static void read_we(Imp *im)
{
    free_rows(im);
    memset(im->kcount, 0, sizeof im->kcount);
    int n = (int)SendMessageW(im->welv, LVM_GETITEMCOUNT, 0, 0);
    im->rows = calloc(n > 0 ? n : 1, sizeof *im->rows);
    im->sel = calloc(n > 0 ? n : 1, 1);
    for (int i = 0; i < n; i++) {
        Row *r = &im->rows[im->nrows++];
        r->we = i;
        r->name = lv_text(im->welv, i, 0);
        r->type = lv_text(im->welv, i, 1);
        r->size = lv_text(im->welv, i, 2);
        r->path = lv_text(im->welv, i, 3);
        for (const wchar_t *s = r->size; *s; s++)
            if (*s >= L'0' && *s <= L'9') r->kb = r->kb * 10 + (*s - L'0');
        size_t l = wcslen(r->path) + wcslen(r->name) + wcslen(r->type) + 3;
        r->low = malloc(l * sizeof *r->low);
        wcscpy(r->low, r->path); wcscat(r->low, L"\n"); wcscat(r->low, r->name); wcscat(r->low, L"\n"); wcscat(r->low, r->type);
        fold_case(r->low);
        const wchar_t *dot = wcsrchr(r->name, L'.');
        r->ext = (wchar_t *)(dot ? dot + 1 : L"");
        r->kind = kind_of(r->ext);
        if (r->kind == K_TEX) {
            static const wchar_t *pre[] = { L"btn", L"disbtn", L"pasbtn", L"dispasbtn", L"atc", L"disatc" };
            const wchar_t *nm = wcschr(r->low, L'\n') + 1;
            int icon = wcsstr(r->low, L"commandbuttons") != NULL && wcsstr(r->low, L"commandbuttons") < nm;
            icon |= wcsstr(r->low, L"passivebuttons") != NULL && wcsstr(r->low, L"passivebuttons") < nm;
            for (int k = 0; k < 6 && !icon; k++) icon = !wcsncmp(nm, pre[k], wcslen(pre[k]));
            if (icon) r->kind = K_ICON;
        }
        im->kcount[K_ALL]++;
        im->kcount[r->kind]++;
        const wchar_t *end = wcschr(r->low, L'\n');
        int last = -1;
        for (const wchar_t *s = r->low; s < end; s++)
            if (*s == L'\\') last = (int)(s - r->low);
        r->dirlen = last + 1;
        if (last >= 0) {
            int f = add_fold(im, r->low, r->path, last + 1);
            im->fold[f].count++;
        }
    }
    free(im->forder);
    im->forder = malloc((im->nfold + 1) * sizeof *im->forder);
    for (int i = 0; i < im->nfold; i++) im->forder[i] = i;
    g_sort_im = im;
    qsort(im->forder, im->nfold, sizeof *im->forder, cmp_fold_q);
    for (int i = 0; i < im->nfold; i++)
        for (int f = i; f >= 0; f = im->fold[f].parent)
            im->fold[f].total += im->fold[i].count;
}

static int cmp_rows(const void *a, const void *b)
{
    Imp *im = g_sort_im;
    const Row *x = &im->rows[*(const int *)a], *y = &im->rows[*(const int *)b];
    int c;
    switch (im->sort_col) {
    case 0: c = _wcsicmp(x->name, y->name); break;
    case 2: c = x->kind != y->kind ? x->kind - y->kind : _wcsicmp(x->ext, y->ext); break;
    case 3: c = x->kb < y->kb ? -1 : x->kb > y->kb; break;
    default: c = _wcsicmp(x->path, y->path); break;
    }
    if (!c) c = _wcsicmp(x->path, y->path);
    return im->sort_desc ? -c : c;
}

static void ensure_thumbs(Imp *im);

static void apply_filter(Imp *im)
{
    wchar_t q[256], *tok[16];
    int nt = 0;
    GetWindowTextW(im->edit, q, 256);
    fold_case(q);
    for (wchar_t *p = q; *p && nt < 16;) {
        while (*p == L' ') *p++ = 0;
        if (!*p) break;
        tok[nt++] = p;
        while (*p && *p != L' ') p++;
    }
    const wchar_t *pre = im->fold_f >= 0 && im->fold_f < im->nfold ? im->fold[im->fold_f].prefix : NULL;
    size_t pl = pre ? wcslen(pre) : 0;
    free(im->vis);
    im->vis = malloc((im->nrows > 0 ? im->nrows : 1) * sizeof *im->vis);
    im->nvis = 0;
    for (int i = 0; i < im->nrows; i++) {
        const Row *r = &im->rows[i];
        if (im->kind_f != K_ALL && r->kind != im->kind_f) continue;
        if (pre && wcsncmp(r->low, pre, pl)) continue;
        int ok = 1;
        for (int t = 0; t < nt && ok; t++) ok = wcsstr(r->low, tok[t]) != NULL;
        if (ok) im->vis[im->nvis++] = i;
    }
    uint8_t *shown = calloc(im->nrows > 0 ? im->nrows : 1, 1);
    for (int v = 0; v < im->nvis; v++) shown[im->vis[v]] = 1;
    for (int i = 0; i < im->nrows; i++) im->sel[i] &= shown[i];
    free(shown);
    g_sort_im = im;
    qsort(im->vis, im->nvis, sizeof *im->vis, cmp_rows);
    im->focus = -1;
    for (int v = 0; v < im->nvis && im->focus < 0; v++)
        if (im->sel[im->vis[v]]) im->focus = v;
    im->anchor = im->focus;
    im->list_y = 0;
    InvalidateRect(im->view, NULL, FALSE);
    ensure_thumbs(im);
}

static int sel_count(Imp *im, int *kb)
{
    int n = 0, k = 0;
    for (int i = 0; i < im->nrows; i++)
        if (im->sel[i]) { n++; k += im->rows[i].kb; }
    if (kb) *kb = k;
    return n;
}

static int sel_rows(Imp *im, int *out, int max)
{
    int n = 0;
    for (int v = 0; v < im->nvis && n < max; v++)
        if (im->sel[im->vis[v]]) out[n++] = im->vis[v];
    return n;
}

static int focus_row(Imp *im)
{
    if (im->focus >= 0 && im->focus < im->nvis && im->sel[im->vis[im->focus]]) return im->vis[im->focus];
    for (int v = 0; v < im->nvis; v++)
        if (im->sel[im->vis[v]]) return im->vis[v];
    return -1;
}

static int select_we(Imp *im, int r)
{
    if (r < 0 || r >= im->nrows) return 0;
    int n = (int)SendMessageW(im->welv, LVM_GETITEMCOUNT, 0, 0), wi = im->rows[r].we;
    wchar_t *t = wi < n ? lv_text(im->welv, wi, 3) : NULL;
    if (!t || _wcsicmp(t, im->rows[r].path)) {
        wi = -1;
        for (int i = 0; i < n && wi < 0; i++) {
            wchar_t *u = lv_text(im->welv, i, 3);
            if (!_wcsicmp(u, im->rows[r].path)) wi = i;
            free(u);
        }
    }
    free(t);
    if (wi < 0) return 0;
    LVITEMW it = { 0 };
    it.stateMask = LVIS_SELECTED | LVIS_FOCUSED;
    SendMessageW(im->welv, LVM_SETITEMSTATE, (WPARAM)-1, (LPARAM)&it);
    it.state = LVIS_SELECTED | LVIS_FOCUSED;
    SendMessageW(im->welv, LVM_SETITEMSTATE, wi, (LPARAM)&it);
    return 1;
}

static void we_command(Imp *im, int cmd)
{
    g_inner++;
    SendMessageW(im->dlg, WM_COMMAND, cmd, 0);
    g_inner--;
}

static uint8_t *read_file(const wchar_t *path, DWORD *len)
{
    HANDLE f = CreateFileW(path, GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_DELETE, NULL, OPEN_EXISTING, 0, NULL);
    if (f == INVALID_HANDLE_VALUE) return NULL;
    DWORD n = GetFileSize(f, NULL), got = 0;
    uint8_t *b = n && n < (64u << 20) ? malloc(n) : NULL;
    if (b && (!ReadFile(f, b, n, &got, NULL) || got != n)) { free(b); b = NULL; }
    CloseHandle(f);
    *len = n;
    return b;
}

static uint8_t *decode_blp(const uint8_t *b, DWORD n, int *w, int *h, wchar_t *fmt)
{
    if (n < 160 || memcmp(b, "BLP1", 4)) return NULL;
    uint32_t comp = *(uint32_t *)(b + 4), abits = *(uint32_t *)(b + 8), W = *(uint32_t *)(b + 12), H = *(uint32_t *)(b + 16);
    uint32_t off = *(uint32_t *)(b + 28), sz = *(uint32_t *)(b + 92);
    if (!W || !H || W > 4096 || H > 4096 || off > n || sz > n - off) return NULL;
    uint8_t *px = malloc((size_t)W * H * 4);
    if (comp == 1) {
        const uint32_t *pal = (const uint32_t *)(b + 156);
        if (off + (size_t)W * H > n) { free(px); return NULL; }
        const uint8_t *idx = b + off, *al = idx + (size_t)W * H;
        for (uint32_t i = 0; i < W * H; i++) {
            uint32_t c = pal[idx[i]];
            uint8_t a = 255;
            if (abits == 8 && al + i < b + n) a = al[i];
            else if (abits == 4 && al + i / 2 < b + n) a = (uint8_t)(((al[i / 2] >> ((i & 1) * 4)) & 15) * 17);
            else if (abits == 1 && al + i / 8 < b + n) a = (al[i / 8] >> (i & 7)) & 1 ? 255 : 0;
            px[i * 4 + 0] = (uint8_t)c; px[i * 4 + 1] = (uint8_t)(c >> 8); px[i * 4 + 2] = (uint8_t)(c >> 16); px[i * 4 + 3] = a;
        }
        swprintf(fmt, 64, L"BLP палитра%ls", abits ? L", альфа" : L"");
    } else {
        uint32_t hs = *(uint32_t *)(b + 156);
        if (160 + (size_t)hs > n) { free(px); return NULL; }
        uint8_t *jpg = malloc(hs + sz);
        memcpy(jpg, b + 160, hs);
        memcpy(jpg + hs, b + off, sz);
        int ok = 0;
        static IWICImagingFactory *fac;
        if (!fac) {
            CoInitializeEx(NULL, COINIT_APARTMENTTHREADED);
            CoCreateInstance(&CLSID_WICImagingFactory, NULL, CLSCTX_INPROC_SERVER, &IID_IWICImagingFactory, (void **)&fac);
        }
        IWICStream *st = NULL;
        IWICBitmapDecoder *dec = NULL;
        IWICBitmapFrameDecode *fr = NULL;
        WICPixelFormatGUID pf;
        UINT fw = 0, fh = 0;
        if (fac && SUCCEEDED(IWICImagingFactory_CreateStream(fac, &st)) && SUCCEEDED(IWICStream_InitializeFromMemory(st, jpg, hs + sz)) &&
            SUCCEEDED(IWICImagingFactory_CreateDecoderFromStream(fac, (IStream *)st, NULL, WICDecodeMetadataCacheOnDemand, &dec)) &&
            SUCCEEDED(IWICBitmapDecoder_GetFrame(dec, 0, &fr)) && SUCCEEDED(IWICBitmapFrameDecode_GetPixelFormat(fr, &pf)) &&
            IsEqualGUID(&pf, &GUID_WICPixelFormat32bppCMYK) && SUCCEEDED(IWICBitmapFrameDecode_GetSize(fr, &fw, &fh)) && fw == W && fh == H &&
            SUCCEEDED(IWICBitmapFrameDecode_CopyPixels(fr, NULL, W * 4, W * H * 4, px))) {
            for (uint32_t i = 0; i < W * H * 4; i++) px[i] = (uint8_t)(255 - px[i]);
            ok = 1;
        }
        if (fr) IWICBitmapFrameDecode_Release(fr);
        if (dec) IWICBitmapDecoder_Release(dec);
        if (st) IWICStream_Release(st);
        free(jpg);
        if (!ok) { free(px); return NULL; }
        if (!abits)
            for (uint32_t i = 0; i < W * H; i++) px[i * 4 + 3] = 255;
        swprintf(fmt, 64, L"BLP JPEG%ls", abits ? L", альфа" : L"");
    }
    *w = (int)W;
    *h = (int)H;
    return px;
}

static uint8_t *decode_tga(const uint8_t *b, DWORD n, int *w, int *h, wchar_t *fmt)
{
    if (n < 18) return NULL;
    int type = b[2], W = b[12] | b[13] << 8, H = b[14] | b[15] << 8, bpp = b[16], top = b[17] & 0x20;
    if ((type != 2 && type != 10) || (bpp != 24 && bpp != 32) || !W || !H || W > 4096 || H > 4096) return NULL;
    int bp = bpp / 8;
    const uint8_t *s = b + 18 + b[0], *e = b + n;
    uint8_t *px = malloc((size_t)W * H * 4);
    for (int i = 0; i < W * H;) {
        int cnt = 1, rle = 0;
        if (type == 10) {
            if (s >= e) break;
            uint8_t hd = *s++;
            cnt = (hd & 127) + 1;
            rle = hd & 128;
        }
        for (int k = 0; k < cnt && i < W * H; k++, i++) {
            if (s + bp > e) { i = W * H; break; }
            int y = i / W, x = i % W, row = top ? y : H - 1 - y;
            uint8_t *d = px + ((size_t)row * W + x) * 4;
            d[0] = s[0]; d[1] = s[1]; d[2] = s[2]; d[3] = bp == 4 ? s[3] : 255;
            if (!rle || k == cnt - 1) s += bp;
        }
    }
    swprintf(fmt, 64, L"TGA %d бит", bpp);
    *w = W;
    *h = H;
    return px;
}

static GpImage *make_image(uint8_t *px, int w, int h, int max)
{
    for (int i = 0; i < w * h; i++) {
        unsigned a = px[i * 4 + 3];
        px[i * 4 + 0] = (uint8_t)(px[i * 4 + 0] * a / 255);
        px[i * 4 + 1] = (uint8_t)(px[i * 4 + 1] * a / 255);
        px[i * 4 + 2] = (uint8_t)(px[i * 4 + 2] * a / 255);
    }
    GpImage *src = NULL, *dst = NULL;
    if (GdipCreateBitmapFromScan0(w, h, w * 4, PF_PARGB, px, &src)) return NULL;
    int dw = w, dh = h;
    if (w > max || h > max) {
        if (w >= h) { dw = max; dh = h * max / w; } else { dh = max; dw = w * max / h; }
        if (dw < 1) dw = 1;
        if (dh < 1) dh = 1;
    }
    GpGraphics *g;
    if (!GdipCreateBitmapFromScan0(dw, dh, 0, PF_PARGB, NULL, &dst) && !GdipGetImageGraphicsContext(dst, &g)) {
        GdipSetInterpolationMode(g, 7);
        GdipSetPixelOffsetMode(g, 4);
        GdipDrawImageRectI(g, src, 0, 0, dw, dh);
        GdipDeleteGraphics(g);
    }
    GdipDisposeImage(src);
    return dst;
}

static uint8_t *load_row_image(Imp *im, int r, int *w, int *h, wchar_t *fmt)
{
    Row *row = &im->rows[r];
    if (!IS_IMAGE(row) || (_wcsicmp(row->ext, L"blp") && _wcsicmp(row->ext, L"tga"))) return NULL;
    wchar_t tmp[MAX_PATH], file[MAX_PATH];
    GetTempPathW(MAX_PATH, tmp);
    wcscat(tmp, L"KFImportPreview");
    CreateDirectoryW(tmp, NULL);
    if (!GetShortPathNameW(tmp, file, MAX_PATH)) wcscpy(file, tmp);
    wcscat(file, L"\\preview.");
    wcscat(file, row->ext);
    DeleteFileW(file);
    uint8_t *px = NULL;
    if (select_we(im, r) && WideCharToMultiByte(CP_ACP, 0, file, -1, g_save_to, MAX_PATH, NULL, NULL)) {
        g_save_set = 1;
        g_auto++;
        we_command(im, CMD_EXPORT);
        g_auto--;
        g_save_set = 0;
        DWORD n = 0;
        uint8_t *data = read_file(file, &n);
        if (data) px = !_wcsicmp(row->ext, L"blp") ? decode_blp(data, n, w, h, fmt) : decode_tga(data, n, w, h, fmt);
        free(data);
        DeleteFileW(file);
    }
    return px;
}

static Thumb *thumb_get(Imp *im, const wchar_t *low, int create)
{
    uint32_t hsh = 2166136261u;
    for (const wchar_t *s = low; *s && *s != L'\n'; s++) hsh = (hsh ^ *s) * 16777619u;
    Thumb **slot = &im->thumbs[hsh & 1023];
    size_t len = wcscspn(low, L"\n");
    for (Thumb *t = *slot; t; t = t->next)
        if (wcslen(t->key) == len && !wcsncmp(t->key, low, len)) return t;
    if (!create) return NULL;
    Thumb *t = calloc(1, sizeof *t);
    t->key = malloc((len + 1) * sizeof *t->key);
    wcsncpy(t->key, low, len);
    t->key[len] = 0;
    t->next = *slot;
    *slot = t;
    return t;
}

#define SB_W 250
#define PV_W 310
#define TOP_H 60
#define HEAD_H 30
#define ROW_H 34
#define STATUS_H 30
#define TILE_W 132
#define TILE_H 150

static int content_h(Imp *im)
{
    int w = im->r_list.right - im->r_list.left - 12;
    if (im->grid) {
        int cols = w / TILE_W;
        if (cols < 1) cols = 1;
        return (im->nvis + cols - 1) / cols * TILE_H + 12;
    }
    return im->nvis * ROW_H;
}

static int grid_cols(Imp *im)
{
    int cols = (im->r_list.right - im->r_list.left - 12) / TILE_W;
    return cols < 1 ? 1 : cols;
}

static void clamp_scroll(Imp *im)
{
    int maxy = content_h(im) - (im->r_list.bottom - im->r_list.top);
    if (im->list_y > maxy) im->list_y = maxy;
    if (im->list_y < 0) im->list_y = 0;
}

static void layout(Imp *im)
{
    RECT c;
    GetClientRect(im->view, &c);
    SetRect(&im->r_side, 0, 0, SB_W, c.bottom);
    SetRect(&im->r_prev, c.right - PV_W, 0, c.right, c.bottom);
    SetRect(&im->r_main, SB_W, 0, c.right - PV_W, c.bottom);
    SetRect(&im->r_top, im->r_main.left, 0, im->r_main.right, TOP_H);
    SetRect(&im->r_search, im->r_main.left + 16, 12, im->r_main.right - 16 - 2 * 36 - 8, TOP_H - 12);
    SetRect(&im->r_vlist, im->r_search.right + 8, 12, im->r_search.right + 8 + 36, TOP_H - 12);
    SetRect(&im->r_vgrid, im->r_vlist.right + 4, 12, im->r_vlist.right + 4 + 36, TOP_H - 12);
    SetRect(&im->r_head, im->r_main.left, TOP_H, im->r_main.right, im->grid ? TOP_H : TOP_H + HEAD_H);
    SetRect(&im->r_status, im->r_main.left, c.bottom - STATUS_H, im->r_main.right, c.bottom);
    SetRect(&im->r_list, im->r_main.left, im->r_head.bottom, im->r_main.right, im->r_status.top);
    MoveWindow(im->edit, im->r_search.left + 36, im->r_search.top + 9, im->r_search.right - im->r_search.left - 46, 18, TRUE);
    clamp_scroll(im);
}

static void list_cols(Imp *im, int *x)
{
    int L = im->r_list.left + 12, R = im->r_list.right - 14, w = R - L - 30 - 64 - 74;
    x[0] = L;
    x[1] = L + 34;
    x[2] = x[1] + w * 55 / 100;
    x[3] = R - 64 - 74;
    x[4] = R - 74;
    x[5] = R;
}

typedef struct { HDC dc; GpGraphics *g; } Paint;

static GpPath *rr_path(float x, float y, float w, float h, float r)
{
    GpPath *p;
    GdipCreatePath(0, &p);
    float d = r * 2;
    if (d > w) d = w;
    if (d > h) d = h;
    if (d <= 0) d = 0.01f;
    GdipAddPathArc(p, x, y, d, d, 180, 90);
    GdipAddPathArc(p, x + w - d, y, d, d, 270, 90);
    GdipAddPathArc(p, x + w - d, y + h - d, d, d, 0, 90);
    GdipAddPathArc(p, x, y + h - d, d, d, 90, 90);
    GdipClosePathFigure(p);
    return p;
}

static void fill_rr(Paint *pt, RECT r, float rad, ARGB c)
{
    GpPath *p = rr_path((float)r.left, (float)r.top, (float)(r.right - r.left), (float)(r.bottom - r.top), rad);
    GpBrush *b;
    GdipCreateSolidFill(c, &b);
    GdipFillPath(pt->g, b, p);
    GdipDeleteBrush(b);
    GdipDeletePath(p);
}

static void stroke_rr(Paint *pt, RECT r, float rad, ARGB c, float wd)
{
    GpPath *p = rr_path(r.left + 0.5f, r.top + 0.5f, (float)(r.right - r.left - 1), (float)(r.bottom - r.top - 1), rad);
    GpPen *pen;
    GdipCreatePen1(c, wd, 2, &pen);
    GdipDrawPath(pt->g, pen, p);
    GdipDeletePen(pen);
    GdipDeletePath(p);
}

static void fill_rect(Paint *pt, RECT r, COLORREF c)
{
    GpBrush *b;
    GdipCreateSolidFill(ARGB_OF(c, 255), &b);
    GdipFillRectangleI(pt->g, b, r.left, r.top, r.right - r.left, r.bottom - r.top);
    GdipDeleteBrush(b);
}

static void line(Paint *pt, float x1, float y1, float x2, float y2, ARGB c, float wd)
{
    GpPen *pen;
    GdipCreatePen1(c, wd, 2, &pen);
    GdipSetPenStartCap(pen, 2);
    GdipSetPenEndCap(pen, 2);
    GdipDrawLine(pt->g, pen, x1, y1, x2, y2);
    GdipDeletePen(pen);
}

static void text(Paint *pt, const wchar_t *s, RECT r, HFONT f, COLORREF c, UINT fl)
{
    SelectObject(pt->dc, f);
    SetTextColor(pt->dc, c);
    DrawTextW(pt->dc, s, -1, &r, fl | DT_NOPREFIX);
}

static void checker(Paint *pt, RECT r, float rad)
{
    GpPath *p = rr_path((float)r.left, (float)r.top, (float)(r.right - r.left), (float)(r.bottom - r.top), rad);
    GdipSetClipPath(pt->g, p, 0);
    GpBrush *a, *b;
    GdipCreateSolidFill(ARGB_OF(RGB(0x2A, 0x2C, 0x32), 255), &a);
    GdipCreateSolidFill(ARGB_OF(RGB(0x22, 0x24, 0x29), 255), &b);
    for (int y = r.top; y < r.bottom; y += 8)
        for (int x = r.left; x < r.right; x += 8)
            GdipFillRectangleI(pt->g, ((x - r.left) ^ (y - r.top)) & 8 ? a : b, x, y, 8, 8);
    GdipDeleteBrush(a);
    GdipDeleteBrush(b);
    GdipResetClip(pt->g);
    GdipDeletePath(p);
}

static void draw_image_fit(Paint *pt, GpImage *img, int iw, int ih, RECT box, float rad)
{
    int bw = box.right - box.left, bh = box.bottom - box.top, dw = bw, dh = bh;
    if (iw * bh > ih * bw) dh = ih * bw / iw; else dw = iw * bh / ih;
    if (iw <= bw / 2 && ih <= bh / 2) { dw = iw * 2; dh = ih * 2; }
    int x = box.left + (bw - dw) / 2, y = box.top + (bh - dh) / 2;
    GpPath *p = rr_path((float)box.left, (float)box.top, (float)bw, (float)bh, rad);
    GdipSetClipPath(pt->g, p, 0);
    GdipDrawImageRectI(pt->g, img, x, y, dw, dh);
    GdipResetClip(pt->g);
    GdipDeletePath(p);
}

static void badge(Paint *pt, int pass, RECT r, const Row *row)
{
    COLORREF c = k_color[row->kind];
    if (pass == 0) {
        fill_rr(pt, r, 5, ARGB_OF(c, 40));
        stroke_rr(pt, r, 5, ARGB_OF(c, 110), 1);
    } else {
        wchar_t e[6];
        lstrcpynW(e, row->ext, 5);
        CharUpperW(e);
        text(pt, e, r, f_badge, c, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
    }
}

static void icon_search(Paint *pt, int x, int y, ARGB c)
{
    GpPen *pen;
    GdipCreatePen1(c, 1.8f, 2, &pen);
    GdipDrawEllipse(pt->g, pen, (float)x, (float)y, 11, 11);
    GdipDeletePen(pen);
    line(pt, x + 9.5f, y + 9.5f, x + 14.5f, y + 14.5f, c, 1.8f);
}

static void icon_list(Paint *pt, RECT r, ARGB c)
{
    int cx = (r.left + r.right) / 2, cy = (r.top + r.bottom) / 2;
    for (int i = -1; i <= 1; i++) line(pt, cx - 7.0f, cy + i * 5.0f, cx + 7.0f, cy + i * 5.0f, c, 1.8f);
}

static void icon_grid(Paint *pt, RECT r, ARGB c)
{
    int cx = (r.left + r.right) / 2, cy = (r.top + r.bottom) / 2;
    for (int i = 0; i < 4; i++) {
        RECT q = { cx - 8 + (i & 1) * 9, cy - 8 + (i >> 1) * 9, cx - 8 + (i & 1) * 9 + 7, cy - 8 + (i >> 1) * 9 + 7 };
        fill_rr(pt, q, 2, c);
    }
}

static void icon_folder(Paint *pt, int x, int y, ARGB c)
{
    RECT tab = { x, y + 1, x + 7, y + 5 }, body = { x, y + 3, x + 16, y + 14 };
    fill_rr(pt, tab, 1.5f, c);
    fill_rr(pt, body, 2.5f, c);
}

static void chevron(Paint *pt, int x, int y, int open, ARGB c)
{
    if (open) {
        line(pt, x - 4.0f, y - 2.0f, (float)x, y + 2.0f, c, 1.6f);
        line(pt, (float)x, y + 2.0f, x + 4.0f, y - 2.0f, c, 1.6f);
    } else {
        line(pt, x - 2.0f, y - 4.0f, x + 2.0f, (float)y, c, 1.6f);
        line(pt, x + 2.0f, (float)y, x - 2.0f, y + 4.0f, c, 1.6f);
    }
}

static int hovered(Imp *im, int kind, int idx) { return im->hover_kind == kind && im->hover_idx == idx; }

static void build_sidebar(Imp *im)
{
    free(im->sb);
    im->sb = malloc((K_N + im->nfold + 4) * sizeof *im->sb);
    im->nsb = 0;
    int y = 16 - im->sb_y, x = im->r_side.left + 10, w = SB_W - 20;
#define SB_ADD(k, i, h) do { SbItem *s_ = &im->sb[im->nsb++]; s_->kind = (k); s_->idx = (i); SetRect(&s_->rc, x, y, x + w, y + (h)); y += (h); } while (0)
    SB_ADD(0, 0, 26);
    for (int k = 0; k < K_N; k++) SB_ADD(1, k, 32);
    y += 12;
    SB_ADD(0, 1, 26);
    SB_ADD(2, -1, 32);
    for (int o = 0; o < im->nfold; o++) {
        int f = im->forder[o], vis = 1;
        for (int p = im->fold[f].parent; p >= 0 && vis; p = im->fold[p].parent) vis = im->fold[p].expanded;
        if (vis) SB_ADD(3, f, 30);
    }
#undef SB_ADD
}

static void draw_sidebar(Imp *im, Paint *pt, int pass)
{
    if (pass == 0) {
        fill_rect(pt, im->r_side, C_PANEL);
        RECT ln = { im->r_side.right - 1, 0, im->r_side.right, im->r_side.bottom };
        fill_rect(pt, ln, C_LINE);
    }
    HRGN clip = CreateRectRgnIndirect(&im->r_side);
    if (pass) SelectClipRgn(pt->dc, clip);
    for (int i = 0; i < im->nsb; i++) {
        SbItem *s = &im->sb[i];
        RECT r = s->rc;
        if (r.bottom < 0 || r.top > im->r_side.bottom) continue;
        if (s->kind == 0) {
            RECT t = { r.left + 8, r.top + 6, r.right, r.bottom };
            if (pass) {
                SetTextCharacterExtra(pt->dc, 1);
                text(pt, s->idx ? L"ПАПКИ" : L"ТИПЫ", t, f_section, C_FAINT, DT_SINGLELINE | DT_VCENTER);
                SetTextCharacterExtra(pt->dc, 0);
            }
            continue;
        }
        int active = (s->kind == 1 && im->kind_f == s->idx) || (s->kind == 2 && im->fold_f < 0) || (s->kind == 3 && im->fold_f == s->idx);
        int hov = hovered(im, 2, i);
        RECT bg = { r.left, r.top + 1, r.right, r.bottom - 1 };
        int indent = s->kind == 3 ? im->fold[s->idx].depth * 14 : 0;
        int cnt = s->kind == 1 ? im->kcount[s->idx] : s->kind == 2 ? im->nrows : im->fold[s->idx].total;
        wchar_t num[16];
        swprintf(num, 16, L"%d", cnt);
        SIZE nsz;
        SelectObject(pt->dc, f_small);
        GetTextExtentPoint32W(pt->dc, num, (int)wcslen(num), &nsz);
        RECT pill = { r.right - 10 - nsz.cx - 14, r.top + 8, r.right - 10, r.bottom - 8 };
        int ix = r.left + 12 + indent;
        if (pass == 0) {
            if (active) fill_rr(pt, bg, 7, ARGB_OF(C_SEL, 255));
            else if (hov) fill_rr(pt, bg, 7, ARGB_OF(C_HOVER, 160));
            if (active) { RECT bar = { r.left, r.top + 8, r.left + 3, r.bottom - 8 }; fill_rr(pt, bar, 1.5f, ARGB_OF(C_ACCENT, 255)); }
            if (s->kind == 1) {
                RECT dot = { ix + 1, r.top + 12, ix + 9, r.top + 20 };
                fill_rr(pt, dot, 4, ARGB_OF(k_color[s->idx], 255));
            } else {
                if (s->kind == 3 && im->fold[s->idx].kids)
                    chevron(pt, ix + 2, (r.top + r.bottom) / 2, im->fold[s->idx].expanded, ARGB_OF(C_FAINT, 255));
                icon_folder(pt, ix + 10, r.top + 8, ARGB_OF(active ? C_ACCENT : RGB(0xC9, 0xA2, 0x4A), active ? 255 : 200));
            }
            fill_rr(pt, pill, 7, ARGB_OF(active ? C_ACCENT : C_CARD, active ? 90 : 255));
        } else {
            RECT tr = { s->kind == 1 ? ix + 18 : ix + 32, r.top, pill.left - 6, r.bottom };
            const wchar_t *lb = s->kind == 1 ? k_name[s->idx] : s->kind == 2 ? L"Все папки" : im->fold[s->idx].label;
            text(pt, lb, tr, active ? f_bold : f_ui, active ? RGB(0xFF, 0xFF, 0xFF) : C_TEXT, DT_SINGLELINE | DT_VCENTER | DT_END_ELLIPSIS);
            text(pt, num, pill, f_small, active ? RGB(0xFF, 0xFF, 0xFF) : C_DIM, DT_SINGLELINE | DT_VCENTER | DT_CENTER);
        }
    }
    if (pass) SelectClipRgn(pt->dc, NULL);
    DeleteObject(clip);
}

static void draw_topbar(Imp *im, Paint *pt, int pass)
{
    if (pass == 0) {
        fill_rect(pt, im->r_top, C_BG);
        int focus = GetFocus() == im->edit;
        fill_rr(pt, im->r_search, 9, ARGB_OF(C_INPUT, 255));
        stroke_rr(pt, im->r_search, 9, ARGB_OF(focus ? C_ACCENT : C_HOVER, 255), focus ? 1.6f : 1);
        icon_search(pt, im->r_search.left + 13, im->r_search.top + 10, ARGB_OF(focus ? C_ACCENT : C_FAINT, 255));
        RECT *vb[2] = { &im->r_vlist, &im->r_vgrid };
        for (int i = 0; i < 2; i++) {
            int on = im->grid == i;
            if (on) fill_rr(pt, *vb[i], 8, ARGB_OF(C_SEL, 255));
            else if (hovered(im, 4, i)) fill_rr(pt, *vb[i], 8, ARGB_OF(C_HOVER, 255));
            ARGB c = ARGB_OF(on ? RGB(0xFF, 0xFF, 0xFF) : C_DIM, 255);
            if (i) icon_grid(pt, *vb[i], c); else icon_list(pt, *vb[i], c);
        }
    } else if (!GetWindowTextLengthW(im->edit) && GetFocus() != im->edit) {
        RECT t = { im->r_search.left + 36, im->r_search.top, im->r_search.right - 10, im->r_search.bottom };
        text(pt, L"Поиск по имени, пути или типу", t, f_ui, C_FAINT, DT_SINGLELINE | DT_VCENTER);
    }
}

static void draw_list(Imp *im, Paint *pt, int pass)
{
    int x[6];
    list_cols(im, x);
    if (pass == 0) fill_rect(pt, im->r_list, C_BG);
    HRGN clip = CreateRectRgnIndirect(&im->r_list);
    SelectClipRgn(pt->dc, clip);
    int lh = im->r_list.bottom - im->r_list.top;
    if (!im->grid) {
        int first = im->list_y / ROW_H, last = (im->list_y + lh) / ROW_H + 1;
        for (int v = first; v < im->nvis && v <= last; v++) {
            const Row *row = &im->rows[im->vis[v]];
            int top = im->r_list.top + v * ROW_H - im->list_y;
            RECT rr = { im->r_list.left + 6, top + 1, im->r_list.right - 10, top + ROW_H - 1 };
            int sel = im->sel[im->vis[v]], hov = hovered(im, 1, v);
            RECT ic = { x[0], top + 5, x[0] + 24, top + ROW_H - 5 };
            Thumb *th = IS_IMAGE(row) ? thumb_get(im, row->low, 0) : NULL;
            if (pass == 0) {
                if (sel) fill_rr(pt, rr, 7, ARGB_OF(C_SEL, 255));
                else if (hov) fill_rr(pt, rr, 7, ARGB_OF(C_HOVER, 150));
                if (sel && v == im->focus) stroke_rr(pt, rr, 7, ARGB_OF(C_ACCENT, 200), 1);
                if (th && th->state == 1) {
                    checker(pt, ic, 4);
                    draw_image_fit(pt, th->img, th->w, th->h, ic, 4);
                } else
                    badge(pt, 0, ic, row);
                RECT tb = { x[3], top + 8, x[3] + 46, top + ROW_H - 8 };
                badge(pt, 0, tb, row);
            } else {
                if (!(th && th->state == 1)) badge(pt, 1, ic, row);
                RECT tn = { x[1], top, x[2] - 12, top + ROW_H };
                text(pt, row->name, tn, sel ? f_bold : f_ui, sel ? RGB(0xFF, 0xFF, 0xFF) : C_TEXT, DT_SINGLELINE | DT_VCENTER | DT_END_ELLIPSIS);
                wchar_t dir[600];
                int dl = row->dirlen < 599 ? row->dirlen : 599;
                wcsncpy(dir, row->path, dl);
                dir[dl > 0 ? dl - 1 : 0] = 0;
                RECT td = { x[2], top, x[3] - 12, top + ROW_H };
                text(pt, dl ? dir : L"—", td, f_small, C_DIM, DT_SINGLELINE | DT_VCENTER | DT_PATH_ELLIPSIS);
                RECT tb = { x[3], top + 8, x[3] + 46, top + ROW_H - 8 };
                badge(pt, 1, tb, row);
                wchar_t sz[32];
                swprintf(sz, 32, L"%d КБ", row->kb);
                RECT ts = { x[4], top, x[5], top + ROW_H };
                text(pt, sz, ts, f_small, C_DIM, DT_SINGLELINE | DT_VCENTER | DT_RIGHT);
            }
        }
    } else {
        int cols = grid_cols(im), x0 = im->r_list.left + 12 + ((im->r_list.right - im->r_list.left - 12) - cols * TILE_W) / 2;
        int first = im->list_y / TILE_H * cols, last = ((im->list_y + lh) / TILE_H + 1) * cols + cols;
        for (int v = first; v < im->nvis && v < last; v++) {
            const Row *row = &im->rows[im->vis[v]];
            int cx = x0 + (v % cols) * TILE_W, cy = im->r_list.top + 6 + (v / cols) * TILE_H - im->list_y;
            RECT tile = { cx + 4, cy, cx + TILE_W - 4, cy + TILE_H - 8 };
            RECT pic = { tile.left + 10, tile.top + 10, tile.right - 10, tile.top + 10 + (TILE_W - 28) };
            int sel = im->sel[im->vis[v]], hov = hovered(im, 1, v);
            Thumb *th = IS_IMAGE(row) ? thumb_get(im, row->low, 0) : NULL;
            if (pass == 0) {
                fill_rr(pt, tile, 10, ARGB_OF(sel ? C_SEL : hov ? C_HOVER : C_PANEL, 255));
                if (sel) stroke_rr(pt, tile, 10, ARGB_OF(C_ACCENT, v == im->focus ? 255 : 150), v == im->focus ? 1.6f : 1);
                if (th && th->state == 1) {
                    checker(pt, pic, 7);
                    draw_image_fit(pt, th->img, th->w, th->h, pic, 7);
                } else {
                    fill_rr(pt, pic, 7, ARGB_OF(C_CARD, 255));
                    RECT b = { (pic.left + pic.right) / 2 - 26, (pic.top + pic.bottom) / 2 - 14, (pic.left + pic.right) / 2 + 26, (pic.top + pic.bottom) / 2 + 14 };
                    badge(pt, 0, b, row);
                }
            } else {
                if (!(th && th->state == 1)) {
                    RECT b = { (pic.left + pic.right) / 2 - 26, (pic.top + pic.bottom) / 2 - 14, (pic.left + pic.right) / 2 + 26, (pic.top + pic.bottom) / 2 + 14 };
                    badge(pt, 1, b, row);
                }
                RECT tn = { tile.left + 8, pic.bottom + 4, tile.right - 8, tile.bottom - 4 };
                text(pt, row->name, tn, sel ? f_bold : f_small, sel ? RGB(0xFF, 0xFF, 0xFF) : C_TEXT, DT_CENTER | DT_SINGLELINE | DT_VCENTER | DT_END_ELLIPSIS);
            }
        }
    }
    if (!im->nvis && pass) {
        RECT t = im->r_list;
        text(pt, im->nrows ? L"Ничего не найдено" : L"В карте нет импортированных файлов", t, f_ui, C_FAINT, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
    }
    if (im->marq && pass == 0) {
        RECT mr = { im->marq_x0 < im->marq_x1 ? im->marq_x0 : im->marq_x1, (im->marq_y0 < im->marq_y1 ? im->marq_y0 : im->marq_y1) - im->list_y + im->r_list.top,
                    im->marq_x0 < im->marq_x1 ? im->marq_x1 : im->marq_x0, (im->marq_y0 < im->marq_y1 ? im->marq_y1 : im->marq_y0) - im->list_y + im->r_list.top };
        if (mr.right - mr.left < 2) mr.right = mr.left + 2;
        if (mr.bottom - mr.top < 2) mr.bottom = mr.top + 2;
        fill_rr(pt, mr, 3, ARGB_OF(C_ACCENT, 40));
        stroke_rr(pt, mr, 3, ARGB_OF(C_ACCENT, 200), 1);
    }
    int ch = content_h(im);
    if (pass == 0 && ch > lh) {
        int th = lh * lh / ch;
        if (th < 30) th = 30;
        int ty = im->r_list.top + (int)((long long)im->list_y * (lh - th) / (ch - lh));
        RECT sb = { im->r_list.right - 8, ty + 2, im->r_list.right - 3, ty + th - 2 };
        fill_rr(pt, sb, 2.5f, ARGB_OF(C_FAINT, im->drag_scroll || hovered(im, 6, 0) ? 220 : 120));
    }
    SelectClipRgn(pt->dc, NULL);
    DeleteObject(clip);
    if (pass == 0) fill_rect(pt, im->r_head, C_BG);
    if (!im->grid) {
        static const wchar_t *hn[4] = { L"Имя", L"Папка", L"Тип", L"Размер" };
        static const int hc[4] = { 0, 1, 2, 3 };
        for (int i = 0; i < 4; i++) {
            RECT h = { x[i + 1] - (i == 3 ? 0 : 0), im->r_head.top, x[i + 2], im->r_head.bottom };
            if (i == 3) { h.left = x[4]; h.right = x[5]; }
            if (pass) {
                wchar_t lb[32];
                int on = im->sort_col == hc[i];
                swprintf(lb, 32, L"%ls%ls", hn[i], on ? (im->sort_desc ? L"  ↓" : L"  ↑") : L"");
                text(pt, lb, h, on ? f_bold : f_small, on ? C_TEXT : (hovered(im, 5, i) ? C_TEXT : C_FAINT), DT_SINGLELINE | DT_VCENTER | (i == 3 ? DT_RIGHT : 0));
            }
        }
        if (pass == 0) { RECT ln = { im->r_list.left + 12, im->r_head.bottom - 1, im->r_list.right - 12, im->r_head.bottom }; fill_rect(pt, ln, C_LINE); }
    }
}

static void draw_status(Imp *im, Paint *pt, int pass)
{
    if (pass == 0) {
        fill_rect(pt, im->r_status, C_PANEL);
        RECT ln = { im->r_status.left, im->r_status.top, im->r_status.right, im->r_status.top + 1 };
        fill_rect(pt, ln, C_LINE);
        return;
    }
    int kb, n = sel_count(im, &kb);
    wchar_t s[200];
    if (n) swprintf(s, 200, L"Показано %d из %d  ·  выделено %d (%d КБ)", im->nvis, im->nrows, n, kb);
    else swprintf(s, 200, L"Показано %d из %d", im->nvis, im->nrows);
    RECT t = { im->r_status.left + 16, im->r_status.top, im->r_status.right - 16, im->r_status.bottom };
    text(pt, s, t, f_small, C_DIM, DT_SINGLELINE | DT_VCENTER);
    static const wchar_t *hints[2] = { L"Enter: свойства   Del: удалить   Ctrl+E: экспорт   Ctrl+C: путь", L"Enter · Del · Ctrl+E · Ctrl+C" };
    SIZE a, b;
    GetTextExtentPoint32W(pt->dc, s, (int)wcslen(s), &a);
    for (int i = 0; i < 2; i++) {
        GetTextExtentPoint32W(pt->dc, hints[i], (int)wcslen(hints[i]), &b);
        if (a.cx + b.cx + 40 < t.right - t.left) {
            text(pt, hints[i], t, f_small, C_FAINT, DT_SINGLELINE | DT_VCENTER | DT_RIGHT);
            break;
        }
    }
}

static void add_btn(Imp *im, int id, const wchar_t *lb, int danger, RECT r)
{
    Btn *b = &im->btn[im->nbtn++];
    b->id = id;
    b->label = lb;
    b->danger = danger;
    b->rc = r;
}

static void draw_preview(Imp *im, Paint *pt, int pass)
{
    RECT P = im->r_prev;
    if (pass == 0) {
        fill_rect(pt, P, C_PANEL);
        RECT ln = { P.left, 0, P.left + 1, P.bottom };
        fill_rect(pt, ln, C_LINE);
    }
    int pad = 18, w = P.right - P.left - 2 * pad, y = 18, kb;
    int nsel = sel_count(im, &kb), r = focus_row(im);
    if (pass == 0) im->nbtn = 0;
    if (nsel == 0 || r < 0) {
        if (pass) {
            RECT t = { P.left + pad, P.top, P.right - pad, P.bottom };
            text(pt, L"Выберите файл", t, f_ui, C_FAINT, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
        }
        return;
    }
    const Row *row = &im->rows[r];
    int side = (int)(P.bottom - P.top) - y - (nsel == 1 ? 380 : 190);
    if (side > w) side = w;
    if (side < 96) side = 96;
    RECT box = { P.left + pad + (w - side) / 2, y, P.left + pad + (w - side) / 2 + side, y + side };
    im->r_thumb = box;
    if (nsel == 1) {
        if (pass == 0) {
            if (im->prev_img && im->prev_row == r) {
                checker(pt, box, 12);
                draw_image_fit(pt, im->prev_img, im->prev_w, im->prev_h, box, 12);
            } else {
                fill_rr(pt, box, 12, ARGB_OF(C_CARD, 255));
                RECT b = { (box.left + box.right) / 2 - 44, (box.top + box.bottom) / 2 - 24, (box.left + box.right) / 2 + 44, (box.top + box.bottom) / 2 + 24 };
                badge(pt, 0, b, row);
            }
            stroke_rr(pt, box, 12, ARGB_OF(C_LINE, 255), 1);
        } else if (!(im->prev_img && im->prev_row == r)) {
            RECT b = { (box.left + box.right) / 2 - 44, (box.top + box.bottom) / 2 - 24, (box.left + box.right) / 2 + 44, (box.top + box.bottom) / 2 + 24 };
            HFONT old = f_badge;
            f_badge = f_title;
            badge(pt, 1, b, row);
            f_badge = old;
        }
        y = box.bottom + 16;
        RECT tn = { P.left + pad, y, P.right - pad, y + 44 };
        if (pass) {
            RECT calc = tn;
            SelectObject(pt->dc, f_title);
            DrawTextW(pt->dc, row->name, -1, &calc, DT_CALCRECT | DT_WORDBREAK | DT_NOPREFIX | DT_EDITCONTROL);
            tn.bottom = tn.top + (calc.bottom - calc.top);
            text(pt, row->name, tn, f_title, C_TEXT, DT_WORDBREAK | DT_EDITCONTROL);
        }
        y += 48;
        wchar_t chips[3][48];
        int nc = 0;
        swprintf(chips[nc++], 48, L"%ls", row->type);
        swprintf(chips[nc++], 48, L"%d КБ", row->kb);
        if (im->prev_img && im->prev_row == r) swprintf(chips[nc++], 48, L"%d × %d", im->prev_w, im->prev_h);
        int cx = P.left + pad;
        for (int i = 0; i < nc; i++) {
            SIZE s;
            SelectObject(pt->dc, f_small);
            GetTextExtentPoint32W(pt->dc, chips[i], (int)wcslen(chips[i]), &s);
            if (cx + s.cx + 20 > P.right - pad) { cx = P.left + pad; y += 30; }
            RECT c = { cx, y, cx + s.cx + 20, y + 24 };
            if (pass == 0) fill_rr(pt, c, 12, ARGB_OF(i == 0 ? k_color[row->kind] : C_CARD, i == 0 ? 45 : 255));
            else text(pt, chips[i], c, f_small, i == 0 ? k_color[row->kind] : C_DIM, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
            cx = c.right + 6;
        }
        y += 36;
        if (pass) {
            RECT tp = { P.left + pad, y, P.right - pad, y + 60 };
            text(pt, L"ПУТЬ", tp, f_section, C_FAINT, DT_SINGLELINE);
            tp.top += 18;
            text(pt, row->path, tp, f_small, C_DIM, DT_WORDBREAK | DT_EDITCONTROL);
            if (im->prev_img && im->prev_row == r && im->prev_fmt[0]) {
                RECT tf = { P.left + pad, y + 64, P.right - pad, y + 84 };
                text(pt, im->prev_fmt, tf, f_small, C_FAINT, DT_SINGLELINE);
            }
        }
        y += 96;
    } else {
        if (pass == 0) fill_rr(pt, box, 12, ARGB_OF(C_CARD, 255));
        else {
            wchar_t s[80];
            swprintf(s, 80, L"%d", nsel);
            RECT a = { box.left, box.top + 40, box.right, box.top + (box.bottom - box.top) / 2 + 10 };
            HFONT big = CreateFontW(-64, 0, 0, 0, FW_SEMIBOLD, 0, 0, 0, DEFAULT_CHARSET, 0, 0, CLEARTYPE_QUALITY, 0, L"Segoe UI");
            text(pt, s, a, big, C_TEXT, DT_CENTER | DT_BOTTOM | DT_SINGLELINE);
            DeleteObject(big);
            swprintf(s, 80, L"файлов выделено  ·  %d КБ", kb);
            RECT b = { box.left, a.bottom + 4, box.right, a.bottom + 30 };
            text(pt, s, b, f_ui, C_DIM, DT_CENTER | DT_TOP | DT_SINGLELINE);
        }
        y = box.bottom + 20;
    }
    int bw = (w - 8) / 2, bh = 34;
    if (pass == 0) {
        if (nsel == 1) {
            add_btn(im, CMD_PROPS, L"Свойства", 0, (RECT){ P.left + pad, y, P.left + pad + bw, y + bh });
            add_btn(im, CMD_REPLACE, L"Заменить…", 0, (RECT){ P.left + pad + bw + 8, y, P.right - pad, y + bh });
            y += bh + 8;
        }
        add_btn(im, CMD_EXPORT, nsel > 1 ? L"Экспорт…" : L"Экспорт…", 0, (RECT){ P.left + pad, y, P.left + pad + bw, y + bh });
        add_btn(im, CMD_COPYPATH, nsel > 1 ? L"Копировать пути" : L"Копировать путь", 0, (RECT){ P.left + pad + bw + 8, y, P.right - pad, y + bh });
        y += bh + 8;
        add_btn(im, CMD_DELETE, nsel > 1 ? L"Удалить выделенные" : L"Удалить", 1, (RECT){ P.left + pad, y, P.right - pad, y + bh });
    }
    for (int i = 0; i < im->nbtn; i++) {
        Btn *b = &im->btn[i];
        int hov = hovered(im, 3, i);
        if (pass == 0) {
            if (b->danger) {
                fill_rr(pt, b->rc, 8, ARGB_OF(C_DANGER, hov ? 60 : 22));
                stroke_rr(pt, b->rc, 8, ARGB_OF(C_DANGER, hov ? 200 : 110), 1);
            } else {
                fill_rr(pt, b->rc, 8, ARGB_OF(hov ? C_HOVER : C_CARD, 255));
            }
        } else
            text(pt, b->label, b->rc, f_ui, b->danger ? RGB(0xFF, 0x8A, 0x87) : C_TEXT, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
    }
}

static void paint(Imp *im, HDC wdc)
{
    RECT c;
    GetClientRect(im->view, &c);
    if (!im->bb_dc || im->bb_w != c.right || im->bb_h != c.bottom) {
        if (im->bb_dc) { SelectObject(im->bb_dc, im->bb_old); DeleteObject(im->bb_bmp); DeleteDC(im->bb_dc); }
        BITMAPINFO bi = { { sizeof(BITMAPINFOHEADER), c.right, -c.bottom, 1, 32, BI_RGB } };
        void *bits;
        im->bb_dc = CreateCompatibleDC(wdc);
        im->bb_bmp = CreateDIBSection(wdc, &bi, DIB_RGB_COLORS, &bits, NULL, 0);
        im->bb_old = SelectObject(im->bb_dc, im->bb_bmp);
        im->bb_w = c.right;
        im->bb_h = c.bottom;
    }
    HDC dc = im->bb_dc;
    SetBkMode(dc, TRANSPARENT);
    build_sidebar(im);
    Paint pt = { dc, NULL };
    for (int pass = 0; pass < 2; pass++) {
        if (pass == 0) {
            GdipCreateFromHDC(dc, &pt.g);
            GdipSetSmoothingMode(pt.g, 4);
            GdipSetPixelOffsetMode(pt.g, 4);
            GdipSetInterpolationMode(pt.g, 7);
        }
        draw_sidebar(im, &pt, pass);
        draw_list(im, &pt, pass);
        draw_topbar(im, &pt, pass);
        draw_status(im, &pt, pass);
        draw_preview(im, &pt, pass);
        if (pass == 0) { GdipDeleteGraphics(pt.g); pt.g = NULL; }
    }
    BitBlt(wdc, 0, 0, c.right, c.bottom, dc, 0, 0, SRCCOPY);
}

static void ensure_thumbs(Imp *im)
{
    if (im->view) SetTimer(im->view, TIMER_THUMB, 30, NULL);
}

static void thumb_tick(Imp *im)
{
    DWORD t0 = GetTickCount();
    int lh = im->r_list.bottom - im->r_list.top, first, last;
    if (im->grid) {
        int cols = grid_cols(im);
        first = im->list_y / TILE_H * cols;
        last = ((im->list_y + lh) / TILE_H + 1) * cols + cols;
    } else {
        first = im->list_y / ROW_H;
        last = (im->list_y + lh) / ROW_H + 1;
    }
    int keep = focus_row(im), did = 0;
    for (int v = first; v < im->nvis && v <= last; v++) {
        Row *row = &im->rows[im->vis[v]];
        if (!IS_IMAGE(row)) continue;
        Thumb *th = thumb_get(im, row->low, 1);
        if (th->state) continue;
        int w, h;
        wchar_t fmt[64];
        uint8_t *px = load_row_image(im, im->vis[v], &w, &h, fmt);
        th->state = 2;
        if (px) {
            th->img = make_image(px, w, h, 96);
            free(px);
            if (th->img) { th->state = 1; th->w = w > h ? 96 : 96 * w / h; th->h = h >= w ? 96 : 96 * h / w; if (w <= 96 && h <= 96) { th->w = w; th->h = h; } }
        }
        did++;
        if (GetTickCount() - t0 > 12) break;
    }
    if (did) {
        if (keep >= 0) select_we(im, keep);
        InvalidateRect(im->view, &im->r_list, FALSE);
    } else
        KillTimer(im->view, TIMER_THUMB);
}

static void update_preview(Imp *im)
{
    int r = focus_row(im), n = sel_count(im, NULL);
    if (r >= 0) select_we(im, r);
    if (n == 1 && r != im->prev_row) {
        if (im->prev_img) { GdipDisposeImage(im->prev_img); im->prev_img = NULL; }
        im->prev_row = r;
        im->prev_fmt[0] = 0;
        int w, h;
        uint8_t *px = load_row_image(im, r, &w, &h, im->prev_fmt);
        if (px) {
            im->prev_img = make_image(px, w, h, 512);
            im->prev_w = w;
            im->prev_h = h;
            free(px);
        }
        if (r >= 0) select_we(im, r);
    }
    InvalidateRect(im->view, NULL, FALSE);
}

static void refresh(Imp *im)
{
    int n = sel_count(im, NULL), nk = 0;
    wchar_t **keep = n ? malloc(n * sizeof *keep) : NULL;
    for (int i = 0; i < im->nrows && nk < n; i++)
        if (im->sel[i]) keep[nk++] = _wcsdup(im->rows[i].path);
    wchar_t *focus = focus_row(im) >= 0 ? _wcsdup(im->rows[focus_row(im)].path) : NULL;
    wchar_t *curpre = im->fold_f >= 0 && im->fold_f < im->nfold ? _wcsdup(im->fold[im->fold_f].prefix) : NULL;
    int ne = 0;
    wchar_t **exp = im->nfold ? malloc(im->nfold * sizeof *exp) : NULL;
    for (int i = 0; i < im->nfold; i++)
        if (im->fold[i].expanded) exp[ne++] = _wcsdup(im->fold[i].prefix);
    int y = im->list_y;
    read_we(im);
    for (int i = 0; i < ne; i++) {
        int f = find_fold(im, exp[i]);
        if (f >= 0) im->fold[f].expanded = 1;
        free(exp[i]);
    }
    free(exp);
    im->fold_f = curpre ? find_fold(im, curpre) : -1;
    free(curpre);
    for (int i = 0; i < im->nrows; i++)
        for (int k = 0; k < nk; k++)
            if (keep[k] && !_wcsicmp(im->rows[i].path, keep[k])) { im->sel[i] = 1; break; }
    for (int k = 0; k < nk; k++) free(keep[k]);
    free(keep);
    apply_filter(im);
    im->list_y = y;
    clamp_scroll(im);
    for (int v = 0; focus && v < im->nvis; v++)
        if (!_wcsicmp(im->rows[im->vis[v]].path, focus)) im->focus = v;
    free(focus);
    im->prev_row = -2;
    update_preview(im);
}

static Imp *g_cmp_im;
static int cmp_we_desc_q(const void *a, const void *b) { return g_cmp_im->rows[*(const int *)b].we - g_cmp_im->rows[*(const int *)a].we; }

static void batch_delete(Imp *im, int *rows, int n)
{
    wchar_t msg[200];
    swprintf(msg, 200, L"Удалить из карты %d файлов?", n);
    if (MessageBoxW(im->dlg, msg, L"Менеджер импорта", MB_YESNO | MB_ICONQUESTION) != IDYES) return;
    g_cmp_im = im;
    qsort(rows, n, sizeof *rows, cmp_we_desc_q);
    g_auto++;
    for (int i = 0; i < n; i++)
        if (select_we(im, rows[i])) we_command(im, CMD_DELETE);
    g_auto--;
    memset(im->sel, 0, im->nrows);
    refresh(im);
}

static void make_dirs_for(wchar_t *path)
{
    for (wchar_t *p = path + 3; *p; p++)
        if (*p == L'\\') { *p = 0; CreateDirectoryW(path, NULL); *p = L'\\'; }
}

static void batch_export(Imp *im, int *rows, int n)
{
    BROWSEINFOW bi = { 0 };
    wchar_t dir[MAX_PATH];
    bi.hwndOwner = im->dlg;
    bi.lpszTitle = L"Папка для экспорта (пути файлов сохранятся)";
    bi.ulFlags = BIF_RETURNONLYFSDIRS | BIF_NEWDIALOGSTYLE;
    LPITEMIDLIST id = SHBrowseForFolderW(&bi);
    if (!id || !SHGetPathFromIDListW(id, dir)) return;
    CoTaskMemFree(id);
    int fails = 0;
    g_auto++;
    for (int i = 0; i < n; i++) {
        Row *r = &im->rows[rows[i]];
        wchar_t full[MAX_PATH * 2], sh[MAX_PATH];
        swprintf(full, MAX_PATH * 2, L"%ls\\%ls", dir, r->path);
        make_dirs_for(full);
        BOOL lossy = FALSE;
        char a[MAX_PATH];
        int okc = WideCharToMultiByte(CP_ACP, WC_NO_BEST_FIT_CHARS, full, -1, a, MAX_PATH, NULL, &lossy);
        if (!okc || lossy) {
            HANDLE f = CreateFileW(full, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, 0, NULL);
            if (f != INVALID_HANDLE_VALUE) CloseHandle(f);
            if (!GetShortPathNameW(full, sh, MAX_PATH) || !WideCharToMultiByte(CP_ACP, 0, sh, -1, a, MAX_PATH, NULL, NULL)) { fails++; continue; }
        }
        if (!select_we(im, rows[i])) { fails++; continue; }
        lstrcpynA(g_save_to, a, MAX_PATH);
        g_save_set = 1;
        we_command(im, CMD_EXPORT);
        if (g_save_set) fails++;
        g_save_set = 0;
    }
    g_auto--;
    wchar_t msg[300];
    if (fails) swprintf(msg, 300, L"Выгружено %d из %d файлов в\n%ls", n - fails, n, dir);
    else swprintf(msg, 300, L"Выгружено %d файлов в\n%ls", n, dir);
    MessageBoxW(im->dlg, msg, L"Менеджер импорта", MB_OK | MB_ICONINFORMATION);
}

static void copy_paths(Imp *im, int *rows, int n)
{
    size_t len = 1;
    for (int i = 0; i < n; i++) len += wcslen(im->rows[rows[i]].path) + 2;
    HGLOBAL g = GlobalAlloc(GMEM_MOVEABLE, len * sizeof(wchar_t));
    if (!g) return;
    wchar_t *s = GlobalLock(g);
    s[0] = 0;
    for (int i = 0; i < n; i++) {
        wcscat(s, im->rows[rows[i]].path);
        if (i + 1 < n) wcscat(s, L"\r\n");
    }
    GlobalUnlock(g);
    if (OpenClipboard(im->dlg)) {
        EmptyClipboard();
        SetClipboardData(CF_UNICODETEXT, g);
        CloseClipboard();
    } else
        GlobalFree(g);
}

static int on_command(Imp *im, int cmd)
{
    HWND f = GetFocus();
    if (cmd == CMD_DELETE && f == im->edit) {
        SendMessageW(im->edit, WM_KEYDOWN, VK_DELETE, 0);
        return 1;
    }
    if (cmd != CMD_PROPS && cmd != CMD_REPLACE && cmd != CMD_DELETE && cmd != CMD_EXPORT && cmd != CMD_COPYPATH) return 0;
    int n = sel_count(im, NULL);
    if (n <= 0) return 1;
    int *rows = malloc(n * sizeof *rows);
    n = sel_rows(im, rows, n);
    if (cmd == CMD_COPYPATH)
        copy_paths(im, rows, n);
    else if (n == 1 || cmd == CMD_PROPS || cmd == CMD_REPLACE) {
        if (select_we(im, focus_row(im))) we_command(im, cmd);
        SetTimer(im->view, TIMER_REFRESH, 60, NULL);
    } else if (cmd == CMD_DELETE)
        batch_delete(im, rows, n);
    else
        batch_export(im, rows, n);
    free(rows);
    return 1;
}

static int hit_row(Imp *im, POINT p)
{
    if (!PtInRect(&im->r_list, p) || p.x > im->r_list.right - 10) return -1;
    int y = p.y - im->r_list.top + im->list_y;
    if (!im->grid) {
        int v = y / ROW_H;
        return v < im->nvis ? v : -1;
    }
    int cols = grid_cols(im), x0 = im->r_list.left + 12 + ((im->r_list.right - im->r_list.left - 12) - cols * TILE_W) / 2;
    int c = (p.x - x0) / TILE_W, r = (y - 6) / TILE_H;
    if (p.x < x0 || c >= cols || y < 6) return -1;
    int v = r * cols + c;
    return v < im->nvis ? v : -1;
}

static void hover_at(Imp *im, POINT p, int *kind, int *idx)
{
    *kind = 0;
    *idx = -1;
    if (PtInRect(&im->r_side, p)) {
        for (int i = 0; i < im->nsb; i++)
            if (im->sb[i].kind && PtInRect(&im->sb[i].rc, p)) { *kind = 2; *idx = i; return; }
        return;
    }
    for (int i = 0; i < im->nbtn; i++)
        if (PtInRect(&im->btn[i].rc, p)) { *kind = 3; *idx = i; return; }
    if (PtInRect(&im->r_vlist, p)) { *kind = 4; *idx = 0; return; }
    if (PtInRect(&im->r_vgrid, p)) { *kind = 4; *idx = 1; return; }
    if (!im->grid && PtInRect(&im->r_head, p)) {
        int x[6];
        list_cols(im, x);
        *kind = 5;
        *idx = p.x >= x[4] ? 3 : p.x >= x[3] ? 2 : p.x >= x[2] ? 1 : 0;
        return;
    }
    if (PtInRect(&im->r_list, p) && p.x > im->r_list.right - 12) { *kind = 6; *idx = 0; return; }
    int v = hit_row(im, p);
    if (v >= 0) { *kind = 1; *idx = v; }
}

static void ensure_visible(Imp *im, int v)
{
    int lh = im->r_list.bottom - im->r_list.top, top, bot;
    if (im->grid) { int cols = grid_cols(im); top = 6 + v / cols * TILE_H; bot = top + TILE_H; }
    else { top = v * ROW_H; bot = top + ROW_H; }
    if (top < im->list_y) im->list_y = top - 4;
    if (bot > im->list_y + lh) im->list_y = bot - lh + 4;
    clamp_scroll(im);
}

static void select_click(Imp *im, int v, int ctrl, int shift)
{
    if (v < 0 || v >= im->nvis) return;
    if (shift && im->anchor >= 0) {
        if (!ctrl) memset(im->sel, 0, im->nrows);
        int a = im->anchor < v ? im->anchor : v, b = im->anchor < v ? v : im->anchor;
        for (int i = a; i <= b; i++) im->sel[im->vis[i]] = 1;
    } else if (ctrl) {
        im->sel[im->vis[v]] ^= 1;
        im->anchor = v;
    } else {
        memset(im->sel, 0, im->nrows);
        im->sel[im->vis[v]] = 1;
        im->anchor = v;
    }
    im->focus = v;
    ensure_visible(im, v);
    SetTimer(im->view, TIMER_SEL, 90, NULL);
    InvalidateRect(im->view, NULL, FALSE);
}

static void show_menu(Imp *im, POINT sp)
{
    if (!sel_count(im, NULL)) return;
    HMENU m = CreatePopupMenu();
    AppendMenuW(m, MF_STRING, CMD_PROPS, L"Свойства\tEnter");
    AppendMenuW(m, MF_STRING, CMD_REPLACE, L"Заменить файл…\tCtrl+R");
    AppendMenuW(m, MF_STRING, CMD_EXPORT, L"Экспорт…\tCtrl+E");
    AppendMenuW(m, MF_STRING, CMD_COPYPATH, L"Копировать путь\tCtrl+C");
    AppendMenuW(m, MF_SEPARATOR, 0, NULL);
    AppendMenuW(m, MF_STRING, CMD_DELETE, L"Удалить\tDelete");
    int cmd = TrackPopupMenu(m, TPM_RETURNCMD | TPM_RIGHTBUTTON, sp.x, sp.y, 0, im->dlg, NULL);
    DestroyMenu(m);
    if (cmd) on_command(im, cmd);
}

static void click_sidebar(Imp *im, int i, POINT p, int dbl)
{
    SbItem *s = &im->sb[i];
    if (s->kind == 1) { im->kind_f = s->idx; apply_filter(im); return; }
    if (s->kind == 2) { im->fold_f = -1; apply_filter(im); return; }
    if (s->kind == 3) {
        Fold *f = &im->fold[s->idx];
        int chev_x = s->rc.left + 12 + f->depth * 14;
        if (f->kids && (dbl || p.x < chev_x + 9)) { f->expanded = !f->expanded; InvalidateRect(im->view, NULL, FALSE); if (p.x < chev_x + 9) return; }
        im->fold_f = s->idx;
        apply_filter(im);
    }
}

static void marquee_apply(Imp *im)
{
    int x0 = im->marq_x0 < im->marq_x1 ? im->marq_x0 : im->marq_x1, x1 = im->marq_x0 < im->marq_x1 ? im->marq_x1 : im->marq_x0;
    int y0 = im->marq_y0 < im->marq_y1 ? im->marq_y0 : im->marq_y1, y1 = im->marq_y0 < im->marq_y1 ? im->marq_y1 : im->marq_y0;
    memcpy(im->sel, im->sel_base, im->nrows);
    int cols = im->grid ? grid_cols(im) : 1;
    int gx0 = im->r_list.left + 12 + ((im->r_list.right - im->r_list.left - 12) - cols * TILE_W) / 2;
    for (int v = 0; v < im->nvis; v++) {
        int top, bot, left, right;
        if (im->grid) {
            left = gx0 + (v % cols) * TILE_W + 4;
            right = left + TILE_W - 8;
            top = 6 + (v / cols) * TILE_H;
            bot = top + TILE_H - 8;
        } else {
            left = im->r_list.left;
            right = im->r_list.right;
            top = v * ROW_H;
            bot = top + ROW_H;
        }
        if (top < y1 && bot > y0 && left < x1 && right > x0) {
            im->sel[im->vis[v]] = 1;
            im->focus = v;
        }
    }
}

static void marquee_move(Imp *im, POINT p)
{
    if (p.x < im->r_list.left) p.x = im->r_list.left;
    if (p.x > im->r_list.right) p.x = im->r_list.right;
    int y = p.y;
    if (y < im->r_list.top) y = im->r_list.top;
    if (y > im->r_list.bottom) y = im->r_list.bottom;
    im->marq_x1 = p.x;
    im->marq_y1 = y - im->r_list.top + im->list_y;
    marquee_apply(im);
    int edge = p.y < im->r_list.top + 24 || p.y > im->r_list.bottom - 24;
    if (edge) SetTimer(im->view, TIMER_AUTOSCROLL, 30, NULL);
    else KillTimer(im->view, TIMER_AUTOSCROLL);
    InvalidateRect(im->view, NULL, FALSE);
}

static LRESULT CALLBACK view_proc(HWND h, UINT m, WPARAM w, LPARAM l)
{
    Imp *im = (Imp *)GetWindowLongPtrW(h, GWLP_USERDATA);
    if (!im) return DefWindowProcW(h, m, w, l);
    switch (m) {
    case WM_ERASEBKGND:
        return 1;
    case WM_PAINT: {
        PAINTSTRUCT ps;
        HDC dc = BeginPaint(h, &ps);
        paint(im, dc);
        EndPaint(h, &ps);
        return 0;
    }
    case WM_PRINTCLIENT:
        paint(im, (HDC)w);
        return 0;
    case WM_SIZE:
        layout(im);
        InvalidateRect(h, NULL, FALSE);
        return 0;
    case WM_CTLCOLOREDIT:
        SetTextColor((HDC)w, C_TEXT);
        SetBkColor((HDC)w, C_INPUT);
        {
            static HBRUSH br;
            if (!br) br = CreateSolidBrush(C_INPUT);
            return (LRESULT)br;
        }
    case WM_COMMAND:
        if ((HWND)l == im->edit) {
            if (HIWORD(w) == EN_CHANGE) apply_filter(im);
            if (HIWORD(w) == EN_SETFOCUS || HIWORD(w) == EN_KILLFOCUS) InvalidateRect(h, &im->r_top, FALSE);
        }
        return 0;
    case WM_GETDLGCODE:
        return DLGC_WANTALLKEYS | DLGC_WANTARROWS | DLGC_WANTCHARS;
    case WM_SETFOCUS:
    case WM_KILLFOCUS:
        InvalidateRect(h, NULL, FALSE);
        return 0;
    case WM_MOUSEMOVE: {
        POINT p = { GET_X_LPARAM(l), GET_Y_LPARAM(l) };
        if (im->press) {
            if (!im->marq && abs(p.x - im->marq_x0) + abs(p.y - im->r_list.top + im->list_y - im->marq_y0) > 5) im->marq = 1;
            if (im->marq) marquee_move(im, p);
            return 0;
        }
        if (im->drag_scroll) {
            int lh = im->r_list.bottom - im->r_list.top, ch = content_h(im);
            int th = lh * lh / (ch > 0 ? ch : 1);
            if (th < 30) th = 30;
            if (lh > th) im->list_y = (p.y - im->drag_off - im->r_list.top) * (ch - lh) / (lh - th);
            clamp_scroll(im);
            ensure_thumbs(im);
            InvalidateRect(h, NULL, FALSE);
            return 0;
        }
        int k, i;
        hover_at(im, p, &k, &i);
        if (k != im->hover_kind || i != im->hover_idx) {
            im->hover_kind = k;
            im->hover_idx = i;
            InvalidateRect(h, NULL, FALSE);
            TRACKMOUSEEVENT t = { sizeof t, TME_LEAVE, h, 0 };
            TrackMouseEvent(&t);
            SetCursor(LoadCursor(NULL, k == 2 || k == 3 || k == 4 || k == 5 ? IDC_HAND : IDC_ARROW));
        }
        return 0;
    }
    case WM_MOUSELEAVE:
        im->hover_kind = 0;
        im->hover_idx = -1;
        InvalidateRect(h, NULL, FALSE);
        return 0;
    case WM_LBUTTONDOWN:
    case WM_LBUTTONDBLCLK: {
        POINT p = { GET_X_LPARAM(l), GET_Y_LPARAM(l) };
        int k, i, dbl = m == WM_LBUTTONDBLCLK;
        hover_at(im, p, &k, &i);
        if (PtInRect(&im->r_search, p)) { SetFocus(im->edit); return 0; }
        SetFocus(h);
        if (k == 2) click_sidebar(im, i, p, dbl);
        else if (k == 3) on_command(im, im->btn[i].id);
        else if (k == 4) { im->grid = i; layout(im); clamp_scroll(im); ensure_thumbs(im); InvalidateRect(h, NULL, FALSE); }
        else if (k == 5) { im->sort_desc = im->sort_col == i ? !im->sort_desc : i == 3; im->sort_col = i; apply_filter(im); }
        else if (k == 6) {
            int lh = im->r_list.bottom - im->r_list.top, ch = content_h(im);
            if (ch > lh) {
                int th = lh * lh / ch;
                if (th < 30) th = 30;
                int ty = im->r_list.top + (int)((long long)im->list_y * (lh - th) / (ch - lh));
                im->drag_off = p.y >= ty && p.y < ty + th ? p.y - ty : th / 2;
                im->drag_scroll = 1;
                SetCapture(h);
                SendMessageW(h, WM_MOUSEMOVE, w, l);
            }
        } else if (k == 1 || (PtInRect(&im->r_list, p) && k == 0)) {
            int ctrl = (w & MK_CONTROL) != 0, shift = (w & MK_SHIFT) != 0;
            if (k == 1 && dbl) { select_click(im, i, 0, 0); on_command(im, CMD_PROPS); return 0; }
            free(im->sel_base);
            im->sel_base = calloc(im->nrows > 0 ? im->nrows : 1, 1);
            if (ctrl) memcpy(im->sel_base, im->sel, im->nrows);
            if (k == 1) select_click(im, i, ctrl, shift);
            else if (!ctrl && !shift) { memset(im->sel, 0, im->nrows); InvalidateRect(h, NULL, FALSE); }
            if (!shift) {
                im->press = 1;
                im->marq = 0;
                im->marq_x0 = im->marq_x1 = p.x;
                im->marq_y0 = im->marq_y1 = p.y - im->r_list.top + im->list_y;
                SetCapture(h);
            }
        }
        return 0;
    }
    case WM_LBUTTONUP:
        if (im->drag_scroll) { im->drag_scroll = 0; ReleaseCapture(); InvalidateRect(h, NULL, FALSE); }
        if (im->press) {
            im->press = 0;
            KillTimer(h, TIMER_AUTOSCROLL);
            ReleaseCapture();
            if (im->marq) {
                im->marq = 0;
                if (im->focus >= 0) im->anchor = im->focus;
                SetTimer(h, TIMER_SEL, 90, NULL);
            }
            InvalidateRect(h, NULL, FALSE);
        }
        return 0;
    case WM_CAPTURECHANGED:
        if (im->press && (HWND)l != h) { im->press = im->marq = 0; KillTimer(h, TIMER_AUTOSCROLL); InvalidateRect(h, NULL, FALSE); }
        return 0;
    case WM_RBUTTONUP: {
        POINT p = { GET_X_LPARAM(l), GET_Y_LPARAM(l) };
        int v = hit_row(im, p);
        if (v >= 0 && !im->sel[im->vis[v]]) select_click(im, v, 0, 0);
        if (v >= 0) { ClientToScreen(h, &p); show_menu(im, p); }
        return 0;
    }
    case WM_MOUSEWHEEL: {
        POINT p = { GET_X_LPARAM(l), GET_Y_LPARAM(l) };
        ScreenToClient(h, &p);
        int d = -GET_WHEEL_DELTA_WPARAM(w);
        if (PtInRect(&im->r_side, p)) {
            im->sb_y += d * 60 / 120;
            int maxy = im->nsb ? im->sb[im->nsb - 1].rc.bottom + im->sb_y - (im->r_side.bottom - 20) : 0;
            if (im->sb_y > maxy) im->sb_y = maxy;
            if (im->sb_y < 0) im->sb_y = 0;
        } else {
            im->list_y += d * (im->grid ? TILE_H : ROW_H * 3) / 120;
            clamp_scroll(im);
            ensure_thumbs(im);
        }
        InvalidateRect(h, NULL, FALSE);
        return 0;
    }
    case WM_KEYDOWN: {
        int ctrl = GetKeyState(VK_CONTROL) < 0, shift = GetKeyState(VK_SHIFT) < 0;
        int v = im->focus < 0 ? 0 : im->focus, step = 1, page = (im->r_list.bottom - im->r_list.top) / ROW_H - 1;
        if (im->grid) { step = grid_cols(im); page = step * ((im->r_list.bottom - im->r_list.top) / TILE_H); }
        if (page < 1) page = 1;
        switch (w) {
        case VK_DOWN: v += step; break;
        case VK_UP: v -= step; break;
        case VK_RIGHT: if (!im->grid) return 0; v++; break;
        case VK_LEFT: if (!im->grid) return 0; v--; break;
        case VK_NEXT: v += page; break;
        case VK_PRIOR: v -= page; break;
        case VK_HOME: v = 0; break;
        case VK_END: v = im->nvis - 1; break;
        case VK_RETURN: on_command(im, CMD_PROPS); return 0;
        case 'A':
            if (ctrl) { for (int i = 0; i < im->nvis; i++) im->sel[im->vis[i]] = 1; SetTimer(h, TIMER_SEL, 90, NULL); InvalidateRect(h, NULL, FALSE); }
            return 0;
        case 'C':
            if (ctrl) on_command(im, CMD_COPYPATH);
            return 0;
        case 'F':
            if (ctrl) SetFocus(im->edit);
            return 0;
        case VK_ESCAPE:
            memset(im->sel, 0, im->nrows);
            InvalidateRect(h, NULL, FALSE);
            return 0;
        default:
            return 0;
        }
        if (v < 0) v = 0;
        if (v >= im->nvis) v = im->nvis - 1;
        if (v >= 0) select_click(im, v, 0, shift);
        return 0;
    }
    case WM_CHAR:
        if (w >= 32 && w != 127 && !(GetKeyState(VK_CONTROL) < 0)) {
            SetFocus(im->edit);
            SendMessageW(im->edit, WM_CHAR, w, l);
        }
        return 0;
    case WM_TIMER:
        if (w == TIMER_REFRESH) { KillTimer(h, w); refresh(im); }
        else if (w == TIMER_SEL) { KillTimer(h, w); update_preview(im); }
        else if (w == TIMER_THUMB) thumb_tick(im);
        else if (w == TIMER_AUTOSCROLL && im->marq) {
            POINT p;
            GetCursorPos(&p);
            ScreenToClient(h, &p);
            int d = p.y < im->r_list.top + 24 ? -1 : p.y > im->r_list.bottom - 24 ? 1 : 0;
            if (!d) { KillTimer(h, w); return 0; }
            im->list_y += d * (im->grid ? TILE_H / 4 : ROW_H / 2);
            clamp_scroll(im);
            ensure_thumbs(im);
            marquee_move(im, p);
        }
        return 0;
    }
    return DefWindowProcW(h, m, w, l);
}

static LRESULT CALLBACK edit_proc(HWND h, UINT m, WPARAM w, LPARAM l, UINT_PTR id, DWORD_PTR ref)
{
    Imp *im = (Imp *)ref;
    (void)id;
    if (m == WM_GETDLGCODE && l && ((MSG *)l)->message == WM_KEYDOWN && (((MSG *)l)->wParam == VK_RETURN || ((MSG *)l)->wParam == VK_ESCAPE))
        return DLGC_WANTALLKEYS;
    if (m == WM_KEYDOWN) {
        if (w == VK_DOWN || w == VK_RETURN) {
            SetFocus(im->view);
            if (im->nvis && !sel_count(im, NULL)) select_click(im, 0, 0, 0);
            return 0;
        }
        if (w == VK_ESCAPE) { SetWindowTextW(h, L""); return 0; }
        if (w == 'A' && GetKeyState(VK_CONTROL) < 0) { SendMessageW(h, EM_SETSEL, 0, -1); return 0; }
    }
    if (m == WM_CHAR && (w == L'\r' || w == 27 || w == 1)) return 0;
    return fs_def(h, m, w, l);
}

static LRESULT CALLBACK welv_proc(HWND h, UINT m, WPARAM w, LPARAM l, UINT_PTR id, DWORD_PTR ref)
{
    Imp *im = (Imp *)ref;
    (void)id;
    if (m == WM_WINDOWPOSCHANGING) ((WINDOWPOS *)l)->flags &= ~SWP_SHOWWINDOW;
    LRESULT r = fs_def(h, m, w, l);
    switch (m) {
    case LVM_INSERTITEMA: case LVM_INSERTITEMW: case LVM_DELETEITEM: case LVM_DELETEALLITEMS:
    case LVM_SETITEMA: case LVM_SETITEMW: case LVM_SETITEMTEXTA: case LVM_SETITEMTEXTW:
        SetTimer(im->view, TIMER_REFRESH, 60, NULL);
        break;
    case WM_WINDOWPOSCHANGED: {
        RECT rc;
        GetWindowRect(h, &rc);
        MapWindowPoints(NULL, im->dlg, (POINT *)&rc, 2);
        MoveWindow(im->view, rc.left, rc.top, rc.right - rc.left, rc.bottom - rc.top, TRUE);
        break;
    }
    }
    return r;
}

static LRESULT CALLBACK dlg_proc(HWND h, UINT m, WPARAM w, LPARAM l, UINT_PTR id, DWORD_PTR ref)
{
    Imp *im = (Imp *)ref;
    if (m == WM_COMMAND && !g_inner && (!l || (HWND)l == GetDlgItem(h, IDC_WE_TOOLBAR)) && on_command(im, LOWORD(w)))
        return 0;
    if (m == WM_NCDESTROY) {
        LRESULT r = fs_def(h, m, w, l);
        RemovePropW(h, PROP);
        free_rows(im);
        free(im->fold);
        free(im->forder);
        free(im->vis);
        free(im->sb);
        free(im->sel_base);
        if (im->prev_img) GdipDisposeImage(im->prev_img);
        if (im->bb_dc) { SelectObject(im->bb_dc, im->bb_old); DeleteObject(im->bb_bmp); DeleteDC(im->bb_dc); }
        for (int i = 0; i < 1024; i++)
            for (Thumb *t = im->thumbs[i], *n; t; t = n) { n = t->next; if (t->img) GdipDisposeImage(t->img); free(t->key); free(t); }
        free(im);
        return r;
    }
    (void)id;
    return fs_def(h, m, w, l);
}

static int is_class(HWND h, const wchar_t *c)
{
    wchar_t b[40];
    return h && GetClassNameW(h, b, 40) && !lstrcmpiW(b, c);
}

static int has_menu_cmd(HMENU m, UINT id)
{
    int n = GetMenuItemCount(m);
    for (int i = 0; i < n; i++) {
        if (GetMenuItemID(m, i) == id) return 1;
        HMENU s = GetSubMenu(m, i);
        if (s && has_menu_cmd(s, id)) return 1;
    }
    return 0;
}

static void attach(HWND dlg)
{
    if (GetPropW(dlg, PROP)) return;
    HWND lv = GetDlgItem(dlg, IDC_WE_LIST), tb = GetDlgItem(dlg, IDC_WE_TOOLBAR);
    HMENU menu = GetMenu(dlg);
    if (!is_class(lv, L"SysListView32") || !is_class(tb, L"ToolbarWindow32") || !menu ||
        !has_menu_cmd(menu, CMD_DELETE) || !has_menu_cmd(menu, CMD_PROPS) || !has_menu_cmd(menu, CMD_REPLACE))
        return;
    HWND hdr = (HWND)SendMessageW(lv, LVM_GETHEADER, 0, 0);
    if (!hdr || SendMessageW(hdr, HDM_GETITEMCOUNT, 0, 0) != 4) return;
    Imp *im = calloc(1, sizeof *im);
    im->dlg = dlg;
    im->welv = lv;
    im->fold_f = -1;
    im->kind_f = K_ALL;
    im->prev_row = -1;
    im->focus = im->anchor = -1;
    im->sort_col = 1;
    im->hover_idx = -1;
    RECT rc;
    GetWindowRect(lv, &rc);
    MapWindowPoints(NULL, dlg, (POINT *)&rc, 2);
    im->view = CreateWindowExW(0, VIEW_CLASS, L"", WS_CHILD | WS_VISIBLE | WS_TABSTOP | WS_CLIPCHILDREN, rc.left, rc.top,
                               rc.right - rc.left, rc.bottom - rc.top, dlg, NULL, g_inst, NULL);
    if (!im->view) { free(im); return; }
    im->edit = CreateWindowExW(0, L"EDIT", L"", WS_CHILD | WS_VISIBLE | WS_TABSTOP | ES_AUTOHSCROLL, 0, 0, 10, 10, im->view, NULL, g_inst, NULL);
    SendMessageW(im->edit, WM_SETFONT, (WPARAM)f_ui, 0);
    SetPropW(dlg, PROP, (HANDLE)im);
    SetWindowLongPtrW(im->view, GWLP_USERDATA, (LONG_PTR)im);
    ShowWindow(lv, SW_HIDE);
    fs_set(dlg, dlg_proc, 1, (DWORD_PTR)im);
    fs_set(lv, welv_proc, 1, (DWORD_PTR)im);
    fs_set(im->edit, edit_proc, 1, (DWORD_PTR)im);
    layout(im);
    refresh(im);
}

static void CALLBACK on_show(HWINEVENTHOOK hk, DWORD ev, HWND h, LONG obj, LONG child, DWORD tid, DWORD t)
{
    (void)hk; (void)ev; (void)tid; (void)t;
    if (h && obj == OBJID_WINDOW && child == CHILDID_SELF && GetClassLongW(h, GCW_ATOM) == 32770 && !(GetWindowLongW(h, GWL_STYLE) & WS_CHILD))
        attach(h);
}

static HFONT mkfont(int px, int weight)
{
    return CreateFontW(-px, 0, 0, 0, weight, 0, 0, 0, DEFAULT_CHARSET, 0, 0, CLEARTYPE_QUALITY, 0, L"Segoe UI");
}

__declspec(dllexport) int ImportPlusInit(void)
{
    static int done;
    if (done) return done;
    GdipInput gi = { 1, NULL, FALSE, FALSE };
    if (GdiplusStartup(&g_gdip, &gi, NULL)) return done = -4;
    f_ui = mkfont(13, FW_NORMAL);
    f_bold = mkfont(13, FW_SEMIBOLD);
    f_small = mkfont(12, FW_NORMAL);
    f_title = mkfont(17, FW_SEMIBOLD);
    f_badge = mkfont(10, FW_BOLD);
    f_section = mkfont(11, FW_BOLD);
    WNDCLASSW wc = { CS_DBLCLKS, view_proc, 0, 0, g_inst, NULL, LoadCursor(NULL, IDC_ARROW), NULL, NULL, VIEW_CLASS };
    if (!RegisterClassW(&wc)) return done = -2;
    HMODULE we = GetModuleHandleW(NULL);
    o_save = iat_patch(we, "comdlg32.dll", "GetSaveFileNameA", (void *)h_save);
    o_msgbox = iat_patch(we, "user32.dll", "MessageBoxA", (void *)h_msgbox);
    if (!o_save || !o_msgbox) return done = -3;
    return done = SetWinEventHook(EVENT_OBJECT_SHOW, EVENT_OBJECT_SHOW, g_inst, on_show, GetCurrentProcessId(), 0, WINEVENT_INCONTEXT) ? 1 : -1;
}

BOOL WINAPI DllMain(HINSTANCE inst, DWORD reason, LPVOID res)
{
    (void)res;
    if (reason == DLL_PROCESS_ATTACH) {
        g_inst = inst;
        DisableThreadLibraryCalls(inst);
    }
    return TRUE;
}
