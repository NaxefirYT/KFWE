#include <windows.h>
#include <stdint.h>
#include <stdio.h>

#define GAME_BASE 0x6F000000u
static uint8_t *g_game;
#define G(va) (g_game + ((va) - GAME_BASE))
static char g_log[MAX_PATH];
static HMODULE g_self;
static int g_playas = -1;

static void logf(const char *fmt, ...)
{
    FILE *f = fopen(g_log, "a");
    if (!f) return;
    va_list ap; va_start(ap, fmt); vfprintf(f, fmt, ap); va_end(ap);
    fputc('\n', f); fclose(f);
}

static void __cdecl fix_slots(uint8_t *setup, const int32_t *slot_player)
{
    int target = g_playas;
    g_playas = -1;
    if (target < 1 || target > 11 || !setup || !slot_player) return;
    int n = *(int *)(setup + 4);
    uint8_t *s = *(uint8_t **)(setup + 8);
    if (n <= 0 || n > 12 || !s) return;
    int red = -1, dst = -1;
    for (int i = 0; i < n; i++) {
        uint8_t *r = s + i * 9;
        logf("slot %d -> player %d: pid=%u st=%u comp=%u team=%u color=%u", i, slot_player[i], r[0], r[2], r[3], r[4], r[5]);
        if (slot_player[i] == 0) red = i;
        if (slot_player[i] == target) dst = i;
    }
    if (red < 0 || dst < 0 || s[red * 9 + 2] != 2 || s[red * 9 + 3] != 0) { logf("нет слота: red=%d dst=%d", red, dst); return; }
    static const int occ[] = { 0, 1, 2, 3, 7, 8 };
    for (int k = 0; k < 6; k++) {
        uint8_t t = s[red * 9 + occ[k]]; s[red * 9 + occ[k]] = s[dst * 9 + occ[k]]; s[dst * 9 + occ[k]] = t;
    }
    s[red * 9 + 5] = s[red * 9 + 2] == 2 ? (uint8_t)slot_player[red] : 12;
    s[dst * 9 + 5] = (uint8_t)slot_player[dst];
    logf("человек пересажен из слота %d (игрок 0) в слот %d (игрок %d)", red, dst, target);
}

static int install(void)
{
    static const uint8_t orig[8] = { 0x8B, 0x4C, 0x24, 0x68, 0x8B, 0x44, 0x24, 0x6C };
    uint8_t *fn = G(0x6F5C086A);
    if (memcmp(fn, orig, 8)) return 0;
    uint8_t *t = (uint8_t *)VirtualAlloc(NULL, 64, MEM_COMMIT | MEM_RESERVE, PAGE_EXECUTE_READWRITE);
    if (!t) return 0;
    uint8_t *w = t;
    *w++ = 0x60;
    *w++ = 0x8D; *w++ = 0x84; *w++ = 0x24; *(int32_t *)w = 0x20 + 0x68; w += 4;
    *w++ = 0x50;
    *w++ = 0x8B; *w++ = 0x44; *w++ = 0x24; *w++ = 0x20 + 0x20 + 4;
    *w++ = 0x50;
    *w++ = 0xE8; *(int32_t *)w = (int32_t)((uint8_t *)fix_slots - (w + 4)); w += 4;
    *w++ = 0x83; *w++ = 0xC4; *w++ = 0x08;
    *w++ = 0x61;
    memcpy(w, orig, 8); w += 8;
    *w++ = 0xE9; *(int32_t *)w = (int32_t)((fn + 8) - (w + 4)); w += 4;
    FlushInstructionCache(GetCurrentProcess(), t, 64);
    DWORD old;
    if (!VirtualProtect(fn, 8, PAGE_EXECUTE_READWRITE, &old)) return 0;
    fn[0] = 0xE9; *(int32_t *)(fn + 1) = (int32_t)(t - (fn + 5)); fn[5] = fn[6] = fn[7] = 0x90;
    VirtualProtect(fn, 8, old, &old);
    FlushInstructionCache(GetCurrentProcess(), fn, 8);
    return 1;
}

#define L_KEY_GAP_MS 1500
typedef float (__attribute__((fastcall)) *menu_fn)(int, int, int, int, int, int);
static menu_fn g_menu;
static WNDPROC g_wndproc;
static DWORD g_last_l;
static int g_drop_l;
static int g_auto;

static void panel_speed(uint8_t *panel)
{
    if (!panel || !*(int *)(panel + 0x174)) return;
    uint8_t *frame = **(uint8_t ***)(panel + 0x178);
    if (frame) *(float *)(frame + 0x190) = 100.0f;
}

static float __attribute__((fastcall)) fast_menu(int c, int d, int a1, int a2, int a3, int a4)
{
    float t = g_menu(c, d, a1, a2, a3, a4);
    uint8_t *glue = ((uint8_t *(*)(void))G(0x6F593390))();
    if (!glue) return t;
    panel_speed(*(uint8_t **)(glue + 0x194));
    panel_speed(*(uint8_t **)(glue + 0x198));
    return 0.01f;
}

static int install_fast_menu(void)
{
    static const uint8_t orig[5] = { 0x83, 0xEC, 0x10, 0x53, 0x55 };
    uint8_t *fn = G(0x6F593990);
    if (memcmp(fn, orig, 5)) return 0;
    uint8_t *t = (uint8_t *)VirtualAlloc(NULL, 16, MEM_COMMIT | MEM_RESERVE, PAGE_EXECUTE_READWRITE);
    if (!t) return 0;
    memcpy(t, orig, 5);
    t[5] = 0xE9; *(int32_t *)(t + 6) = (int32_t)((fn + 5) - (t + 10));
    FlushInstructionCache(GetCurrentProcess(), t, 16);
    g_menu = (menu_fn)t;
    DWORD old;
    if (!VirtualProtect(fn, 5, PAGE_EXECUTE_READWRITE, &old)) return 0;
    fn[0] = 0xE9; *(int32_t *)(fn + 1) = (int32_t)((uint8_t *)fast_menu - (fn + 5));
    VirtualProtect(fn, 5, old, &old);
    FlushInstructionCache(GetCurrentProcess(), fn, 5);
    return 1;
}

static LRESULT CALLBACK game_wndproc(HWND h, UINT m, WPARAM w, LPARAM l)
{
    if ((m == WM_KEYDOWN || m == WM_KEYUP) && w == 'L' && InSendMessage()) {
        if (m == WM_KEYDOWN) {
            DWORD now = GetTickCount();
            g_drop_l = g_last_l && now - g_last_l < L_KEY_GAP_MS;
            if (!g_drop_l) g_last_l = now;
        }
        if (g_drop_l) return 0;
    }
    return CallWindowProcA(g_wndproc, h, m, w, l);
}

static BOOL CALLBACK find_game_window(HWND h, LPARAM out)
{
    DWORD pid;
    char cls[32];
    GetWindowThreadProcessId(h, &pid);
    if (pid != GetCurrentProcessId() || !GetClassNameA(h, cls, sizeof cls) || strcmp(cls, "Warcraft III")) return TRUE;
    *(HWND *)out = h;
    return FALSE;
}

static int install_l_filter(void)
{
    HWND h = NULL;
    for (int i = 0; i < 6000 && !h; i++) {
        EnumWindows(find_game_window, (LPARAM)&h);
        if (!h) Sleep(10);
    }
    if (!h) return 0;
    g_wndproc = (WNDPROC)SetWindowLongPtrA(h, GWLP_WNDPROC, (LONG_PTR)game_wndproc);
    return g_wndproc != NULL;
}

static DWORD WINAPI waiter(LPVOID p)
{
    (void)p;
    HMODULE g = NULL;
    for (int i = 0; i < 60000 && !(g = GetModuleHandleA("game.dll")); i++) Sleep(1);
    if (!g) { logf("game.dll не загрузилась"); return 0; }
    g_game = (uint8_t *)g;
    if (g_auto) {
        int menu = install_fast_menu();
        logf("автовход: меню %d, клавиша %d", menu, install_l_filter());
        return 0;
    }
    logf("game.dll по %p, игрок %d, перехват %d", (void *)g, g_playas, install());
    return 0;
}

__declspec(dllexport) int Initialize(void)
{
    GetModuleFileNameA(NULL, g_log, MAX_PATH);
    char *s = strrchr(g_log, '\\'); if (s) strcpy(s + 1, "Logs\\kf_playas.log");
    char cfg[MAX_PATH];
    GetModuleFileNameA(g_self, cfg, MAX_PATH);
    for (int up = 0; up < 3; up++) { s = strrchr(cfg, '\\'); if (s) *s = 0; }
    strcat(cfg, "\\bin\\playas_now.txt");
    FILE *f = fopen(cfg, "r");
    if (f) { if (fscanf(f, "%d", &g_playas) != 1) g_playas = -1; fclose(f); DeleteFileA(cfg); }
    g_auto = strstr(GetCommandLineA(), "-auto") != NULL;
    if (!g_auto && (g_playas < 1 || g_playas > 11)) return 1;
    CloseHandle(CreateThread(NULL, 0, waiter, NULL, 0, NULL));
    return 1;
}

BOOL WINAPI DllMain(HINSTANCE h, DWORD r, LPVOID p)
{
    (void)p;
    if (r == DLL_PROCESS_ATTACH) g_self = h;
    return TRUE;
}
