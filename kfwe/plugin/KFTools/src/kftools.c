#include <windows.h>
#include <winhttp.h>
#include <richedit.h>
#include <uxtheme.h>
#include <dwmapi.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

typedef struct { UINT32 ver; void *cb; BOOL nothread, nocodecs; } GdipInput;
typedef void GpGraphics, GpBrush, GpPath, GpPen, GpImage, GpFontFamily, GpFont, GpStringFormat;
typedef struct { float X, Y, W, H; } RectF;
int WINAPI GdiplusStartup(ULONG_PTR *, const GdipInput *, void *);
int WINAPI GdipCreateBitmapFromScan0(int, int, int, int, BYTE *, GpImage **);
int WINAPI GdipGetImageGraphicsContext(GpImage *, GpGraphics **);
int WINAPI GdipDisposeImage(GpImage *);
int WINAPI GdipDeleteGraphics(GpGraphics *);
int WINAPI GdipSetSmoothingMode(GpGraphics *, int);
int WINAPI GdipSetPixelOffsetMode(GpGraphics *, int);
int WINAPI GdipSetTextRenderingHint(GpGraphics *, int);
int WINAPI GdipGraphicsClear(GpGraphics *, DWORD);
int WINAPI GdipCreateSolidFill(DWORD, GpBrush **);
int WINAPI GdipDeleteBrush(GpBrush *);
int WINAPI GdipCreatePen1(DWORD, float, int, GpPen **);
int WINAPI GdipDeletePen(GpPen *);
int WINAPI GdipCreatePath(int, GpPath **);
int WINAPI GdipDeletePath(GpPath *);
int WINAPI GdipAddPathArc(GpPath *, float, float, float, float, float, float);
int WINAPI GdipClosePathFigure(GpPath *);
int WINAPI GdipFillPath(GpGraphics *, GpBrush *, GpPath *);
int WINAPI GdipDrawPath(GpGraphics *, GpPen *, GpPath *);
int WINAPI GdipSetClipPath(GpGraphics *, GpPath *, int);
int WINAPI GdipResetClip(GpGraphics *);
int WINAPI GdipFillRectangle(GpGraphics *, GpBrush *, float, float, float, float);
int WINAPI GdipCreateFontFamilyFromName(const WCHAR *, void *, GpFontFamily **);
int WINAPI GdipDeleteFontFamily(GpFontFamily *);
int WINAPI GdipCreateFont(const GpFontFamily *, float, int, int, GpFont **);
int WINAPI GdipDeleteFont(GpFont *);
int WINAPI GdipCreateStringFormat(int, LANGID, GpStringFormat **);
int WINAPI GdipDeleteStringFormat(GpStringFormat *);
int WINAPI GdipSetStringFormatLineAlign(GpStringFormat *, int);
int WINAPI GdipSetStringFormatTrimming(GpStringFormat *, int);
int WINAPI GdipDrawString(GpGraphics *, const WCHAR *, int, const GpFont *, const RectF *, const GpStringFormat *, const GpBrush *);
#define PF_PARGB 0xE200B

#define TOAST_W 290
#define TOAST_H 60
#define SHADOW 10
#define TIMER_FADE 1

static HINSTANCE g_inst;
static HHOOK g_kb;
static BOOL (WINAPI *o_settext)(HWND, LPCSTR);
static DWORD g_tid;
static char g_pt[3][32];
static int g_have;
static char g_raw[128];
static HWND g_toast;
static HDC g_mdc;
static HBITMAP g_dib;
static BYTE *g_bits;
static int g_w, g_h;
static float g_s = 1, g_alpha;
static DWORD g_shown_at;
static ULONG_PTR g_gdip;

static float px(float v) { return v * g_s; }

static HWND main_window(void)
{
    HWND found = NULL;
    for (HWND h = GetTopWindow(NULL); h; h = GetWindow(h, GW_HWNDNEXT)) {
        DWORD pid;
        if (GetWindowThreadProcessId(h, &pid) == g_tid && pid == GetCurrentProcessId() && GetMenu(h) && IsWindowVisible(h)) {
            found = h;
            break;
        }
    }
    return found;
}

static int parse_point(const char *s, char out[3][32])
{
    const char *p = strchr(s, '(');
    if (!p)
        return 0;
    p++;
    for (int i = 0; i < 3; i++) {
        int n = 0;
        while (*p == ' ')
            p++;
        if (*p == '-')
            out[i][n++] = *p++;
        while (n < 30 && *p >= '0' && *p <= '9')
            out[i][n++] = *p++;
        if ((*p == '.' || *p == ',') && p[1] >= '0' && p[1] <= '9') {
            out[i][n++] = '.';
            p++;
            while (n < 31 && *p >= '0' && *p <= '9')
                out[i][n++] = *p++;
        }
        out[i][n] = 0;
        if (!n || (n == 1 && out[i][0] == '-'))
            return 0;
        while (*p == ' ')
            p++;
        if (i < 2 && (*p == ',' || *p == ';'))
            p++;
        else if (i == 2 && *p == ')')
            return 1;
        else if (i == 2)
            return 0;
    }
    return 0;
}

static BOOL WINAPI h_settext(HWND h, LPCSTR s)
{
    char cls[16], pt[3][32];
    if (h && s && strchr(s, '(') && GetClassNameA(h, cls, sizeof cls) && !lstrcmpiA(cls, "Static") && GetMenu(GetParent(h))) {
        size_t n = strcspn(s, "\r\n");
        if (n >= sizeof g_raw)
            n = sizeof g_raw - 1;
        memcpy(g_raw, s, n);
        g_raw[n] = 0;
        if (parse_point(s, pt)) {
            memcpy(g_pt, pt, sizeof pt);
            g_have = 1;
        }
    }
    return o_settext(h, s);
}

static int hook_settext(void)
{
    BYTE *b = (BYTE *)GetModuleHandleW(NULL);
    IMAGE_NT_HEADERS *nt = (IMAGE_NT_HEADERS *)(b + ((IMAGE_DOS_HEADER *)b)->e_lfanew);
    IMAGE_DATA_DIRECTORY dd = nt->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_IMPORT];
    if (!dd.VirtualAddress)
        return 0;
    for (IMAGE_IMPORT_DESCRIPTOR *d = (IMAGE_IMPORT_DESCRIPTOR *)(b + dd.VirtualAddress); d->Name; d++) {
        if (!d->OriginalFirstThunk)
            continue;
        IMAGE_THUNK_DATA32 *nm = (IMAGE_THUNK_DATA32 *)(b + d->OriginalFirstThunk), *iat = (IMAGE_THUNK_DATA32 *)(b + d->FirstThunk);
        for (; nm->u1.Ordinal; nm++, iat++) {
            DWORD old;
            if (IMAGE_SNAP_BY_ORDINAL32(nm->u1.Ordinal) || strcmp((const char *)((IMAGE_IMPORT_BY_NAME *)(b + nm->u1.AddressOfData))->Name, "SetWindowTextA"))
                continue;
            if (!VirtualProtect(&iat->u1.Function, 4, PAGE_READWRITE, &old))
                return 0;
            o_settext = (BOOL (WINAPI *)(HWND, LPCSTR))(uintptr_t)iat->u1.Function;
            iat->u1.Function = (DWORD)(uintptr_t)h_settext;
            VirtualProtect(&iat->u1.Function, 4, old, &old);
            return 1;
        }
    }
    return 0;
}

static GpPath *rr_path(float x, float y, float w, float h, float r)
{
    GpPath *p;
    float d = r * 2;
    GdipCreatePath(0, &p);
    GdipAddPathArc(p, x, y, d, d, 180, 90);
    GdipAddPathArc(p, x + w - d, y, d, d, 270, 90);
    GdipAddPathArc(p, x + w - d, y + h - d, d, d, 0, 90);
    GdipAddPathArc(p, x, y + h - d, d, d, 90, 90);
    GdipClosePathFigure(p);
    return p;
}

static void fill(GpGraphics *g, GpPath *p, DWORD argb)
{
    GpBrush *b;
    GdipCreateSolidFill(argb, &b);
    GdipFillPath(g, b, p);
    GdipDeleteBrush(b);
}

static void text(GpGraphics *g, const WCHAR *s, float size, int style, DWORD argb, RectF r)
{
    GpFontFamily *ff = NULL;
    GpFont *f = NULL;
    GpStringFormat *sf;
    GpBrush *b;
    if (GdipCreateFontFamilyFromName(L"Segoe UI", NULL, &ff))
        return;
    GdipCreateFont(ff, size, style, 2, &f);
    GdipCreateStringFormat(0x1000, 0, &sf);
    GdipSetStringFormatLineAlign(sf, 1);
    GdipSetStringFormatTrimming(sf, 3);
    GdipCreateSolidFill(argb, &b);
    GdipDrawString(g, s, -1, f, &r, sf, b);
    GdipDeleteBrush(b);
    GdipDeleteStringFormat(sf);
    GdipDeleteFont(f);
    GdipDeleteFontFamily(ff);
}

static void render(const WCHAR *title, const WCHAR *value, DWORD accent)
{
    GpImage *img;
    GpGraphics *g;
    memset(g_bits, 0, (size_t)g_w * g_h * 4);
    if (GdipCreateBitmapFromScan0(g_w, g_h, g_w * 4, PF_PARGB, g_bits, &img))
        return;
    GdipGetImageGraphicsContext(img, &g);
    GdipSetSmoothingMode(g, 4);
    GdipSetPixelOffsetMode(g, 4);
    GdipSetTextRenderingHint(g, 4);
    float x = px(SHADOW), y = px(SHADOW), w = px(TOAST_W), h = px(TOAST_H), r = px(12);
    for (int i = SHADOW; i >= 1; i--) {
        float k = 1 - (float)i / SHADOW;
        GpPath *s = rr_path(x - px(i), y - px(i) + px(3), w + px(i * 2), h + px(i * 2), r + px(i));
        fill(g, s, (DWORD)(14 * k * k + 0.5f) << 24);
        GdipDeletePath(s);
    }
    GpPath *card = rr_path(x, y, w, h, r);
    fill(g, card, 0xF722232B);
    GdipSetClipPath(g, card, 0);
    GpBrush *ab;
    GdipCreateSolidFill(accent, &ab);
    GdipFillRectangle(g, ab, x, y, px(4), h);
    GdipDeleteBrush(ab);
    GdipResetClip(g);
    GpPen *pen;
    GdipCreatePen1(0x22FFFFFF, px(1), 2, &pen);
    GdipDrawPath(g, pen, card);
    GdipDeletePen(pen);
    GdipDeletePath(card);
    text(g, title, px(12), 0, 0xFF9CA1AF, (RectF){ x + px(18), y + px(9), w - px(30), px(18) });
    text(g, value, px(16), 1, 0xFFFFFFFF, (RectF){ x + px(18), y + px(27), w - px(30), px(24) });
    GdipDeleteGraphics(g);
    GdipDisposeImage(img);
}

static void present(void)
{
    POINT src = { 0, 0 };
    SIZE sz = { g_w, g_h };
    BLENDFUNCTION bf = { AC_SRC_OVER, 0, (BYTE)(g_alpha * 255 + 0.5f), AC_SRC_ALPHA };
    GdiFlush();
    UpdateLayeredWindow(g_toast, NULL, NULL, &sz, g_mdc, &src, 0, &bf, ULW_ALPHA);
}

static LRESULT CALLBACK toast_proc(HWND h, UINT m, WPARAM w, LPARAM l)
{
    if (m == WM_TIMER && w == TIMER_FADE) {
        DWORD t = GetTickCount() - g_shown_at;
        g_alpha = t < 120 ? t / 120.0f : t < 1400 ? 1 : 1 - (t - 1400) / 300.0f;
        if (g_alpha <= 0) {
            g_alpha = 0;
            KillTimer(h, TIMER_FADE);
            ShowWindow(h, SW_HIDE);
            return 0;
        }
        present();
        return 0;
    }
    if (m == WM_MOUSEACTIVATE)
        return MA_NOACTIVATE;
    return DefWindowProcW(h, m, w, l);
}

static void toast_show(const WCHAR *title, const WCHAR *value, DWORD accent)
{
    HMODULE u32 = GetModuleHandleW(L"user32.dll");
    void *(WINAPI *set_ctx)(void *) = (void *(WINAPI *)(void *))GetProcAddress(u32, "SetThreadDpiAwarenessContext");
    void *old = set_ctx ? set_ctx((void *)-4) : NULL;
    POINT cur;
    GetCursorPos(&cur);
    HMONITOR mon = MonitorFromPoint(cur, MONITOR_DEFAULTTONEAREST);
    MONITORINFO mi = { sizeof mi };
    GetMonitorInfoW(mon, &mi);
    if (!g_toast) {
        HMODULE shc = LoadLibraryW(L"shcore.dll");
        HRESULT (WINAPI *dpi_for)(HMONITOR, int, UINT *, UINT *) = shc ? (HRESULT (WINAPI *)(HMONITOR, int, UINT *, UINT *))GetProcAddress(shc, "GetDpiForMonitor") : NULL;
        UINT dx = 96, dy = 96;
        if (set_ctx && dpi_for && dpi_for(mon, 0, &dx, &dy) == S_OK)
            g_s = dx / 96.0f;
        g_w = (int)px(TOAST_W + SHADOW * 2);
        g_h = (int)px(TOAST_H + SHADOW * 2);
        GdipInput gi = { 1, NULL, FALSE, FALSE };
        HDC sdc = GetDC(NULL);
        BITMAPINFO bi = { { sizeof(BITMAPINFOHEADER), g_w, -g_h, 1, 32, BI_RGB } };
        g_mdc = CreateCompatibleDC(sdc);
        g_dib = CreateDIBSection(sdc, &bi, DIB_RGB_COLORS, (void **)&g_bits, NULL, 0);
        ReleaseDC(NULL, sdc);
        WNDCLASSW wc = { 0, toast_proc, 0, 0, g_inst, NULL, NULL, NULL, NULL, L"KFToolsToast" };
        RegisterClassW(&wc);
        if (g_dib && !GdiplusStartup(&g_gdip, &gi, NULL)) {
            SelectObject(g_mdc, g_dib);
            g_toast = CreateWindowExW(WS_EX_LAYERED | WS_EX_TOOLWINDOW | WS_EX_NOACTIVATE | WS_EX_TOPMOST | WS_EX_TRANSPARENT,
                                      L"KFToolsToast", L"", WS_POPUP, 0, 0, g_w, g_h, NULL, NULL, g_inst, NULL);
        }
    }
    if (g_toast) {
        int x = cur.x + (int)px(14), y = cur.y + (int)px(18);
        if (x + g_w > mi.rcWork.right) x = cur.x - g_w - (int)px(4);
        if (y + g_h > mi.rcWork.bottom) y = cur.y - g_h - (int)px(4);
        render(title, value, accent);
        g_alpha = g_alpha > 0 ? 1 : 0;
        g_shown_at = GetTickCount() - (g_alpha > 0 ? 120 : 0);
        SetWindowPos(g_toast, HWND_TOPMOST, x, y, g_w, g_h, SWP_NOACTIVATE | SWP_SHOWWINDOW);
        present();
        SetTimer(g_toast, TIMER_FADE, 16, NULL);
    }
    if (set_ctx)
        set_ctx(old);
}

typedef struct { WCHAR title[64], value[128]; DWORD accent; } ToastMsg;
static DWORD g_ttid;
static HANDLE g_tready;

static DWORD WINAPI toast_thread(void *arg)
{
    MSG m;
    (void)arg;
    PeekMessageW(&m, NULL, WM_USER, WM_USER, PM_NOREMOVE);
    SetEvent(g_tready);
    while (GetMessageW(&m, NULL, 0, 0) > 0) {
        if (!m.hwnd && m.message == WM_APP) {
            ToastMsg *t = (ToastMsg *)m.lParam;
            toast_show(t->title, t->value, t->accent);
            free(t);
            continue;
        }
        DispatchMessageW(&m);
    }
    return 0;
}

static void toast(const WCHAR *title, const WCHAR *value, DWORD accent)
{
    if (!g_ttid) {
        g_tready = CreateEventW(NULL, TRUE, FALSE, NULL);
        HANDLE th = CreateThread(NULL, 0, toast_thread, NULL, 0, &g_ttid);
        if (!th) {
            g_ttid = 0;
            return;
        }
        CloseHandle(th);
        WaitForSingleObject(g_tready, 2000);
    }
    ToastMsg *t = calloc(1, sizeof *t);
    if (!t)
        return;
    lstrcpynW(t->title, title, 64);
    lstrcpynW(t->value, value, 128);
    t->accent = accent;
    if (!PostThreadMessageW(g_ttid, WM_APP, 0, (LPARAM)t))
        free(t);
}

static int set_clipboard(const WCHAR *s, HWND owner)
{
    size_t n = (wcslen(s) + 1) * sizeof(WCHAR);
    HGLOBAL mem = GlobalAlloc(GMEM_MOVEABLE, n);
    if (!mem)
        return 0;
    memcpy(GlobalLock(mem), s, n);
    GlobalUnlock(mem);
    for (int i = 0; i < 5; i++) {
        if (OpenClipboard(owner)) {
            EmptyClipboard();
            int ok = SetClipboardData(CF_UNICODETEXT, mem) != NULL;
            CloseClipboard();
            if (ok)
                return 1;
            break;
        }
        Sleep(10);
    }
    GlobalFree(mem);
    return 0;
}

__declspec(dllexport) int KFToolsCopy(int with_z)
{
    WCHAR value[128];
    if (!g_have) {
        WCHAR raw[128];
        if (g_raw[0] && MultiByteToWideChar(CP_ACP, 0, g_raw, -1, raw, 128))
            toast(L"Не разобрал строку координат", raw, 0xFFFF7A7A);
        else
            toast(L"Координаты не скопированы", L"Наведите курсор на карту", 0xFFFF7A7A);
        return 0;
    }
    if (with_z)
        swprintf(value, 128, L"%hs, %hs, %hs", g_pt[0], g_pt[1], g_pt[2]);
    else
        swprintf(value, 128, L"%hs, %hs", g_pt[0], g_pt[1]);
    if (!set_clipboard(value, main_window())) {
        toast(L"Не удалось открыть буфер обмена", value, 0xFFFF7A7A);
        return 0;
    }
    toast(L"Координаты скопированы", value, 0xFF7C5CFF);
    return 1;
}

#define RELEASE_HOST L"api.github.com"
#define RELEASE_PATH L"/repos/NaxefirYT/KFWE/releases/latest"
#define BODY_MAX (1 << 20)
#define SETUP_MIN (1 << 20)

static void kfwe_dir(WCHAR *out)
{
    GetModuleFileNameW(g_inst, out, MAX_PATH);
    for (int up = 0; up < 3; up++) {
        WCHAR *s = wcsrchr(out, L'\\');
        if (s)
            *s = 0;
    }
}

static int read_version(const WCHAR *dir, char *out, int cap)
{
    static const WCHAR *langs[] = { L"en-US", L"zh-CN" };
    for (int i = 0; i < 2; i++) {
        WCHAR f[MAX_PATH];
        char buf[8192];
        swprintf(f, MAX_PATH, L"%ls\\share\\locale\\%ls\\script.lng", dir, langs[i]);
        FILE *fp = _wfopen(f, L"rb");
        if (!fp)
            continue;
        size_t n = fread(buf, 1, sizeof buf - 1, fp);
        fclose(fp);
        buf[n] = 0;
        char *v = strstr(buf, "KFWE v");
        if (!v)
            continue;
        v += 6;
        int k = 0;
        while (k < cap - 1 && (v[k] == '.' || (v[k] >= '0' && v[k] <= '9'))) {
            out[k] = v[k];
            k++;
        }
        out[k] = 0;
        if (k)
            return 1;
    }
    return 0;
}

static int ver_cmp(const char *a, const char *b)
{
    while (*a || *b) {
        long x = strtol(a, (char **)&a, 10), y = strtol(b, (char **)&b, 10);
        if (x != y)
            return x < y ? -1 : 1;
        if (*a == '.')
            a++;
        if (*b == '.')
            b++;
        if ((*a && (*a < '0' || *a > '9')) || (*b && (*b < '0' || *b > '9')))
            break;
    }
    return 0;
}

static int http_get(const WCHAR *url, char **buf, DWORD *len, const WCHAR *to)
{
    URL_COMPONENTS u = { sizeof u };
    WCHAR host[256], path[2048];
    u.lpszHostName = host;
    u.dwHostNameLength = 256;
    u.lpszUrlPath = path;
    u.dwUrlPathLength = 2048;
    if (!WinHttpCrackUrl(url, 0, 0, &u))
        return 0;
    int ok = 0;
    HANDLE f = INVALID_HANDLE_VALUE;
    HINTERNET s = WinHttpOpen(L"KFWE", WINHTTP_ACCESS_TYPE_AUTOMATIC_PROXY, WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, 0);
    if (!s)
        s = WinHttpOpen(L"KFWE", WINHTTP_ACCESS_TYPE_DEFAULT_PROXY, WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, 0);
    HINTERNET c = s ? WinHttpConnect(s, host, u.nPort, 0) : NULL;
    HINTERNET r = c ? WinHttpOpenRequest(c, L"GET", path, NULL, WINHTTP_NO_REFERER, WINHTTP_DEFAULT_ACCEPT_TYPES, u.nScheme == INTERNET_SCHEME_HTTPS ? WINHTTP_FLAG_SECURE : 0) : NULL;
    DWORD code = 0, sz = sizeof code;
    if (r)
        WinHttpSetTimeouts(r, 10000, 10000, 15000, 30000);
    if (!r || !WinHttpSendRequest(r, L"Accept: application/vnd.github+json\r\n", (DWORD)-1, NULL, 0, 0, 0) || !WinHttpReceiveResponse(r, NULL) ||
        !WinHttpQueryHeaders(r, WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER, NULL, &code, &sz, NULL) || code != 200)
        goto done;
    if (to && (f = CreateFileW(to, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, 0, NULL)) == INVALID_HANDLE_VALUE)
        goto done;
    char chunk[65536];
    DWORD got, total = 0;
    if (buf && !(*buf = malloc(BODY_MAX + 1)))
        goto done;
    while (WinHttpReadData(r, chunk, sizeof chunk, &got) && got) {
        DWORD w;
        if (to && (!WriteFile(f, chunk, got, &w, NULL) || w != got))
            goto done;
        if (buf) {
            if (total + got > BODY_MAX)
                goto done;
            memcpy(*buf + total, chunk, got);
        }
        total += got;
    }
    if (buf)
        (*buf)[total] = 0;
    if (len)
        *len = total;
    ok = 1;
done:
    if (!ok && buf && *buf) {
        free(*buf);
        *buf = NULL;
    }
    if (f != INVALID_HANDLE_VALUE)
        CloseHandle(f);
    if (r)
        WinHttpCloseHandle(r);
    if (c)
        WinHttpCloseHandle(c);
    if (s)
        WinHttpCloseHandle(s);
    return ok;
}

static int json_str(const char *from, const char *key, char *out, int cap)
{
    char k[64];
    snprintf(k, sizeof k, "\"%s\"", key);
    const char *p = strstr(from, k);
    if (!p)
        return 0;
    p += strlen(k);
    while (*p == ' ' || *p == ':')
        p++;
    if (*p++ != '"')
        return 0;
    int n = 0;
    while (*p && *p != '"' && n < cap - 1)
        out[n++] = *p++;
    out[n] = 0;
    return *p == '"';
}

static char *json_text(const char *from, const char *key)
{
    char k[64];
    snprintf(k, sizeof k, "\"%s\"", key);
    const char *p = strstr(from, k);
    if (!p)
        return NULL;
    p += strlen(k);
    while (*p == ' ' || *p == ':')
        p++;
    if (*p++ != '"')
        return NULL;
    char *out = malloc(strlen(p) + 1), *o = out;
    if (!out)
        return NULL;
    while (*p && *p != '"') {
        if (*p != '\\') {
            *o++ = *p++;
            continue;
        }
        p++;
        char e = *p ? *p++ : 0;
        if (e == 'n')
            *o++ = '\n';
        else if (e == 't')
            *o++ = ' ';
        else if (e == 'u' && strlen(p) >= 4) {
            char hex[5] = { p[0], p[1], p[2], p[3], 0 };
            unsigned c = (unsigned)strtoul(hex, NULL, 16);
            p += 4;
            if (c >= 0xD800 && c < 0xDC00 && p[0] == '\\' && p[1] == 'u') {
                char lo[5] = { p[2], p[3], p[4], p[5], 0 };
                c = 0x10000 + ((c - 0xD800) << 10) + ((unsigned)strtoul(lo, NULL, 16) - 0xDC00);
                p += 6;
            }
            if (c < 0x80)
                *o++ = (char)c;
            else if (c < 0x800) {
                *o++ = (char)(0xC0 | c >> 6);
                *o++ = (char)(0x80 | (c & 63));
            } else if (c < 0x10000) {
                *o++ = (char)(0xE0 | c >> 12);
                *o++ = (char)(0x80 | ((c >> 6) & 63));
                *o++ = (char)(0x80 | (c & 63));
            } else {
                *o++ = (char)(0xF0 | c >> 18);
                *o++ = (char)(0x80 | ((c >> 12) & 63));
                *o++ = (char)(0x80 | ((c >> 6) & 63));
                *o++ = (char)(0x80 | (c & 63));
            }
        } else if (e && e != 'r')
            *o++ = e;
    }
    *o = 0;
    return out;
}

#define NOTES_W 600
#define NOTES_H 560
#define NOTES_CLASS L"KFToolsNotes"
#define C_NBG RGB(0x1E, 0x1F, 0x24)
#define C_NTEXT RGB(0xE4, 0xE6, 0xEB)
#define C_NDIM RGB(0x8A, 0x8F, 0x9E)
#define C_NACCENT RGB(0x9D, 0x86, 0xFF)
#define C_NBTN RGB(0x36, 0x39, 0x44)
#define C_NBTN_HOT RGB(0x45, 0x49, 0x59)

typedef struct {
    const WCHAR *title, *sub, *labels[2];
    int nbtn, result, hot;
    RECT btn[2];
    HWND re;
    HFONT f_title, f_sub, f_btn;
} Notes;

static WCHAR *md_render(const char *md, int **heads, int *nheads)
{
    int wn = MultiByteToWideChar(CP_UTF8, 0, md, -1, NULL, 0);
    WCHAR *src = malloc((wn + 1) * sizeof(WCHAR)), *out = malloc((wn * 2 + 16) * sizeof(WCHAR));
    *heads = malloc((wn / 2 + 2) * 3 * sizeof(int));
    *nheads = 0;
    MultiByteToWideChar(CP_UTF8, 0, md, -1, src, wn);
    int o = 0, blank = 1;
    for (WCHAR *line = src, *next; line; line = next) {
        next = wcschr(line, L'\n');
        if (next)
            *next++ = 0;
        WCHAR *s = line;
        int indent = 0;
        while (*s == L' ' || *s == L'\t')
            s++, indent++;
        size_t len = wcslen(s);
        while (len && (s[len - 1] == L'\r' || s[len - 1] == L' '))
            s[--len] = 0;
        if (!len) {
            if (!blank)
                out[o++] = L'\r';
            blank = 1;
            continue;
        }
        if (wcsstr(s, L"KFWE-Setup.exe") || wcsstr(s, L".zip"))
            continue;
        int head = 0;
        if (*s == L'#') {
            while (*s == L'#')
                s++;
            while (*s == L' ')
                s++;
            head = 1;
        } else if (len > 4 && !wcsncmp(s, L"**", 2) && !wcscmp(s + len - 2, L"**")) {
            head = 2;
        } else if ((*s == L'-' || *s == L'*') && s[1] == L' ') {
            for (int i = 0; i < indent / 2; i++)
                out[o++] = L' ', out[o++] = L' ', out[o++] = L' ';
            out[o++] = L'•';
            out[o++] = L' ';
            out[o++] = L' ';
            s += 2;
        }
        int start = o;
        for (; *s; s++)
            if (!((*s == L'*' || *s == L'_') && s[1] == *s) && *s != L'`')
                out[o++] = *s;
            else if (*s != L'`')
                s++;
        if (head) {
            (*heads)[*nheads * 3] = start;
            (*heads)[*nheads * 3 + 1] = o;
            (*heads)[*nheads * 3 + 2] = head;
            (*nheads)++;
        }
        out[o++] = L'\r';
        blank = 0;
    }
    while (o && out[o - 1] == L'\r')
        o--;
    out[o] = 0;
    free(src);
    return out;
}

static void re_format(HWND re, int a, int b, int size, int bold, COLORREF color)
{
    CHARFORMAT2W cf = { 0 };
    cf.cbSize = sizeof cf;
    cf.dwMask = CFM_SIZE | CFM_BOLD | CFM_COLOR | CFM_FACE;
    cf.yHeight = size * 20;
    cf.dwEffects = bold ? CFE_BOLD : 0;
    cf.crTextColor = color;
    wcscpy(cf.szFaceName, L"Segoe UI");
    SendMessageW(re, EM_SETSEL, a, b);
    SendMessageW(re, EM_SETCHARFORMAT, a < 0 ? SCF_ALL : SCF_SELECTION, (LPARAM)&cf);
}

static void notes_btn_layout(Notes *n, RECT c)
{
    int bw = 150, bh = 36, y = c.bottom - 20 - bh, x = c.right - 24 - bw;
    for (int i = 0; i < n->nbtn; i++, x -= bw + 10)
        SetRect(&n->btn[i], x, y, x + bw, y + bh);
}

static LRESULT CALLBACK notes_proc(HWND h, UINT m, WPARAM w, LPARAM l)
{
    Notes *n = (Notes *)GetWindowLongPtrW(h, GWLP_USERDATA);
    switch (m) {
    case WM_NCCREATE:
        SetWindowLongPtrW(h, GWLP_USERDATA, (LONG_PTR)((CREATESTRUCTW *)l)->lpCreateParams);
        break;
    case WM_ERASEBKGND:
        return 1;
    case WM_PAINT: {
        PAINTSTRUCT ps;
        HDC dc = BeginPaint(h, &ps);
        RECT c;
        GetClientRect(h, &c);
        HBRUSH bg = CreateSolidBrush(C_NBG);
        FillRect(dc, &c, bg);
        DeleteObject(bg);
        SetBkMode(dc, TRANSPARENT);
        RECT t = { 24, 18, c.right - 24, 52 };
        SelectObject(dc, n->f_title);
        SetTextColor(dc, C_NTEXT);
        DrawTextW(dc, n->title, -1, &t, DT_SINGLELINE | DT_NOPREFIX | DT_END_ELLIPSIS);
        RECT s = { 24, 54, c.right - 24, 96 };
        SelectObject(dc, n->f_sub);
        SetTextColor(dc, C_NDIM);
        DrawTextW(dc, n->sub, -1, &s, DT_WORDBREAK | DT_NOPREFIX);
        SelectObject(dc, n->f_btn);
        for (int i = 0; i < n->nbtn; i++) {
            HBRUSH b = CreateSolidBrush(i == 0 ? (n->hot == i ? RGB(0xB3, 0xA1, 0xFF) : C_NACCENT) : (n->hot == i ? C_NBTN_HOT : C_NBTN));
            HPEN pen = CreatePen(PS_SOLID, 1, i == 0 ? C_NACCENT : C_NBTN);
            HGDIOBJ ob = SelectObject(dc, b), op = SelectObject(dc, pen);
            RoundRect(dc, n->btn[i].left, n->btn[i].top, n->btn[i].right, n->btn[i].bottom, 10, 10);
            SelectObject(dc, ob);
            SelectObject(dc, op);
            DeleteObject(b);
            DeleteObject(pen);
            SetTextColor(dc, i == 0 ? RGB(0x16, 0x16, 0x1A) : C_NTEXT);
            DrawTextW(dc, n->labels[i], -1, &n->btn[i], DT_CENTER | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);
        }
        EndPaint(h, &ps);
        return 0;
    }
    case WM_MOUSEMOVE: {
        POINT p = { (short)LOWORD(l), (short)HIWORD(l) };
        int hot = -1;
        for (int i = 0; i < n->nbtn; i++)
            if (PtInRect(&n->btn[i], p))
                hot = i;
        if (hot != n->hot) {
            n->hot = hot;
            InvalidateRect(h, NULL, FALSE);
            TRACKMOUSEEVENT tme = { sizeof tme, TME_LEAVE, h, 0 };
            TrackMouseEvent(&tme);
        }
        SetCursor(LoadCursor(NULL, hot >= 0 ? IDC_HAND : IDC_ARROW));
        return 0;
    }
    case WM_MOUSELEAVE:
        n->hot = -1;
        InvalidateRect(h, NULL, FALSE);
        return 0;
    case WM_LBUTTONUP: {
        POINT p = { (short)LOWORD(l), (short)HIWORD(l) };
        for (int i = 0; i < n->nbtn; i++)
            if (PtInRect(&n->btn[i], p)) {
                n->result = i == 0;
                DestroyWindow(h);
                break;
            }
        return 0;
    }
    case WM_MOUSEWHEEL:
        SendMessageW(n->re, m, w, l);
        return 0;
    case WM_CLOSE:
        DestroyWindow(h);
        return 0;
    }
    return DefWindowProcW(h, m, w, l);
}

static int show_notes(HWND owner, const WCHAR *title, const WCHAR *sub, const char *md, int ask)
{
    static int registered;
    if (!LoadLibraryW(L"Msftedit.dll"))
        return -1;
    if (!registered) {
        WNDCLASSW wc = { 0, notes_proc, 0, 0, g_inst, NULL, LoadCursor(NULL, IDC_ARROW), NULL, NULL, NOTES_CLASS };
        registered = RegisterClassW(&wc) != 0;
    }
    Notes n = { title, sub, { ask ? L"Обновить" : L"Понятно", L"Позже" }, ask ? 2 : 1, 0, -1 };
    n.f_title = CreateFontW(-24, 0, 0, 0, FW_SEMIBOLD, 0, 0, 0, DEFAULT_CHARSET, 0, 0, CLEARTYPE_QUALITY, 0, L"Segoe UI");
    n.f_sub = CreateFontW(-14, 0, 0, 0, FW_NORMAL, 0, 0, 0, DEFAULT_CHARSET, 0, 0, CLEARTYPE_QUALITY, 0, L"Segoe UI");
    n.f_btn = CreateFontW(-15, 0, 0, 0, FW_SEMIBOLD, 0, 0, 0, DEFAULT_CHARSET, 0, 0, CLEARTYPE_QUALITY, 0, L"Segoe UI");
    RECT wr = { 0, 0, NOTES_W, NOTES_H }, or;
    DWORD style = WS_POPUP | WS_CAPTION | WS_SYSMENU;
    AdjustWindowRect(&wr, style, FALSE);
    int ww = wr.right - wr.left, wh = wr.bottom - wr.top;
    if (!owner || !GetWindowRect(owner, &or))
        SystemParametersInfoW(SPI_GETWORKAREA, 0, &or, 0);
    HWND h = CreateWindowExW(0, NOTES_CLASS, title, style, (or.left + or.right - ww) / 2, (or.top + or.bottom - wh) / 2, ww, wh, owner, NULL, g_inst, &n);
    if (!h)
        return -1;
    BOOL dark = TRUE;
    DwmSetWindowAttribute(h, 20, &dark, sizeof dark);
    RECT c;
    GetClientRect(h, &c);
    notes_btn_layout(&n, c);
    n.re = CreateWindowExW(0, MSFTEDIT_CLASS, L"", WS_CHILD | WS_VISIBLE | WS_VSCROLL | ES_MULTILINE | ES_READONLY | ES_AUTOVSCROLL, 24, 108,
                           c.right - 40, n.btn[0].top - 20 - 108, h, NULL, g_inst, NULL);
    int *heads, nheads;
    WCHAR *text = md_render(md && *md ? md : "Список изменений не указан.", &heads, &nheads);
    SetWindowTheme(n.re, L"DarkMode_Explorer", NULL);
    SendMessageW(n.re, EM_SETBKGNDCOLOR, 0, C_NBG);
    SendMessageW(n.re, EM_SETTARGETDEVICE, 0, 0);
    SetWindowTextW(n.re, text);
    re_format(n.re, -1, -1, 11, 0, C_NTEXT);
    PARAFORMAT2 pf = { 0 };
    pf.cbSize = sizeof pf;
    pf.dwMask = PFM_SPACEAFTER;
    pf.dySpaceAfter = 80;
    SendMessageW(n.re, EM_SETSEL, 0, -1);
    SendMessageW(n.re, EM_SETPARAFORMAT, 0, (LPARAM)&pf);
    for (int i = 0; i < nheads; i++)
        re_format(n.re, heads[i * 3], heads[i * 3 + 1], heads[i * 3 + 2] == 1 ? 14 : 11, 1, heads[i * 3 + 2] == 1 ? C_NACCENT : C_NTEXT);
    SendMessageW(n.re, EM_SETSEL, 0, 0);
    SendMessageW(n.re, EM_HIDESELECTION, TRUE, 0);
    free(text);
    free(heads);
    ShowWindow(h, SW_SHOW);
    SetForegroundWindow(h);
    SetFocus(h);
    MSG msg;
    while (IsWindow(h) && GetMessageW(&msg, NULL, 0, 0) > 0) {
        if (msg.message == WM_KEYDOWN && (msg.wParam == VK_ESCAPE || msg.wParam == VK_RETURN)) {
            n.result = msg.wParam == VK_RETURN;
            DestroyWindow(h);
            continue;
        }
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }
    DeleteObject(n.f_title);
    DeleteObject(n.f_sub);
    DeleteObject(n.f_btn);
    return n.result;
}

static char *read_text(const WCHAR *path)
{
    FILE *fp = _wfopen(path, L"rb");
    if (!fp)
        return NULL;
    fseek(fp, 0, SEEK_END);
    long n = ftell(fp);
    fseek(fp, 0, SEEK_SET);
    char *b = n >= 0 && n < BODY_MAX ? malloc(n + 1) : NULL;
    if (b) {
        b[fread(b, 1, n, fp)] = 0;
        if (!strncmp(b, "\xEF\xBB\xBF", 3))
            memmove(b, b + 3, strlen(b + 3) + 1);
    }
    fclose(fp);
    return b;
}

static void whats_new(HWND owner, const WCHAR *dir, const char *cur, int force)
{
    WCHAR seen_path[MAX_PATH], log_path[MAX_PATH], title[64];
    char seen[32] = "";
    swprintf(seen_path, MAX_PATH, L"%ls\\bin\\changelog.seen", dir);
    FILE *fp = _wfopen(seen_path, L"rb");
    if (fp) {
        seen[fread(seen, 1, sizeof seen - 1, fp)] = 0;
        fclose(fp);
    }
    if (!force && !strcmp(seen, cur))
        return;
    swprintf(log_path, MAX_PATH, L"%ls\\share\\changelog.md", dir);
    char *md = read_text(log_path);
    if (md) {
        swprintf(title, 64, L"Что нового в KFWE %hs", cur);
        show_notes(owner, title, L"Открыть снова: меню KFWE → «Что нового»", md, 0);
        free(md);
    }
    if ((fp = _wfopen(seen_path, L"wb"))) {
        fputs(cur, fp);
        fclose(fp);
    }
}

static void mark_menu(HWND m, const char *latest)
{
    HMENU bar = GetMenu(m);
    WCHAR buf[64], label[64];
    for (int i = 0, n = GetMenuItemCount(bar); i < n; i++) {
        if (GetMenuStringW(bar, i, buf, 64, MF_BYPOSITION) && wcsstr(buf, L"KFWE v")) {
            MENUITEMINFOW mi = { sizeof mi, MIIM_STRING };
            swprintf(label, 64, L"Доступно обновление %hs", latest);
            mi.dwTypeData = label;
            SetMenuItemInfoW(bar, i, TRUE, &mi);
            DrawMenuBar(m);
            return;
        }
    }
}

static void check_update(int manual)
{
    WCHAR dir[MAX_PATH], cfg[MAX_PATH], url[1024], setup[MAX_PATH], cmd[MAX_PATH * 2 + 64], text[512];
    char cur[32], tag[64] = "", asset[1024], *body = NULL, *notes = NULL;
    HWND m = NULL;
    kfwe_dir(dir);
    if (!read_version(dir, cur, sizeof cur))
        return;
    for (int i = 0; i < 240 && !(m = main_window()); i++)
        Sleep(500);
    if (!m)
        return;
    if (!manual) {
        Sleep(2000);
        whats_new(m, dir, cur, 0);
    }
    swprintf(cfg, MAX_PATH, L"%ls\\bin\\EverConfig.cfg", dir);
    if (!manual && GetPrivateProfileIntW(L"FeatureToggle", L"DisableAutoUpdate", 0, cfg))
        return;
    if (!http_get(L"https://" RELEASE_HOST RELEASE_PATH, &body, NULL, NULL)) {
        if (manual)
            MessageBoxW(m, L"Не удалось связаться с GitHub. Проверьте интернет и попробуйте ещё раз.", L"Обновление KFWE", MB_OK | MB_ICONWARNING);
        return;
    }
    asset[0] = 0;
    if (json_str(body, "tag_name", tag, sizeof tag)) {
        for (const char *p = body; (p = strstr(p, "\"browser_download_url\"")); p++) {
            char a[1024];
            size_t n;
            if (json_str(p, "browser_download_url", a, sizeof a) && (n = strlen(a)) > 15 && !strcmp(a + n - 15, "/KFWE-Setup.exe")) {
                strcpy(asset, a);
                break;
            }
        }
    }
    notes = json_text(body, "body");
    free(body);
    const char *latest = tag[0] == 'v' ? tag + 1 : tag;
    if (!asset[0] || ver_cmp(latest, cur) <= 0 || !(m = main_window())) {
        if (manual && (m = main_window())) {
            swprintf(text, 512, L"У вас последняя версия KFWE %hs.", cur);
            MessageBoxW(m, text, L"Обновление KFWE", MB_OK | MB_ICONINFORMATION);
        }
        free(notes);
        return;
    }
    mark_menu(m, latest);
    WCHAR title[64];
    swprintf(title, 64, L"Вышла KFWE %hs", latest);
    swprintf(text, 512, L"У вас %hs. Редактор скачает обновление и закроется (несохранённую карту предложит сохранить), "
                        L"после установки запустится снова. Настройки сохранятся.", cur);
    int yes = show_notes(m, title, text, notes, 1);
    free(notes);
    if (yes != 1)
        return;
    toast(L"Скачиваю обновление", L"Редактор закроется сам", 0xFF7C5CFF);
    MultiByteToWideChar(CP_UTF8, 0, asset, -1, url, 1024);
    GetTempPathW(MAX_PATH, setup);
    wcscat(setup, L"KFWE-Setup-update.exe");
    DWORD got = 0;
    char mz[2] = { 0 };
    FILE *fp;
    if (http_get(url, NULL, &got, setup) && got >= SETUP_MIN && (fp = _wfopen(setup, L"rb"))) {
        fread(mz, 1, 2, fp);
        fclose(fp);
    }
    if (mz[0] != 'M' || mz[1] != 'Z') {
        DeleteFileW(setup);
        MessageBoxW(main_window(), L"Не удалось скачать обновление. Его можно поставить вручную со страницы релизов на GitHub.", L"Обновление KFWE", MB_OK | MB_ICONWARNING);
        return;
    }
    STARTUPINFOW si = { sizeof si };
    PROCESS_INFORMATION pi;
    swprintf(cmd, MAX_PATH * 2 + 64, L"\"%ls\" /update \"%ls\" %lu", setup, dir, GetCurrentProcessId());
    if (!CreateProcessW(setup, cmd, NULL, NULL, FALSE, 0, NULL, NULL, &si, &pi)) {
        MessageBoxW(main_window(), L"Не удалось запустить установщик обновления.", L"Обновление KFWE", MB_OK | MB_ICONWARNING);
        return;
    }
    CloseHandle(pi.hThread);
    CloseHandle(pi.hProcess);
    if ((m = main_window()))
        PostMessageW(m, WM_CLOSE, 0, 0);
}

static volatile LONG g_checking;

static DWORD WINAPI update_thread(void *arg)
{
    if (!InterlockedExchange(&g_checking, 1)) {
        check_update(arg != NULL);
        InterlockedExchange(&g_checking, 0);
    }
    return 0;
}

__declspec(dllexport) int KFToolsCheckUpdate(void)
{
    HANDLE th = CreateThread(NULL, 0, update_thread, (void *)1, 0, NULL);
    if (!th)
        return 0;
    CloseHandle(th);
    return 1;
}

static DWORD WINAPI changelog_thread(void *arg)
{
    (void)arg;
    WCHAR dir[MAX_PATH];
    char cur[32];
    kfwe_dir(dir);
    if (read_version(dir, cur, sizeof cur))
        whats_new(main_window(), dir, cur, 1);
    return 0;
}

__declspec(dllexport) int KFToolsShowChangelog(void)
{
    HANDLE th = CreateThread(NULL, 0, changelog_thread, NULL, 0, NULL);
    if (!th)
        return 0;
    CloseHandle(th);
    return 1;
}

static int main_active(void)
{
    HWND fg = GetForegroundWindow(), m = main_window();
    return m && fg == m;
}

static LRESULT CALLBACK kb_hook(int code, WPARAM vk, LPARAM lp)
{
    if (code == HC_ACTION && vk == 'X' && !(lp & 0xC0000000) && (GetKeyState(VK_CONTROL) & 0x8000) &&
        (GetKeyState(VK_SHIFT) & 0x8000) && main_active()) {
        KFToolsCopy((GetKeyState(VK_MENU) & 0x8000) != 0);
        return 1;
    }
    return CallNextHookEx(g_kb, code, vk, lp);
}

__declspec(dllexport) int KFToolsInit(void)
{
    if (g_kb)
        return 1;
    g_tid = GetCurrentThreadId();
    if (!hook_settext())
        return -2;
    g_kb = SetWindowsHookExW(WH_KEYBOARD, kb_hook, NULL, g_tid);
    if (!g_kb)
        return -1;
    HMODULE self;
    GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_PIN, (LPCWSTR)&KFToolsInit, &self);
    HANDLE th = CreateThread(NULL, 0, update_thread, NULL, 0, NULL);
    if (th)
        CloseHandle(th);
    return 1;
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
