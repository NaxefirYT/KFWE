#include <windows.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct { UINT32 ver; void *cb; BOOL nothread, nocodecs; } GdipInput;
typedef void GpGraphics, GpBrush, GpPath, GpPen, GpImage, GpFontFamily, GpFont, GpStringFormat;
typedef struct { float X, Y; } PointF;
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
int WINAPI GdipCreateLineBrush(const PointF *, const PointF *, DWORD, DWORD, int, GpBrush **);
int WINAPI GdipCreatePathGradientFromPath(const GpPath *, GpBrush **);
int WINAPI GdipSetPathGradientCenterColor(GpBrush *, DWORD);
int WINAPI GdipSetPathGradientSurroundColorsWithCount(GpBrush *, const DWORD *, int *);
int WINAPI GdipDeleteBrush(GpBrush *);
int WINAPI GdipCreatePen1(DWORD, float, int, GpPen **);
int WINAPI GdipDeletePen(GpPen *);
int WINAPI GdipCreatePath(int, GpPath **);
int WINAPI GdipDeletePath(GpPath *);
int WINAPI GdipAddPathArc(GpPath *, float, float, float, float, float, float);
int WINAPI GdipAddPathEllipse(GpPath *, float, float, float, float);
int WINAPI GdipClosePathFigure(GpPath *);
int WINAPI GdipFillPath(GpGraphics *, GpBrush *, GpPath *);
int WINAPI GdipDrawPath(GpGraphics *, GpPen *, GpPath *);
int WINAPI GdipSetClipPath(GpGraphics *, GpPath *, int);
int WINAPI GdipResetClip(GpGraphics *);
int WINAPI GdipCreateFontFamilyFromName(const WCHAR *, void *, GpFontFamily **);
int WINAPI GdipDeleteFontFamily(GpFontFamily *);
int WINAPI GdipCreateFont(const GpFontFamily *, float, int, int, GpFont **);
int WINAPI GdipDeleteFont(GpFont *);
int WINAPI GdipCreateStringFormat(int, LANGID, GpStringFormat **);
int WINAPI GdipDeleteStringFormat(GpStringFormat *);
int WINAPI GdipSetStringFormatAlign(GpStringFormat *, int);
int WINAPI GdipSetStringFormatLineAlign(GpStringFormat *, int);
int WINAPI GdipDrawString(GpGraphics *, const WCHAR *, int, const GpFont *, const RectF *, const GpStringFormat *, const GpBrush *);
#define PF_PARGB 0xE200B

#define CARD_W 520
#define CARD_H 252
#define MARGIN 30
#define RADIUS 16
#define TIMEOUT_MS 30000

static HINSTANCE g_inst;
static HANDLE g_thread;
static HWND g_wnd;
static float S = 1;
static int W, H;
static BYTE *g_bits, *g_base;
static HDC g_mdc;
static HBITMAP g_dib;
static GpImage *g_img;
static GpGraphics *g_g;
static DWORD g_t0, g_done_at, g_expected = 3000;
static int g_pending, g_good, g_click;
static HWND g_ready_main;
static float g_prog, g_alpha;
static WCHAR g_version[32] = L"";

static float px(float v) { return v * S; }

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

static void fill_rr(float x, float y, float w, float h, float r, DWORD argb)
{
    GpPath *p = rr_path(x, y, w, h, r);
    GpBrush *b;
    GdipCreateSolidFill(argb, &b);
    GdipFillPath(g_g, b, p);
    GdipDeleteBrush(b);
    GdipDeletePath(p);
}

static void glow(float cx, float cy, float rx, float ry, DWORD center)
{
    GpPath *p;
    GpBrush *b;
    DWORD edge = center & 0x00FFFFFF;
    int n = 1;
    GdipCreatePath(0, &p);
    GdipAddPathEllipse(p, cx - rx, cy - ry, rx * 2, ry * 2);
    GdipCreatePathGradientFromPath(p, &b);
    GdipSetPathGradientCenterColor(b, center);
    GdipSetPathGradientSurroundColorsWithCount(b, &edge, &n);
    GdipFillPath(g_g, b, p);
    GdipDeleteBrush(b);
    GdipDeletePath(p);
}

static void text(const WCHAR *s, const WCHAR *family, float size, int style, DWORD argb, RectF r, int align)
{
    GpFontFamily *ff = NULL;
    GpFont *f = NULL;
    GpStringFormat *sf;
    GpBrush *b;
    if (GdipCreateFontFamilyFromName(family, NULL, &ff) && GdipCreateFontFamilyFromName(L"Segoe UI", NULL, &ff)) return;
    GdipCreateFont(ff, size, style, 2, &f);
    GdipCreateStringFormat(0x1000, 0, &sf);
    GdipSetStringFormatAlign(sf, align);
    GdipSetStringFormatLineAlign(sf, 1);
    GdipCreateSolidFill(argb, &b);
    GdipDrawString(g_g, s, -1, f, &r, sf, b);
    GdipDeleteBrush(b);
    GdipDeleteStringFormat(sf);
    GdipDeleteFont(f);
    GdipDeleteFontFamily(ff);
}

static void render_base(void)
{
    float cx = px(MARGIN), cy = px(MARGIN), cw = px(CARD_W), ch = px(CARD_H), r = px(RADIUS);
    GdipGraphicsClear(g_g, 0);
    for (int i = MARGIN; i >= 1; i--) {
        float k = 1 - (float)i / MARGIN;
        DWORD a = (DWORD)(9 * k * k + 0.5f);
        fill_rr(cx - px(i), cy - px(i) + px(8), cw + px(i * 2), ch + px(i * 2), r + px(i), a << 24);
    }
    GpPath *card = rr_path(cx, cy, cw, ch, r);
    PointF p0 = { cx, cy }, p1 = { cx, cy + ch };
    GpBrush *bg;
    GdipCreateLineBrush(&p0, &p1, 0xFF22232B, 0xFF16171C, 0, &bg);
    GdipFillPath(g_g, bg, card);
    GdipDeleteBrush(bg);
    GdipSetClipPath(g_g, card, 0);
    glow(cx + cw - px(70), cy - px(30), px(300), px(230), 0x667C5CFF);
    glow(cx + px(40), cy + ch + px(40), px(260), px(160), 0x2E3A7BFF);
    GdipResetClip(g_g);
    GpPen *pen;
    GdipCreatePen1(0x1FFFFFFF, px(1), 2, &pen);
    GdipDrawPath(g_g, pen, card);
    GdipDeletePen(pen);
    GdipDeletePath(card);

    float mx = cx + px(44), my = cy + px(48), ms = px(68);
    GpPath *mark = rr_path(mx, my, ms, ms, px(18));
    PointF m0 = { mx, my }, m1 = { mx + ms, my + ms };
    GpBrush *mb;
    GdipCreateLineBrush(&m0, &m1, 0xFF9479FF, 0xFF5E3FEA, 0, &mb);
    GdipFillPath(g_g, mb, mark);
    GdipDeleteBrush(mb);
    GdipDeletePath(mark);
    text(L"K", L"Segoe UI", px(40), 1, 0xFFFFFFFF, (RectF){ mx, my - px(2), ms, ms }, 1);

    float tx = mx + ms + px(22);
    text(L"KFWE", L"Segoe UI", px(46), 1, 0xFFFFFFFF, (RectF){ tx, my - px(10), px(330), px(58) }, 0);
    text(L"Редактор Warcraft 3 нового поколения", L"Segoe UI", px(15), 0, 0xFF9CA1AF, (RectF){ tx + px(2), my + px(44), px(360), px(24) }, 0);
    text(L"Загрузка редактора", L"Segoe UI", px(13), 0, 0xFF8B90A0, (RectF){ cx + px(44), cy + ch - px(70), px(260), px(22) }, 0);
    if (g_version[0])
        text(g_version, L"Segoe UI", px(13), 0, 0xFF6B7080, (RectF){ cx + cw - px(304), cy + ch - px(70), px(260), px(22) }, 2);
    memcpy(g_base, g_bits, (size_t)W * H * 4);
}

static void render_frame(void)
{
    float cx = px(MARGIN), cy = px(MARGIN), cw = px(CARD_W), ch = px(CARD_H);
    float bx = cx + px(44), by = cy + ch - px(40), bw = cw - px(88), bh = px(4);
    memcpy(g_bits, g_base, (size_t)W * H * 4);
    fill_rr(bx, by, bw, bh, bh / 2, 0x1FFFFFFF);
    float fw = bw * g_prog;
    if (fw >= bh) {
        GpPath *p = rr_path(bx, by, fw, bh, bh / 2);
        PointF a = { bx, by }, b = { bx + bw, by };
        GpBrush *br;
        GdipCreateLineBrush(&a, &b, 0xFF6A4BF0, 0xFFA48CFF, 0, &br);
        GdipFillPath(g_g, br, p);
        GdipDeleteBrush(br);
        GdipDeletePath(p);
        glow(bx + fw, by + bh / 2, px(18), px(9), 0x88A48CFF);
    }
}

static void present(void)
{
    POINT src = { 0, 0 };
    SIZE sz = { W, H };
    BLENDFUNCTION bf = { AC_SRC_OVER, 0, (BYTE)(g_alpha * 255 + 0.5f), AC_SRC_ALPHA };
    GdiFlush();
    UpdateLayeredWindow(g_wnd, NULL, NULL, &sz, g_mdc, &src, 0, &bf, ULW_ALPHA);
}

typedef struct { HWND main; int dialog; } Found;

static BOOL CALLBACK find_ready(HWND h, LPARAM l)
{
    Found *f = (Found *)l;
    DWORD pid;
    RECT r;
    WCHAR cls[16];
    GetWindowThreadProcessId(h, &pid);
    if (pid != GetCurrentProcessId() || h == g_wnd || !IsWindowVisible(h) || !GetWindowRect(h, &r)) return TRUE;
    GetClassNameW(h, cls, 16);
    if (!lstrcmpW(cls, L"#32770")) { f->dialog = 1; return FALSE; }
    if (r.right - r.left >= 300 && r.bottom - r.top >= 200 && GetMenu(h) && !f->main) f->main = h;
    return TRUE;
}

static VOID CALLBACK ping_done(HWND h, UINT m, ULONG_PTR sent, LRESULT r)
{
    (void)h; (void)m; (void)r;
    g_pending = 0;
    if (GetTickCount() - g_t0 - (DWORD)sent < 150) g_good++;
    else g_good = 0;
}

static void save_expected(DWORD ms)
{
    HKEY k;
    if (RegCreateKeyExW(HKEY_CURRENT_USER, L"Software\\KFWE", 0, NULL, 0, KEY_SET_VALUE, NULL, &k, NULL)) return;
    RegSetValueExW(k, L"StartupMs", 0, REG_DWORD, (const BYTE *)&ms, sizeof ms);
    RegCloseKey(k);
}

static void load_expected(void)
{
    DWORD v, n = sizeof v;
    if (!RegGetValueW(HKEY_CURRENT_USER, L"Software\\KFWE", L"StartupMs", RRF_RT_REG_DWORD, NULL, &v, &n) && v >= 500 && v <= 20000)
        g_expected = v;
}

static void tick(void)
{
    DWORD t = GetTickCount() - g_t0;
    if (!g_done_at) {
        Found f = { NULL, 0 };
        int ready = 0;
        EnumWindows(find_ready, (LPARAM)&f);
        if (f.main)
            g_ready_main = f.main;
        if (f.dialog) ready = 1;
        else if (f.main) {
            if (!g_pending && SendMessageCallbackW(f.main, WM_NULL, 0, 0, ping_done, t)) g_pending = 1;
            ready = g_good >= 5;
        }
        if (ready || g_click || t > TIMEOUT_MS) {
            g_done_at = t;
            if (ready) save_expected(t);
        }
    }
    if (!g_done_at) {
        g_alpha = t < 180 ? t / 180.0f : 1;
        g_prog = 0.92f * (1 - expf(-2.0f * t / g_expected));
    } else {
        DWORD d = t - g_done_at;
        g_prog += (1 - g_prog) * 0.35f;
        g_alpha = d < 120 ? (g_alpha < 1 ? g_alpha : 1) : 1 - (d - 120) / 200.0f;
        if (g_alpha <= 0) { DestroyWindow(g_wnd); return; }
    }
    render_frame();
    present();
}

static LRESULT CALLBACK wnd_proc(HWND h, UINT m, WPARAM w, LPARAM l)
{
    if (m == WM_TIMER) { tick(); return 0; }
    if (m == WM_DESTROY) { PostQuitMessage(0); return 0; }
    if (m == WM_LBUTTONDOWN) { g_click = 1; return 0; }
    return DefWindowProcW(h, m, w, l);
}

static void read_version(void)
{
    WCHAR path[MAX_PATH];
    GetModuleFileNameW(g_inst, path, MAX_PATH);
    for (int up = 0; up < 3; up++) { WCHAR *s = wcsrchr(path, L'\\'); if (s) *s = 0; }
    static const WCHAR *langs[] = { L"en-US", L"zh-CN" };
    for (int i = 0; i < 2 && !g_version[0]; i++) {
        WCHAR f[MAX_PATH];
        swprintf(f, MAX_PATH, L"%ls\\share\\locale\\%ls\\script.lng", path, langs[i]);
        FILE *fp = _wfopen(f, L"rb");
        if (!fp) continue;
        char buf[8192];
        size_t n = fread(buf, 1, sizeof buf - 1, fp);
        fclose(fp);
        buf[n] = 0;
        char *v = strstr(buf, "KFWE v");
        if (!v) continue;
        v += 5;
        int k = 0;
        while (k < 30 && (v[k] == 'v' || v[k] == '.' || (v[k] >= '0' && v[k] <= '9'))) { g_version[k] = v[k]; k++; }
        g_version[k] = 0;
    }
}

static DWORD WINAPI splash_thread(void *arg)
{
    (void)arg;
    SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_BELOW_NORMAL);
    HMODULE u32 = GetModuleHandleW(L"user32.dll");
    void *(WINAPI *set_ctx)(void *) = (void *(WINAPI *)(void *))GetProcAddress(u32, "SetThreadDpiAwarenessContext");
    if (set_ctx) set_ctx((void *)-4);
    POINT pt;
    GetCursorPos(&pt);
    HMONITOR mon = MonitorFromPoint(pt, MONITOR_DEFAULTTOPRIMARY);
    MONITORINFO mi = { sizeof mi };
    GetMonitorInfoW(mon, &mi);
    HMODULE shc = set_ctx ? LoadLibraryW(L"shcore.dll") : NULL;
    HRESULT (WINAPI *dpi_for)(HMONITOR, int, UINT *, UINT *) = shc ? (HRESULT (WINAPI *)(HMONITOR, int, UINT *, UINT *))GetProcAddress(shc, "GetDpiForMonitor") : NULL;
    UINT dx = 96, dy = 96;
    if (dpi_for && dpi_for(mon, 0, &dx, &dy) == S_OK) S = dx / 96.0f;
    W = (int)px(CARD_W + MARGIN * 2);
    H = (int)px(CARD_H + MARGIN * 2);

    ULONG_PTR tok;
    GdipInput gi = { 1, NULL, FALSE, FALSE };
    if (GdiplusStartup(&tok, &gi, NULL)) return 0;
    HDC sdc = GetDC(NULL);
    BITMAPINFO bi = { { sizeof(BITMAPINFOHEADER), W, -H, 1, 32, BI_RGB } };
    g_mdc = CreateCompatibleDC(sdc);
    g_dib = CreateDIBSection(sdc, &bi, DIB_RGB_COLORS, (void **)&g_bits, NULL, 0);
    ReleaseDC(NULL, sdc);
    g_base = malloc((size_t)W * H * 4);
    if (!g_dib || !g_base) return 0;
    SelectObject(g_mdc, g_dib);
    if (GdipCreateBitmapFromScan0(W, H, W * 4, PF_PARGB, g_bits, &g_img) || GdipGetImageGraphicsContext(g_img, &g_g)) return 0;
    GdipSetSmoothingMode(g_g, 4);
    GdipSetPixelOffsetMode(g_g, 4);
    GdipSetTextRenderingHint(g_g, 4);
    read_version();
    load_expected();
    render_base();

    WNDCLASSW wc = { 0, wnd_proc, 0, 0, g_inst, NULL, LoadCursor(NULL, IDC_APPSTARTING), NULL, NULL, L"KFWESplash" };
    RegisterClassW(&wc);
    RECT wa = mi.rcWork;
    int x = (wa.left + wa.right - W) / 2, y = (wa.top + wa.bottom - H) / 2;
    g_wnd = CreateWindowExW(WS_EX_LAYERED | WS_EX_TOOLWINDOW | WS_EX_NOACTIVATE | WS_EX_TOPMOST, L"KFWESplash", L"KFWE", WS_POPUP, x, y, W, H, NULL, NULL, g_inst, NULL);
    if (!g_wnd) return 0;
    g_alpha = 0;
    render_frame();
    present();
    ShowWindow(g_wnd, SW_SHOWNOACTIVATE);
    SetTimer(g_wnd, 1, 16, NULL);
    MSG msg;
    while (GetMessageW(&msg, NULL, 0, 0) > 0) DispatchMessageW(&msg);
    Sleep(1500);
    if (!g_ready_main) {
        Found f = { NULL, 0 };
        EnumWindows(find_ready, (LPARAM)&f);
        g_ready_main = f.main;
    }
    if (g_ready_main)
        PostMessageW(g_ready_main, WM_COMMAND, 0xBE00, 0);
    GdipDeleteGraphics(g_g);
    GdipDisposeImage(g_img);
    DeleteDC(g_mdc);
    DeleteObject(g_dib);
    free(g_base);
    UnregisterClassW(L"KFWESplash", g_inst);
    return 0;
}

static void skip_welcome(void)
{
    static const BYTE expect[] = { 0x6A, 0x00, 0x68, 0xA4, 0x01, 0x00, 0x00, 0x68, 0xE8, 0x01, 0x7C, 0x00 };
    BYTE *exe = (BYTE *)GetModuleHandleW(NULL), *at = (BYTE *)0x4E4961, *next = (BYTE *)0x4E498B;
    IMAGE_NT_HEADERS *nt = (IMAGE_NT_HEADERS *)(exe + ((IMAGE_DOS_HEADER *)exe)->e_lfanew);
    DWORD old;
    if (exe != (BYTE *)0x400000 || nt->OptionalHeader.SizeOfImage < 0x0E5000) return;
    if (memcmp(at, expect, sizeof expect) || next[0] != 0xBA) return;
    if (!VirtualProtect(at, 5, PAGE_EXECUTE_READWRITE, &old)) return;
    at[0] = 0xE9;
    *(INT32 *)(at + 1) = (INT32)(next - (at + 5));
    VirtualProtect(at, 5, old, &old);
    FlushInstructionCache(GetCurrentProcess(), at, 5);
}

__declspec(dllexport) int KFSplashStart(void)
{
    if (g_thread) return 1;
    skip_welcome();
    g_t0 = GetTickCount();
    g_thread = CreateThread(NULL, 0, splash_thread, NULL, 0, NULL);
    return g_thread ? 1 : 0;
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
