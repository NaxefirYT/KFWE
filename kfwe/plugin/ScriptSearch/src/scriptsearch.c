#include <windows.h>
#include <commctrl.h>
#include <dwmapi.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#define C_BG RGB(0x17, 0x18, 0x1C)
#define C_PANEL RGB(0x1E, 0x1F, 0x24)
#define C_INPUT RGB(0x2C, 0x2C, 0x2C)
#define C_LINE RGB(0x2A, 0x2C, 0x33)
#define C_ACCENT RGB(0x7C, 0x5C, 0xFF)
#define C_SEL RGB(0x35, 0x2E, 0x5E)
#define C_TEXT RGB(0xE4, 0xE6, 0xEB)
#define C_DIM RGB(0xA4, 0xA9, 0xB6)
#define C_FAINT RGB(0x7D, 0x82, 0x90)
#define C_HIT RGB(0x6B, 0x55, 0x12)
#define C_HEAD RGB(0x26, 0x28, 0x2E)
#define C_KEY RGB(0x8F, 0xA8, 0xFF)
#define C_COMMENT RGB(0x6A, 0x99, 0x55)
#define C_STRING RGB(0xCE, 0x91, 0x78)
#define C_NUMBER RGB(0xE0, 0xB0, 0x6C)

#define WE_GET_DOC 0x00524780
#define DOC_TRIGGERS 0x38F0
#define TD_CAT_COUNT 0x14
#define TD_CATS 0x18
#define TD_HEADER_SIZE 0x838
#define TD_HEADER 0x83C
#define CAT_NAME 0x4C
#define CAT_TRIG_COUNT 0x130
#define CAT_TRIGS 0x134
#define TRIG_CUSTOM 0x28
#define TRIG_SIZE 0x34
#define TRIG_TEXT 0x38
#define TRIG_NAME 0x4C

#define SCI_GETLENGTH 2006
#define SCI_GETTEXT 2182
#define SCI_GETSELTEXT 2161
#define SCI_SETSEL 2160
#define SCI_SCROLLCARET 2169
#define SCI_GETSELECTIONSTART 2143
#define SCI_GETSELECTIONEND 2145

#define ID_EDIT 101
#define ID_LIST 102
#define ID_PREVIEW 103
#define ID_CASE 104
#define ID_WORD 105
#define ID_TIMER 1
#define ID_JUMP_TIMER 2
#define ID_SELECT_TIMER 3
#define MAX_HITS 20000
#define ROW_H 24
#define BAR_H 52
#define FOOT_H 26
#define PV_HEAD 32
#define PV_LINE 19
#define CMD_TRIGGER_EDITOR 1793

typedef struct {
    WCHAR *name;
    WCHAR *text;
    WCHAR *lower;
    int len;
    int cat;
    int trig;
    int count;
    int collapsed;
    HTREEITEM item;
} Doc;

typedef struct {
    int doc;
    int pos;
    int len;
    int line;
    int line_start;
    int line_len;
    int name_hit;
} Hit;

static HINSTANCE g_inst;
static HHOOK g_kb;
static DWORD g_tid;
static void *g_app;
static void *(__attribute__((fastcall)) *g_get_doc)(void *);
static HWND g_win, g_edit, g_list, g_preview, g_case, g_word;
static WNDPROC g_edit_proc;
static HFONT g_ui, g_mono;
static HBRUSH g_bg, g_panel, g_input;
static Doc *g_docs;
static int g_ndocs;
static Hit *g_hits;
static int g_nhits;
static int g_case_on, g_word_on;
static int g_jump_doc = -1, g_jump_pos, g_jump_len, g_jump_tries;
static WCHAR g_query[256];

static void *__attribute__((fastcall)) get_doc_hook(void *app)
{
    g_app = app;
    return g_get_doc(app);
}

static int install_app_hook(void)
{
    static const uint8_t orig[6] = { 0x8B, 0x81, 0x14, 0x01, 0x00, 0x00 };
    uint8_t *fn = (uint8_t *)GetModuleHandleW(NULL) + (WE_GET_DOC - 0x00400000);
    if (memcmp(fn, orig, 6))
        return 0;
    uint8_t *t = VirtualAlloc(NULL, 16, MEM_COMMIT | MEM_RESERVE, PAGE_EXECUTE_READWRITE);
    if (!t)
        return 0;
    memcpy(t, orig, 6);
    t[6] = 0xE9;
    *(int32_t *)(t + 7) = (int32_t)((fn + 6) - (t + 11));
    g_get_doc = (void *)t;
    DWORD old;
    if (!VirtualProtect(fn, 6, PAGE_EXECUTE_READWRITE, &old))
        return 0;
    fn[0] = 0xE9;
    *(int32_t *)(fn + 1) = (int32_t)((uint8_t *)get_doc_hook - (fn + 5));
    fn[5] = 0x90;
    VirtualProtect(fn, 6, old, &old);
    FlushInstructionCache(GetCurrentProcess(), fn, 6);
    return 1;
}

static WCHAR *decode(const char *s, int n)
{
    if (!s || n <= 0) {
        WCHAR *e = calloc(1, sizeof(WCHAR));
        return e;
    }
    int good = 0, bad = 0;
    for (int i = 0; i < n; i++) {
        unsigned char c = (unsigned char)s[i];
        if (c < 0x80)
            continue;
        int need = c >= 0xF0 ? 3 : c >= 0xE0 ? 2 : c >= 0xC2 ? 1 : 0, ok = need > 0 && i + need < n;
        for (int k = 1; ok && k <= need; k++)
            ok = ((unsigned char)s[i + k] & 0xC0) == 0x80;
        if (ok) {
            good++;
            i += need;
        } else {
            bad++;
        }
    }
    UINT cp = good >= bad ? CP_UTF8 : CP_ACP;
    int wn = MultiByteToWideChar(cp, 0, s, n, NULL, 0);
    WCHAR *w = malloc((wn + 1) * sizeof(WCHAR));
    if (!w)
        return NULL;
    MultiByteToWideChar(cp, 0, s, n, w, wn);
    w[wn] = 0;
    return w;
}

static uint8_t *trigger_data(void)
{
    if (!g_app)
        return NULL;
    uint8_t *doc = g_get_doc(g_app);
    if (!doc || IsBadReadPtr(doc + DOC_TRIGGERS, 4))
        return NULL;
    uint8_t *td = *(uint8_t **)(doc + DOC_TRIGGERS);
    return td && !IsBadReadPtr(td, TD_HEADER + 4) ? td : NULL;
}

static HWND find_child(HWND top, const WCHAR *cls, int want_largest_tree)
{
    HWND best = NULL, c = NULL;
    LRESULT count = -1;
    while ((c = FindWindowExW(top, c, NULL, NULL))) {
        WCHAR k[32];
        GetClassNameW(c, k, 32);
        if (!wcscmp(k, cls)) {
            if (!want_largest_tree)
                return c;
            LRESULT n = SendMessageW(c, TVM_GETCOUNT, 0, 0);
            if (n > count) {
                count = n;
                best = c;
            }
        }
        HWND d = find_child(c, cls, want_largest_tree);
        if (d) {
            if (!want_largest_tree)
                return d;
            LRESULT n = SendMessageW(d, TVM_GETCOUNT, 0, 0);
            if (n > count) {
                count = n;
                best = d;
            }
        }
    }
    return best;
}

static BOOL CALLBACK find_editor_cb(HWND h, LPARAM out)
{
    if (IsWindowVisible(h) && find_child(h, L"Scintilla", 0)) {
        *(HWND *)out = h;
        return FALSE;
    }
    return TRUE;
}

static HWND trigger_editor(void)
{
    HWND h = NULL;
    EnumThreadWindows(g_tid, find_editor_cb, (LPARAM)&h);
    return h;
}

static BOOL CALLBACK find_main_cb(HWND h, LPARAM out)
{
    WCHAR k[32];
    GetClassNameW(h, k, 32);
    if (!wcscmp(k, L"Warcraft III")) {
        *(HWND *)out = h;
        return FALSE;
    }
    return TRUE;
}

static HWND main_window(void)
{
    HWND h = NULL;
    EnumThreadWindows(g_tid, find_main_cb, (LPARAM)&h);
    return h;
}

static void free_docs(void)
{
    for (int i = 0; i < g_ndocs; i++) {
        free(g_docs[i].name);
        free(g_docs[i].text);
        free(g_docs[i].lower);
    }
    free(g_docs);
    g_docs = NULL;
    g_ndocs = 0;
}

static void add_doc(int cat, int trig, WCHAR *name, WCHAR *text)
{
    Doc *d = &g_docs[g_ndocs++];
    d->name = name;
    d->text = text;
    d->len = text ? lstrlenW(text) : 0;
    d->lower = text ? _wcsdup(text) : NULL;
    if (d->lower)
        CharLowerBuffW(d->lower, d->len);
    d->cat = cat;
    d->trig = trig;
    d->item = NULL;
}

static void map_tree(void)
{
    HWND te = trigger_editor();
    HWND tree = te ? find_child(te, L"SysTreeView32", 1) : NULL;
    for (int i = 0; i < g_ndocs; i++)
        g_docs[i].item = NULL;
    if (!tree)
        return;
    HTREEITEM root = TreeView_GetRoot(tree);
    int ci = 0;
    for (HTREEITEM c = TreeView_GetChild(tree, root); c; c = TreeView_GetNextSibling(tree, c), ci++) {
        int ti = 0;
        for (HTREEITEM t = TreeView_GetChild(tree, c); t; t = TreeView_GetNextSibling(tree, t), ti++)
            for (int i = 0; i < g_ndocs; i++)
                if (g_docs[i].cat == ci && g_docs[i].trig == ti) {
                    g_docs[i].item = t;
                    break;
                }
    }
    for (int i = 0; i < g_ndocs; i++)
        if (g_docs[i].cat < 0)
            g_docs[i].item = root;
}

static void apply_live_text(void)
{
    HWND te = trigger_editor();
    HWND tree = te ? find_child(te, L"SysTreeView32", 1) : NULL;
    HWND sci = te ? find_child(te, L"Scintilla", 0) : NULL;
    if (!tree || !sci || !IsWindowVisible(sci))
        return;
    HTREEITEM cur = TreeView_GetSelection(tree);
    for (int i = 0; i < g_ndocs; i++) {
        if (g_docs[i].item != cur)
            continue;
        int n = (int)SendMessageW(sci, SCI_GETLENGTH, 0, 0);
        char *buf = malloc(n + 1);
        if (!buf)
            return;
        SendMessageW(sci, SCI_GETTEXT, n + 1, (LPARAM)buf);
        WCHAR *w = decode(buf, n);
        free(buf);
        if (!w)
            return;
        free(g_docs[i].text);
        free(g_docs[i].lower);
        g_docs[i].text = w;
        g_docs[i].len = lstrlenW(w);
        g_docs[i].lower = _wcsdup(w);
        if (g_docs[i].lower)
            CharLowerBuffW(g_docs[i].lower, g_docs[i].len);
        return;
    }
}

static void build_index(void)
{
    free_docs();
    uint8_t *td = trigger_data();
    if (!td)
        return;
    int ncat = *(int *)(td + TD_CAT_COUNT);
    uint8_t **cats = *(uint8_t ***)(td + TD_CATS);
    int total = 1;
    for (int c = 0; c < ncat; c++)
        total += *(int *)(cats[c] + CAT_TRIG_COUNT);
    g_docs = calloc(total, sizeof(Doc));
    if (!g_docs)
        return;
    add_doc(-1, 0, _wcsdup(L"Заголовок карты"), decode(*(char **)(td + TD_HEADER), *(int *)(td + TD_HEADER_SIZE)));
    for (int c = 0; c < ncat; c++) {
        int n = *(int *)(cats[c] + CAT_TRIG_COUNT);
        uint8_t **trigs = *(uint8_t ***)(cats[c] + CAT_TRIGS);
        for (int t = 0; t < n; t++) {
            uint8_t *tr = trigs[t];
            const char *name = (const char *)tr + TRIG_NAME;
            int custom = *(int *)(tr + TRIG_CUSTOM);
            int size = custom ? *(int *)(tr + TRIG_SIZE) : 0;
            const char *text = size > 0 ? *(const char **)(tr + TRIG_TEXT) : NULL;
            if (size > 0 && text && !text[size - 1])
                size--;
            add_doc(c, t, decode(name, (int)strlen(name)), decode(text, size));
        }
    }
    map_tree();
    apply_live_text();
}

typedef struct {
    int doc;
    int hit;
} Row;

static Row *g_rows;
static int g_nrows;
static WCHAR g_qcmp[256];
static int g_qn;
static int g_pv_doc = -1, g_pv_line, g_pv_first;
static WNDPROC g_list_proc;
static HFONT g_bold;
static int g_cw = 7;
static RECT g_box, g_info_rect;
static WCHAR g_info_text[96];

static const WCHAR *const k_words[] = {
    L"function", L"endfunction", L"takes", L"returns", L"nothing", L"local", L"set", L"call", L"if", L"then", L"else", L"elseif",
    L"endif", L"loop", L"endloop", L"exitwhen", L"return", L"globals", L"endglobals", L"constant", L"type", L"extends", L"native",
    L"array", L"and", L"or", L"not", L"true", L"false", L"null", L"integer", L"real", L"string", L"boolean", L"code", L"handle",
    L"unit", L"item", L"player", L"timer", L"trigger", L"group", L"force", L"location", L"rect", L"effect", L"struct", L"endstruct",
    L"method", L"endmethod", L"library", L"endlibrary", L"scope", L"endscope", L"private", L"public", L"static", L"this",
};

static int is_word(WCHAR c)
{
    return c == '_' || IsCharAlphaNumericW(c);
}

static int match_at(const WCHAR *hay, int len, int pos, int qn)
{
    if (!g_word_on)
        return 1;
    if (pos > 0 && is_word(hay[pos - 1]))
        return 0;
    return pos + qn >= len || !is_word(hay[pos + qn]);
}

static int next_match(const WCHAR *s, int n, int from)
{
    if (!g_qn)
        return -1;
    for (int i = from; i + g_qn <= n; i++)
        if (!wcsncmp(s + i, g_qcmp, g_qn) && match_at(s, n, i, g_qn))
            return i;
    return -1;
}

static void add_hit(int doc, int pos, int len, int line, int ls, int ll, int name_hit)
{
    if (g_nhits >= MAX_HITS)
        return;
    g_hits[g_nhits++] = (Hit){ doc, pos, len, line, ls, ll, name_hit };
}

static void search_doc(int di)
{
    Doc *d = &g_docs[di];
    WCHAR low[256];
    int nlen = d->name ? lstrlenW(d->name) : 0;
    d->count = 0;
    if (nlen && nlen < 256) {
        memcpy(low, d->name, (nlen + 1) * sizeof(WCHAR));
        if (!g_case_on)
            CharLowerBuffW(low, nlen);
        int i = next_match(low, nlen, 0);
        if (i >= 0)
            add_hit(di, i, g_qn, 0, 0, nlen, 1);
    }
    const WCHAR *hay = g_case_on ? d->text : d->lower;
    if (!hay)
        return;
    int line = 1, ls = 0, scan = 0;
    const WCHAR *p = hay;
    while ((p = wcsstr(p, g_qcmp))) {
        int pos = (int)(p - hay);
        if (match_at(hay, d->len, pos, g_qn)) {
            for (; scan < pos; scan++)
                if (d->text[scan] == '\n' || (d->text[scan] == '\r' && d->text[scan + 1] != '\n')) {
                    line++;
                    ls = scan + 1;
                }
            int le = pos;
            while (le < d->len && d->text[le] != '\r' && d->text[le] != '\n')
                le++;
            add_hit(di, pos, g_qn, line, ls, le - ls, 0);
            d->count++;
        }
        p += g_qn;
    }
}

static void rebuild_rows(void)
{
    g_nrows = 0;
    int last_doc = -1, last_line = -1;
    for (int i = 0; i < g_nhits; i++) {
        Hit *h = &g_hits[i];
        if (h->doc != last_doc) {
            g_rows[g_nrows++] = (Row){ h->doc, -1 };
            last_doc = h->doc;
            last_line = -1;
        }
        if (h->name_hit || g_docs[h->doc].collapsed || h->line == last_line)
            continue;
        g_rows[g_nrows++] = (Row){ h->doc, i };
        last_line = h->line;
    }
    SendMessageW(g_list, LB_SETCOUNT, g_nrows, 0);
}

static int line_offset(Doc *d, int line)
{
    int cur = 1, i = 0;
    while (cur < line && i < d->len) {
        if (d->text[i] == '\n' || (d->text[i] == '\r' && d->text[i + 1] != '\n'))
            cur++;
        i++;
    }
    return i;
}

static void show_preview(void)
{
    int r = (int)SendMessageW(g_list, LB_GETCURSEL, 0, 0);
    RECT c;
    GetClientRect(g_preview, &c);
    int visible = (c.bottom - PV_HEAD) / PV_LINE;
    if (r < 0 || r >= g_nrows) {
        g_pv_doc = -1;
    } else {
        g_pv_doc = g_rows[r].doc;
        g_pv_line = g_rows[r].hit >= 0 ? g_hits[g_rows[r].hit].line : 0;
        g_pv_first = g_pv_line > visible / 3 ? g_pv_line - visible / 3 : 1;
    }
    InvalidateRect(g_preview, NULL, FALSE);
}

static int selectable(int r)
{
    return g_rows[r].hit >= 0 || r + 1 >= g_nrows || g_rows[r + 1].hit < 0;
}

static void select_row(int r)
{
    SendMessageW(g_list, LB_SETCURSEL, r, 0);
    show_preview();
}

static void step(int delta)
{
    if (!g_nrows)
        return;
    int r = (int)SendMessageW(g_list, LB_GETCURSEL, 0, 0) + delta;
    int dir = delta < 0 ? -1 : 1;
    if (r < 0)
        r = 0;
    if (r >= g_nrows)
        r = g_nrows - 1;
    while (r >= 0 && r < g_nrows && !selectable(r))
        r += dir;
    if (r >= 0 && r < g_nrows)
        select_row(r);
}

static void run_search(void)
{
    WCHAR info[96];
    GetWindowTextW(g_edit, g_qcmp, 256);
    lstrcpyW(g_query, g_qcmp);
    if (!g_case_on)
        CharLowerBuffW(g_qcmp, lstrlenW(g_qcmp));
    g_qn = lstrlenW(g_qcmp);
    g_nhits = 0;
    if (g_qn)
        for (int i = 0; i < g_ndocs && g_nhits < MAX_HITS; i++)
            search_doc(i);
    SendMessageW(g_list, WM_SETREDRAW, FALSE, 0);
    rebuild_rows();
    int files = 0, last = -1, lines = 0;
    for (int i = 0; i < g_nhits; i++) {
        if (g_hits[i].doc != last) {
            files++;
            last = g_hits[i].doc;
        }
        if (!g_hits[i].name_hit)
            lines++;
    }
    int first = -1;
    for (int r = 0; r < g_nrows && first < 0; r++)
        if (selectable(r))
            first = r;
    SendMessageW(g_list, LB_SETCURSEL, first, 0);
    SendMessageW(g_list, WM_SETREDRAW, TRUE, 0);
    InvalidateRect(g_list, NULL, TRUE);
    if (!g_ndocs)
        lstrcpyW(info, L"Нет данных карты");
    else if (!g_qn)
        wsprintfW(info, L"Триггеров: %d", g_ndocs - 1);
    else if (!g_nhits)
        lstrcpyW(info, L"Ничего не найдено");
    else
        wsprintfW(info, L"%s%d в %d", g_nhits >= MAX_HITS ? L"Больше " : L"", lines, files);
    lstrcpyW(g_info_text, info);
    InvalidateRect(g_win, &g_info_rect, FALSE);
    show_preview();
}

static int utf8_offset(const WCHAR *s, int pos)
{
    return pos ? WideCharToMultiByte(CP_UTF8, 0, s, pos, NULL, 0, NULL, NULL) : 0;
}

static int finish_jump(void)
{
    HWND te = trigger_editor();
    HWND tree = te ? find_child(te, L"SysTreeView32", 1) : NULL;
    if (!tree || g_jump_doc < 0)
        return 0;
    map_tree();
    Doc *d = &g_docs[g_jump_doc];
    if (!d->item)
        return 0;
    TreeView_SelectItem(tree, d->item);
    TreeView_EnsureVisible(tree, d->item);
    SetForegroundWindow(te);
    SetTimer(g_win, ID_SELECT_TIMER, 80, NULL);
    return 1;
}

static void select_hit(void)
{
    HWND te = trigger_editor();
    HWND sci = te ? find_child(te, L"Scintilla", 0) : NULL;
    if (sci && g_jump_doc >= 0 && g_jump_pos >= 0) {
        Doc *d = &g_docs[g_jump_doc];
        int a = utf8_offset(d->text, g_jump_pos);
        int b = utf8_offset(d->text, g_jump_pos + g_jump_len);
        SendMessageW(sci, SCI_SETSEL, a, b);
        SendMessageW(sci, SCI_SCROLLCARET, 0, 0);
        SetFocus(sci);
    }
    g_jump_doc = -1;
}

static void jump(int hide)
{
    int r = (int)SendMessageW(g_list, LB_GETCURSEL, 0, 0);
    if (r < 0 || r >= g_nrows)
        return;
    Row *row = &g_rows[r];
    g_jump_doc = row->doc;
    g_jump_pos = row->hit >= 0 ? g_hits[row->hit].pos : -1;
    g_jump_len = row->hit >= 0 ? g_hits[row->hit].len : 0;
    if (hide)
        ShowWindow(g_win, SW_HIDE);
    if (finish_jump())
        return;
    HWND m = main_window();
    if (m)
        PostMessageW(m, WM_COMMAND, CMD_TRIGGER_EDITOR, 0);
    g_jump_tries = 40;
    SetTimer(g_win, ID_JUMP_TIMER, 100, NULL);
}

static int expand_line(const WCHAR *s, int n, WCHAR *out, int cap)
{
    int o = 0;
    for (int i = 0; i < n && o < cap - 5; i++) {
        if (s[i] == '\r' || s[i] == '\n')
            break;
        if (s[i] == '\t')
            for (int k = 0; k < 4; k++)
                out[o++] = ' ';
        else
            out[o++] = s[i];
    }
    out[o] = 0;
    return o;
}

static void put_text(HDC dc, const WCHAR *s, int n, int x, int y, int right, COLORREF color)
{
    if (n <= 0 || x >= right)
        return;
    RECT clip = { x, y, right, y + PV_LINE + 4 };
    SetTextColor(dc, color);
    ExtTextOutW(dc, x, y, ETO_CLIPPED, &clip, s, n, NULL);
}

static COLORREF word_color(const WCHAR *s, int n)
{
    WCHAR w[20];
    if (n >= 20)
        return C_TEXT;
    memcpy(w, s, n * sizeof(WCHAR));
    w[n] = 0;
    for (size_t i = 0; i < sizeof k_words / sizeof *k_words; i++)
        if (!wcscmp(w, k_words[i]))
            return C_KEY;
    return C_TEXT;
}

static void draw_code(HDC dc, const WCHAR *line, int n, int x, int y, int h, int right, int skip_to_hit)
{
    static WCHAR d[1024], low[1024];
    int m = expand_line(line, n, d, 1024);
    int start = 0;
    memcpy(low, d, (m + 1) * sizeof(WCHAR));
    if (!g_case_on)
        CharLowerBuffW(low, m);
    if (skip_to_hit) {
        while (start < m && d[start] == ' ')
            start++;
        int first = next_match(low, m, 0);
        int room = (right - x) / g_cw;
        if (first >= 0 && first + g_qn - start > room - 4)
            start = first - room / 3;
    }
    for (int p = next_match(low, m, 0); p >= 0; p = next_match(low, m, p + g_qn)) {
        int a = x + (p - start) * g_cw;
        RECT r = { a < x ? x : a, y + (h - PV_LINE) / 2, a + g_qn * g_cw, y + (h + PV_LINE) / 2 };
        if (r.left < right) {
            if (r.right > right)
                r.right = right;
            HBRUSH b = CreateSolidBrush(C_HIT);
            FillRect(dc, &r, b);
            DeleteObject(b);
        }
    }
    int ty = y + (h - PV_LINE) / 2 + 1;
    for (int i = 0; i < m;) {
        int j = i;
        COLORREF color = C_TEXT;
        WCHAR c = d[i];
        if (c == '/' && d[i + 1] == '/') {
            j = m;
            color = C_COMMENT;
        } else if (c == '"' || c == '\'') {
            j = i + 1;
            while (j < m && d[j] != c) {
                if (d[j] == '\\')
                    j++;
                j++;
            }
            j = j < m ? j + 1 : m;
            color = c == '"' ? C_STRING : C_NUMBER;
        } else if (c >= '0' && c <= '9') {
            while (j < m && (is_word(d[j]) || d[j] == '.'))
                j++;
            color = C_NUMBER;
        } else if (is_word(c)) {
            while (j < m && is_word(d[j]))
                j++;
            color = word_color(d + i, j - i);
        } else {
            j = i + 1;
        }
        if (j > start) {
            int a = i < start ? start : i;
            put_text(dc, d + a, j - a, x + (a - start) * g_cw, ty, right, color);
        }
        i = j;
    }
}

static void draw_row(DRAWITEMSTRUCT *di)
{
    HDC dc = di->hDC;
    RECT r = di->rcItem;
    int sel = (di->itemState & ODS_SELECTED) != 0;
    int id = (int)di->itemID;
    int header = id >= 0 && id < g_nrows && g_rows[id].hit < 0;
    HBRUSH b = CreateSolidBrush(sel ? C_SEL : header ? C_HEAD : C_PANEL);
    FillRect(dc, &r, b);
    DeleteObject(b);
    if (id < 0 || id >= g_nrows)
        return;
    SetBkMode(dc, TRANSPARENT);
    Row *row = &g_rows[id];
    Doc *d = &g_docs[row->doc];
    if (header) {
        WCHAR count[24];
        SelectObject(dc, g_ui);
        put_text(dc, d->collapsed ? L"▸" : L"▾", 1, r.left + 10, r.top + 4, r.left + 26, C_FAINT);
        SelectObject(dc, g_bold);
        int n = lstrlenW(d->name);
        SIZE sz;
        GetTextExtentPoint32W(dc, d->name, n, &sz);
        put_text(dc, d->name, n, r.left + 28, r.top + 4, r.right - 50, C_TEXT);
        SelectObject(dc, g_ui);
        int cn = d->count ? wsprintfW(count, L"%d", d->count) : wsprintfW(count, L"имя");
        put_text(dc, count, cn, r.left + 28 + sz.cx + 10, r.top + 4, r.right - 6, C_FAINT);
        return;
    }
    Hit *h = &g_hits[row->hit];
    WCHAR num[16];
    SelectObject(dc, g_mono);
    int nn = wsprintfW(num, L"%d", h->line);
    put_text(dc, num, nn, r.left + 62 - nn * g_cw, r.top + 3, r.left + 66, C_FAINT);
    draw_code(dc, d->text + h->line_start, h->line_len, r.left + 76, r.top, ROW_H, r.right - 8, 1);
}

static void draw_toggle(DRAWITEMSTRUCT *di, int on)
{
    HBRUSH b = CreateSolidBrush(on ? C_ACCENT : C_INPUT);
    FillRect(di->hDC, &di->rcItem, b);
    DeleteObject(b);
    WCHAR t[8];
    GetWindowTextW(di->hwndItem, t, 8);
    SetBkMode(di->hDC, TRANSPARENT);
    SetTextColor(di->hDC, on ? RGB(255, 255, 255) : C_DIM);
    SelectObject(di->hDC, g_ui);
    DrawTextW(di->hDC, t, -1, &di->rcItem, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
}

static void paint_preview(HWND h)
{
    PAINTSTRUCT ps;
    HDC wdc = BeginPaint(h, &ps);
    RECT c;
    GetClientRect(h, &c);
    HDC dc = CreateCompatibleDC(wdc);
    HBITMAP bmp = CreateCompatibleBitmap(wdc, c.right, c.bottom);
    HGDIOBJ old = SelectObject(dc, bmp);
    FillRect(dc, &c, g_panel);
    SetBkMode(dc, TRANSPARENT);
    if (g_pv_doc < 0 || g_pv_doc >= g_ndocs) {
        SelectObject(dc, g_ui);
        SetTextColor(dc, C_FAINT);
        DrawTextW(dc, g_qn ? L"Ничего не найдено" : L"Начните вводить текст", -1, &c, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
    } else {
        Doc *d = &g_docs[g_pv_doc];
        RECT head = { 0, 0, c.right, PV_HEAD };
        HBRUSH hb = CreateSolidBrush(C_HEAD);
        FillRect(dc, &head, hb);
        DeleteObject(hb);
        SelectObject(dc, g_bold);
        put_text(dc, d->name, lstrlenW(d->name), 14, 7, c.right - 8, C_TEXT);
        SelectObject(dc, g_mono);
        int y = PV_HEAD + 4, line = g_pv_first;
        int i = line_offset(d, line);
        while (y < c.bottom && (i < d->len || line == g_pv_first)) {
            int e = i;
            while (e < d->len && d->text[e] != '\r' && d->text[e] != '\n')
                e++;
            if (line == g_pv_line) {
                RECT cur = { 0, y, c.right, y + PV_LINE };
                HBRUSH b = CreateSolidBrush(C_SEL);
                FillRect(dc, &cur, b);
                DeleteObject(b);
            }
            WCHAR num[16];
            int nn = wsprintfW(num, L"%d", line);
            put_text(dc, num, nn, 52 - nn * g_cw, y + 1, 58, line == g_pv_line ? C_TEXT : C_FAINT);
            if (d->text)
                draw_code(dc, d->text + i, e - i, 68, y, PV_LINE, c.right - 8, 0);
            i = e;
            if (i < d->len && d->text[i] == '\r')
                i++;
            if (i < d->len && d->text[i] == '\n')
                i++;
            line++;
            y += PV_LINE;
        }
        if (!d->len) {
            RECT note = { 0, PV_HEAD, c.right, c.bottom };
            SelectObject(dc, g_ui);
            SetTextColor(dc, C_FAINT);
            DrawTextW(dc, L"Триггер на GUI, совпало имя", -1, &note, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
        }
    }
    BitBlt(wdc, 0, 0, c.right, c.bottom, dc, 0, 0, SRCCOPY);
    SelectObject(dc, old);
    DeleteObject(bmp);
    DeleteDC(dc);
    EndPaint(h, &ps);
}

static LRESULT CALLBACK preview_proc(HWND h, UINT m, WPARAM w, LPARAM l)
{
    switch (m) {
    case WM_PAINT:
        paint_preview(h);
        return 0;
    case WM_ERASEBKGND:
        return 1;
    case WM_MOUSEWHEEL:
        g_pv_first -= GET_WHEEL_DELTA_WPARAM(w) / WHEEL_DELTA * 3;
        if (g_pv_first < 1)
            g_pv_first = 1;
        InvalidateRect(h, NULL, FALSE);
        return 0;
    }
    return DefWindowProcW(h, m, w, l);
}

static void layout(HWND h)
{
    RECT c;
    GetClientRect(h, &c);
    int w = c.right, ht = c.bottom;
    int left = w * 45 / 100;
    int body = ht - BAR_H - FOOT_H;
    g_box = (RECT){ 12, 10, w - 12 - 72 - 170, 42 };
    MoveWindow(g_edit, g_box.left + 10, g_box.top + 8, g_box.right - g_box.left - 20, 18, TRUE);
    MoveWindow(g_case, w - 12 - 170 - 64, 10, 30, 32, TRUE);
    MoveWindow(g_word, w - 12 - 170 - 32, 10, 30, 32, TRUE);
    g_info_rect = (RECT){ w - 12 - 160, 10, w - 12, 42 };
    MoveWindow(g_list, 0, BAR_H, left, body, TRUE);
    MoveWindow(g_preview, left + 1, BAR_H, w - left - 1, body, TRUE);
    InvalidateRect(h, NULL, TRUE);
}

static void toggle(int *flag, HWND button)
{
    *flag = !*flag;
    InvalidateRect(button, NULL, FALSE);
    run_search();
    SetFocus(g_edit);
}

static int common_key(WPARAM w)
{
    switch (w) {
    case VK_RETURN: jump(1); return 1;
    case VK_ESCAPE: ShowWindow(g_win, SW_HIDE); return 1;
    case VK_F3: step(GetKeyState(VK_SHIFT) & 0x8000 ? -1 : 1); return 1;
    }
    return 0;
}

static LRESULT CALLBACK edit_proc(HWND h, UINT m, WPARAM w, LPARAM l)
{
    if (m == WM_KEYDOWN) {
        if (common_key(w))
            return 0;
        switch (w) {
        case VK_DOWN: step(1); return 0;
        case VK_UP: step(-1); return 0;
        case VK_NEXT: step(15); return 0;
        case VK_PRIOR: step(-15); return 0;
        }
    }
    if (m == WM_SYSKEYDOWN && w == 'C') {
        toggle(&g_case_on, g_case);
        return 0;
    }
    if (m == WM_SYSKEYDOWN && w == 'W') {
        toggle(&g_word_on, g_word);
        return 0;
    }
    if ((m == WM_CHAR && (w == '\r' || w == 27)) || m == WM_SYSCHAR)
        return 0;
    return CallWindowProcW(g_edit_proc, h, m, w, l);
}

static LRESULT CALLBACK list_proc(HWND h, UINT m, WPARAM w, LPARAM l)
{
    if (m == WM_KEYDOWN && common_key(w))
        return 0;
    if (m == WM_CHAR && (w == '\r' || w == 27))
        return 0;
    if (m == WM_LBUTTONDOWN) {
        LRESULT it = SendMessageW(h, LB_ITEMFROMPOINT, 0, l);
        int r = LOWORD(it);
        if (!HIWORD(it) && r < g_nrows && g_rows[r].hit < 0 && (short)LOWORD(l) < 28 && g_docs[g_rows[r].doc].count) {
            g_docs[g_rows[r].doc].collapsed = !g_docs[g_rows[r].doc].collapsed;
            rebuild_rows();
            SendMessageW(h, LB_SETCURSEL, r, 0);
            InvalidateRect(h, NULL, TRUE);
            return 0;
        }
    }
    return CallWindowProcW(g_list_proc, h, m, w, l);
}

static void paint_window(HWND h)
{
    PAINTSTRUCT ps;
    HDC dc = BeginPaint(h, &ps);
    RECT c;
    GetClientRect(h, &c);
    FillRect(dc, &c, g_bg);
    FillRect(dc, &g_box, g_input);
    RECT line = { g_box.left, g_box.bottom - 2, g_box.right, g_box.bottom };
    HBRUSH b = CreateSolidBrush(C_ACCENT);
    FillRect(dc, &line, b);
    DeleteObject(b);
    RECT foot = { 12, c.bottom - FOOT_H, c.right - 12, c.bottom };
    SelectObject(dc, g_ui);
    SetBkMode(dc, TRANSPARENT);
    SetTextColor(dc, C_DIM);
    DrawTextW(dc, g_info_text, -1, &g_info_rect, DT_RIGHT | DT_VCENTER | DT_SINGLELINE);
    SetTextColor(dc, C_FAINT);
    DrawTextW(dc, L"Enter  перейти      ↑↓  по результатам      F3  следующее      Alt+C  регистр      Alt+W  слово целиком      Esc  закрыть", -1, &foot,
              DT_LEFT | DT_VCENTER | DT_SINGLELINE);
    EndPaint(h, &ps);
}

static LRESULT CALLBACK win_proc(HWND h, UINT m, WPARAM w, LPARAM l)
{
    switch (m) {
    case WM_SIZE:
        layout(h);
        show_preview();
        return 0;
    case WM_GETMINMAXINFO:
        ((MINMAXINFO *)l)->ptMinTrackSize.x = 760;
        ((MINMAXINFO *)l)->ptMinTrackSize.y = 360;
        return 0;
    case WM_ERASEBKGND:
        return 1;
    case WM_PAINT:
        paint_window(h);
        return 0;
    case WM_CTLCOLOREDIT:
    case WM_CTLCOLORSTATIC:
        SetBkColor((HDC)w, (HWND)l == g_edit ? C_INPUT : C_BG);
        SetTextColor((HDC)w, C_TEXT);
        return (LRESULT)((HWND)l == g_edit ? g_input : g_bg);
    case WM_CTLCOLORLISTBOX:
        return (LRESULT)g_panel;
    case WM_MEASUREITEM:
        ((MEASUREITEMSTRUCT *)l)->itemHeight = ROW_H;
        return TRUE;
    case WM_DRAWITEM: {
        DRAWITEMSTRUCT *di = (DRAWITEMSTRUCT *)l;
        if (di->CtlID == ID_LIST)
            draw_row(di);
        else
            draw_toggle(di, di->CtlID == ID_CASE ? g_case_on : g_word_on);
        return TRUE;
    }
    case WM_COMMAND:
        switch (LOWORD(w)) {
        case ID_EDIT:
            if (HIWORD(w) == EN_CHANGE)
                SetTimer(h, ID_TIMER, 120, NULL);
            break;
        case ID_LIST:
            if (HIWORD(w) == LBN_SELCHANGE)
                show_preview();
            else if (HIWORD(w) == LBN_DBLCLK)
                jump(1);
            break;
        case ID_CASE:
            toggle(&g_case_on, g_case);
            break;
        case ID_WORD:
            toggle(&g_word_on, g_word);
            break;
        }
        return 0;
    case WM_TIMER:
        if (w == ID_TIMER) {
            KillTimer(h, ID_TIMER);
            run_search();
        } else if (w == ID_SELECT_TIMER) {
            KillTimer(h, ID_SELECT_TIMER);
            select_hit();
        } else if (w == ID_JUMP_TIMER && (finish_jump() || --g_jump_tries <= 0)) {
            KillTimer(h, ID_JUMP_TIMER);
        }
        return 0;
    case WM_ACTIVATE:
        if (LOWORD(w) != WA_INACTIVE && g_jump_doc < 0) {
            build_index();
            run_search();
            SetFocus(g_edit);
        }
        return 0;
    case WM_CLOSE:
        ShowWindow(h, SW_HIDE);
        return 0;
    }
    return DefWindowProcW(h, m, w, l);
}

static HWND child(const WCHAR *cls, const WCHAR *text, DWORD style, DWORD ex, int id)
{
    HWND c = CreateWindowExW(ex, cls, text, WS_CHILD | WS_VISIBLE | style, 0, 0, 10, 10, g_win, (HMENU)(INT_PTR)id, g_inst, NULL);
    SendMessageW(c, WM_SETFONT, (WPARAM)g_ui, FALSE);
    return c;
}

static int create_window(void)
{
    WNDCLASSEXW wc = { sizeof wc };
    wc.lpfnWndProc = win_proc;
    wc.hInstance = g_inst;
    wc.hCursor = LoadCursor(NULL, IDC_ARROW);
    wc.lpszClassName = L"KFScriptSearch";
    RegisterClassExW(&wc);
    WNDCLASSEXW pc = wc;
    pc.lpfnWndProc = preview_proc;
    pc.lpszClassName = L"KFScriptPreview";
    RegisterClassExW(&pc);
    g_rows = malloc((MAX_HITS * 2 + 2) * sizeof(Row));
    if (!g_rows)
        return 0;
    g_bg = CreateSolidBrush(C_BG);
    g_panel = CreateSolidBrush(C_PANEL);
    g_input = CreateSolidBrush(C_INPUT);
    g_ui = CreateFontW(-13, 0, 0, 0, FW_NORMAL, 0, 0, 0, DEFAULT_CHARSET, 0, 0, CLEARTYPE_QUALITY, 0, L"Segoe UI");
    g_bold = CreateFontW(-13, 0, 0, 0, FW_SEMIBOLD, 0, 0, 0, DEFAULT_CHARSET, 0, 0, CLEARTYPE_QUALITY, 0, L"Segoe UI");
    g_mono = CreateFontW(-13, 0, 0, 0, FW_NORMAL, 0, 0, 0, DEFAULT_CHARSET, 0, 0, CLEARTYPE_QUALITY, FIXED_PITCH, L"Consolas");
    g_win = CreateWindowExW(0, wc.lpszClassName, L"Поиск по триггерам", WS_OVERLAPPEDWINDOW, CW_USEDEFAULT, CW_USEDEFAULT, 1180, 680,
                            main_window(), NULL, g_inst, NULL);
    if (!g_win)
        return 0;
    HDC dc = GetDC(g_win);
    SIZE sz;
    SelectObject(dc, g_mono);
    GetTextExtentPoint32W(dc, L"x", 1, &sz);
    g_cw = sz.cx;
    ReleaseDC(g_win, dc);
    BOOL dark = TRUE;
    DwmSetWindowAttribute(g_win, 20, &dark, sizeof dark);
    g_edit = child(L"EDIT", L"", ES_AUTOHSCROLL, 0, ID_EDIT);
    g_edit_proc = (WNDPROC)SetWindowLongPtrW(g_edit, GWLP_WNDPROC, (LONG_PTR)edit_proc);
    SendMessageW(g_edit, EM_SETCUEBANNER, TRUE, (LPARAM)L"Поиск по всем триггерам");
    g_case = child(L"BUTTON", L"Aa", BS_OWNERDRAW, 0, ID_CASE);
    g_word = child(L"BUTTON", L"W", BS_OWNERDRAW, 0, ID_WORD);
    g_list = child(L"LISTBOX", L"", WS_VSCROLL | LBS_NOTIFY | LBS_NODATA | LBS_OWNERDRAWFIXED | LBS_NOINTEGRALHEIGHT, 0, ID_LIST);
    g_list_proc = (WNDPROC)SetWindowLongPtrW(g_list, GWLP_WNDPROC, (LONG_PTR)list_proc);
    g_preview = child(L"KFScriptPreview", L"", 0, 0, ID_PREVIEW);
    layout(g_win);
    return 1;
}

static void selected_text(WCHAR *out, int cap)
{
    out[0] = 0;
    HWND te = trigger_editor();
    HWND sci = te ? find_child(te, L"Scintilla", 0) : NULL;
    if (!sci)
        return;
    int a = (int)SendMessageW(sci, SCI_GETSELECTIONSTART, 0, 0), b = (int)SendMessageW(sci, SCI_GETSELECTIONEND, 0, 0);
    if (b <= a || b - a > 200)
        return;
    char buf[256];
    SendMessageW(sci, SCI_GETSELTEXT, 0, (LPARAM)buf);
    WCHAR *w = decode(buf, (int)strlen(buf));
    if (w) {
        lstrcpynW(out, w, cap);
        free(w);
    }
}

static void open_search(void)
{
    if (!g_win && !create_window())
        return;
    WCHAR sel[256];
    selected_text(sel, 256);
    if (sel[0] && !wcschr(sel, '\n'))
        SetWindowTextW(g_edit, sel);
    SendMessageW(g_edit, EM_SETSEL, 0, -1);
    ShowWindow(g_win, SW_SHOW);
    SetForegroundWindow(g_win);
    SetFocus(g_edit);
    build_index();
    run_search();
    KillTimer(g_win, ID_TIMER);
}

static int focus_in_scintilla(void)
{
    WCHAR k[32];
    HWND f = GetFocus();
    return f && GetClassNameW(f, k, 32) && !wcscmp(k, L"Scintilla");
}

static void next_in_code(int dir)
{
    int cur = (int)SendMessageW(g_list, LB_GETCURSEL, 0, 0);
    build_index();
    run_search();
    if (!g_nrows)
        return;
    if (cur >= g_nrows)
        cur = 0;
    SendMessageW(g_list, LB_SETCURSEL, cur, 0);
    for (int n = 0; n < g_nrows; n++) {
        cur = (cur + dir + g_nrows) % g_nrows;
        if (g_rows[cur].hit >= 0) {
            select_row(cur);
            jump(0);
            return;
        }
    }
}

static LRESULT CALLBACK kb_hook(int code, WPARAM vk, LPARAM lp)
{
    if (code == HC_ACTION && vk == VK_F3 && !(lp & 0xC0000000) && g_win && g_query[0] && focus_in_scintilla()) {
        next_in_code(GetKeyState(VK_SHIFT) & 0x8000 ? -1 : 1);
        return 1;
    }
    if (code == HC_ACTION && vk == 'F' && !(lp & 0xC0000000) && (GetKeyState(VK_CONTROL) & 0x8000) && !(GetKeyState(VK_MENU) & 0x8000)) {
        int shift = (GetKeyState(VK_SHIFT) & 0x8000) != 0;
        if (shift ? trigger_editor() != NULL : focus_in_scintilla()) {
            open_search();
            return 1;
        }
    }
    return CallNextHookEx(g_kb, code, vk, lp);
}

__declspec(dllexport) int ScriptSearchInit(void)
{
    if (g_kb)
        return 1;
    g_tid = GetCurrentThreadId();
    g_hits = malloc(MAX_HITS * sizeof(Hit));
    if (!g_hits || !install_app_hook())
        return -1;
    g_kb = SetWindowsHookExW(WH_KEYBOARD, kb_hook, NULL, g_tid);
    if (!g_kb)
        return -2;
    HMODULE self;
    GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_PIN, (LPCWSTR)&ScriptSearchInit, &self);
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
