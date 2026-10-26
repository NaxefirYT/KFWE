#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <commctrl.h>
#include "fastsub.h"
#include <stdlib.h>
#include <wchar.h>

#define IDC_STD 12
#define IDC_IMP 13
#define IDC_CUSTOM 14
#define IDC_IMPLIST 15
#define IDC_PATH 16
#define IDC_CAT 17
#define IDC_ITEM 18
#define IDC_SEARCH 0x7E01
#define MAX_HITS 500
#define MAX_ROWS 14
#define PROP L"KFModelSearch"

typedef struct { int cat, idx, namelen; wchar_t *name, *low; } Entry;
typedef struct { HWND dlg, cat, item, edit, pop, list, lb; HFONT font; int mode; Entry *le; int ln; wchar_t *lfirst, *llast; } State;
#define MODE_MODEL 0
#define MODE_LIST 1
#define POPUP_CLASS L"KFModelSearchPopup"

static HINSTANCE g_inst;
static Entry *g_ent;
static int g_n, g_cap;
static wchar_t **g_cats;
static int g_ncat;
static wchar_t *g_sig;

static wchar_t *cb_text(HWND cb, int i)
{
    int n = (int)SendMessageW(cb, CB_GETLBTEXTLEN, i, 0);
    wchar_t *s = malloc((n > 0 ? n + 1 : 1) * sizeof *s);
    if (n <= 0 || SendMessageW(cb, CB_GETLBTEXT, i, (LPARAM)s) < 0)
        s[0] = 0;
    return s;
}

static void fold(wchar_t *s)
{
    CharLowerBuffW(s, (DWORD)wcslen(s));
    for (; *s; s++)
        if (*s == L'ё') *s = L'е';
}

static void pick(State *st, HWND cb, int id, int i)
{
    SendMessageW(cb, CB_SETCURSEL, i, 0);
    SendMessageW(st->dlg, WM_COMMAND, MAKEWPARAM(id, CBN_SELCHANGE), (LPARAM)cb);
}

static void free_index(void)
{
    for (int i = 0; i < g_n; i++) { free(g_ent[i].name); free(g_ent[i].low); }
    for (int i = 0; i < g_ncat; i++) free(g_cats[i]);
    free(g_ent); free(g_cats); free(g_sig);
    g_ent = NULL; g_cats = NULL; g_sig = NULL; g_n = g_cap = g_ncat = 0;
}

static void build_index(State *st)
{
    int nc = (int)SendMessageW(st->cat, CB_GETCOUNT, 0, 0);
    if (nc <= 0) return;
    wchar_t **names = malloc(nc * sizeof *names);
    size_t len = 1;
    for (int i = 0; i < nc; i++) { names[i] = cb_text(st->cat, i); len += wcslen(names[i]) + 1; }
    wchar_t *sig = malloc(len * sizeof *sig);
    sig[0] = 0;
    for (int i = 0; i < nc; i++) { wcscat(sig, names[i]); wcscat(sig, L"\n"); }
    if (g_sig && !wcscmp(sig, g_sig)) {
        for (int i = 0; i < nc; i++) free(names[i]);
        free(names); free(sig);
        return;
    }
    free_index();
    g_cats = names; g_ncat = nc; g_sig = sig;

    HCURSOR old = SetCursor(LoadCursor(NULL, IDC_WAIT));
    int c0 = (int)SendMessageW(st->cat, CB_GETCURSEL, 0, 0), m0 = (int)SendMessageW(st->item, CB_GETCURSEL, 0, 0);
    for (int c = 0; c < nc; c++) {
        pick(st, st->cat, IDC_CAT, c);
        int n = (int)SendMessageW(st->item, CB_GETCOUNT, 0, 0);
        for (int j = 0; j < n; j++) {
            if (g_n == g_cap) { g_cap = g_cap ? g_cap * 2 : 1024; g_ent = realloc(g_ent, g_cap * sizeof *g_ent); }
            Entry *e = &g_ent[g_n++];
            e->cat = c; e->idx = j; e->name = cb_text(st->item, j);
            e->namelen = (int)wcslen(e->name);
            e->low = malloc((e->namelen + wcslen(names[c]) + 2) * sizeof *e->low);
            wcscpy(e->low, e->name); wcscat(e->low, L" "); wcscat(e->low, names[c]);
            fold(e->low);
        }
    }
    if (c0 >= 0) pick(st, st->cat, IDC_CAT, c0);
    if (m0 >= 0) pick(st, st->item, IDC_ITEM, m0);
    SetCursor(old);
}

static wchar_t *lb_text(HWND lb, int i)
{
    int n = (int)SendMessageW(lb, LB_GETTEXTLEN, i, 0);
    wchar_t *s = malloc((n > 0 ? n + 1 : 1) * sizeof *s);
    if (n <= 0 || SendMessageW(lb, LB_GETTEXT, i, (LPARAM)s) < 0)
        s[0] = 0;
    return s;
}

static void free_list_index(State *st)
{
    for (int i = 0; i < st->ln; i++) { free(st->le[i].name); free(st->le[i].low); }
    free(st->le); free(st->lfirst); free(st->llast);
    st->le = NULL; st->lfirst = st->llast = NULL; st->ln = 0;
}

static void build_list_index(State *st)
{
    int n = (int)SendMessageW(st->lb, LB_GETCOUNT, 0, 0);
    if (n < 0) n = 0;
    if (st->le && n == st->ln && n > 0) {
        wchar_t *a = lb_text(st->lb, 0), *b = lb_text(st->lb, n - 1);
        int same = !wcscmp(a, st->lfirst) && !wcscmp(b, st->llast);
        free(a); free(b);
        if (same) return;
    }
    free_list_index(st);
    st->le = malloc((n > 0 ? n : 1) * sizeof *st->le);
    for (int i = 0; i < n; i++) {
        Entry *e = &st->le[st->ln++];
        e->cat = -1; e->idx = i; e->name = lb_text(st->lb, i);
        e->namelen = (int)wcslen(e->name);
        e->low = _wcsdup(e->name);
        fold(e->low);
    }
    st->lfirst = n ? _wcsdup(st->le[0].name) : NULL;
    st->llast = n ? _wcsdup(st->le[n - 1].name) : NULL;
}

static Entry *entries(State *st, int *n)
{
    *n = st->mode == MODE_LIST ? st->ln : g_n;
    return st->mode == MODE_LIST ? st->le : g_ent;
}

static void hide_list(State *st)
{
    if (IsWindowVisible(st->pop)) ShowWindow(st->pop, SW_HIDE);
}

static void search(State *st)
{
    wchar_t q[256], *tok[16];
    int nt = 0;
    GetWindowTextW(st->edit, q, 256);
    fold(q);
    for (wchar_t *p = q; *p && nt < 16;) {
        while (*p == L' ') *p++ = 0;
        if (!*p) break;
        tok[nt++] = p;
        while (*p && *p != L' ') p++;
    }
    if (!nt) { hide_list(st); return; }
    if (st->mode == MODE_LIST)
        build_list_index(st);
    else
        build_index(st);
    int ne;
    Entry *ents = entries(st, &ne);

    size_t l0 = wcslen(tok[0]);
    int hits = 0;
    SendMessageW(st->list, WM_SETREDRAW, FALSE, 0);
    SendMessageW(st->list, LB_RESETCONTENT, 0, 0);
    for (int pass = 0; pass < 3 && hits < MAX_HITS; pass++)
        for (int k = 0; k < ne && hits < MAX_HITS; k++) {
            Entry *e = &ents[k];
            int ok = 1;
            for (int t = 0; t < nt && ok; t++) ok = wcsstr(e->low, tok[t]) != NULL;
            if (!ok) continue;
            wchar_t *p = wcsstr(e->low, tok[0]);
            int rank = p == e->low ? 0 : (p && p + l0 <= e->low + e->namelen) ? 1 : 2;
            if (rank != pass) continue;
            SendMessageW(st->list, LB_ADDSTRING, 0, k);
            hits++;
        }
    SendMessageW(st->list, WM_SETREDRAW, TRUE, 0);
    if (!hits) { hide_list(st); return; }

    RECT re, rc;
    GetWindowRect(st->edit, &re);
    rc = re;
    if (st->mode == MODE_MODEL)
        GetWindowRect(st->cat, &rc);
    int ih = (int)SendMessageW(st->list, LB_GETITEMHEIGHT, 0, 0);
    int h = (hits < MAX_ROWS ? hits : MAX_ROWS) * ih + 2;
    int x = rc.left, w = re.right - rc.left;
    if (w < 340) {
        MONITORINFO mi = { sizeof mi };
        GetMonitorInfoW(MonitorFromWindow(st->dlg, MONITOR_DEFAULTTONEAREST), &mi);
        w = 340;
        if (x + w > mi.rcWork.right) x = mi.rcWork.right - w;
        if (x < mi.rcWork.left) x = mi.rcWork.left;
    }
    SendMessageW(st->list, LB_SETCURSEL, 0, 0);
    SetWindowPos(st->pop, HWND_TOP, x, re.bottom + 1, w, h, SWP_NOACTIVATE | SWP_SHOWWINDOW);
    InvalidateRect(st->list, NULL, TRUE);
}

static void apply(State *st, int row)
{
    int k = row < 0 ? -1 : (int)SendMessageW(st->list, LB_GETITEMDATA, row, 0), ne;
    Entry *ents = entries(st, &ne);
    hide_list(st);
    if (k < 0 || k >= ne) return;
    Entry *e = &ents[k];
    if (st->mode == MODE_LIST) {
        int i = e->idx;
        wchar_t *t = i < (int)SendMessageW(st->lb, LB_GETCOUNT, 0, 0) ? lb_text(st->lb, i) : NULL;
        if (!t || wcscmp(t, e->name))
            i = (int)SendMessageW(st->lb, LB_FINDSTRINGEXACT, (WPARAM)-1, (LPARAM)e->name);
        free(t);
        if (i >= 0) {
            SendMessageW(st->lb, LB_SETCURSEL, i, 0);
            SendMessageW(st->lb, LB_SETTOPINDEX, i > 3 ? i - 3 : 0, 0);
            SendMessageW(st->dlg, WM_COMMAND, MAKEWPARAM(GetDlgCtrlID(st->lb), LBN_SELCHANGE), (LPARAM)st->lb);
        }
        SendMessageW(st->edit, EM_SETSEL, 0, -1);
        return;
    }
    HWND std = GetDlgItem(st->dlg, IDC_STD);
    if (SendMessageW(std, BM_GETCHECK, 0, 0) != BST_CHECKED) {
        CheckRadioButton(st->dlg, IDC_STD, IDC_CUSTOM, IDC_STD);
        SendMessageW(st->dlg, WM_COMMAND, MAKEWPARAM(IDC_STD, BN_CLICKED), (LPARAM)std);
    }
    pick(st, st->cat, IDC_CAT, e->cat);
    int i = e->idx;
    wchar_t *t = i < (int)SendMessageW(st->item, CB_GETCOUNT, 0, 0) ? cb_text(st->item, i) : NULL;
    if (!t || wcscmp(t, e->name))
        i = (int)SendMessageW(st->item, CB_FINDSTRINGEXACT, (WPARAM)-1, (LPARAM)e->name);
    free(t);
    if (i >= 0) pick(st, st->item, IDC_ITEM, i);
    SendMessageW(st->edit, EM_SETSEL, 0, -1);
}

static COLORREF mix(COLORREF a, COLORREF b)
{
    return RGB((GetRValue(a) * 3 + GetRValue(b) * 2) / 5, (GetGValue(a) * 3 + GetGValue(b) * 2) / 5,
               (GetBValue(a) * 3 + GetBValue(b) * 2) / 5);
}

static void draw_item(State *st, DRAWITEMSTRUCT *d)
{
    if ((int)d->itemID < 0) return;
    int k = (int)d->itemData, ne;
    Entry *ents = entries(st, &ne);
    if (k < 0 || k >= ne) return;
    Entry *e = &ents[k];
    int sel = d->itemState & ODS_SELECTED;
    HBRUSH bg = (HBRUSH)SendMessageW(st->dlg, WM_CTLCOLORLISTBOX, (WPARAM)d->hDC, (LPARAM)st->list);
    COLORREF fg = GetTextColor(d->hDC), bk = GetBkColor(d->hDC);
    if (sel) { bg = GetSysColorBrush(COLOR_HIGHLIGHT); fg = GetSysColor(COLOR_HIGHLIGHTTEXT); bk = GetSysColor(COLOR_HIGHLIGHT); }
    else if (!bg) bg = GetSysColorBrush(COLOR_WINDOW);
    COLORREF dim = mix(fg, bk);
    FillRect(d->hDC, &d->rcItem, bg);
    SetBkMode(d->hDC, TRANSPARENT);
    HGDIOBJ of = SelectObject(d->hDC, st->font);
    RECT r = d->rcItem, rc = d->rcItem;
    r.left += 6; r.right -= 8; rc.right -= 8;
    const wchar_t *cat = e->cat >= 0 ? g_cats[e->cat] : L"";
    SetTextColor(d->hDC, dim);
    DrawTextW(d->hDC, cat, -1, &rc, DT_RIGHT | DT_SINGLELINE | DT_VCENTER | DT_NOPREFIX);
    SIZE sz;
    GetTextExtentPoint32W(d->hDC, cat, (int)wcslen(cat), &sz);
    r.right -= sz.cx + 16;
    SetTextColor(d->hDC, fg);
    DrawTextW(d->hDC, e->name, -1, &r, DT_LEFT | DT_SINGLELINE | DT_VCENTER | DT_NOPREFIX | DT_END_ELLIPSIS);
    SelectObject(d->hDC, of);
}

static LRESULT CALLBACK pop_proc(HWND h, UINT m, WPARAM w, LPARAM l)
{
    State *st = (State *)GetWindowLongPtrW(h, GWLP_USERDATA);
    switch (m) {
    case WM_MOUSEACTIVATE:
        return MA_NOACTIVATE;
    case WM_DRAWITEM:
        if (st) { draw_item(st, (DRAWITEMSTRUCT *)l); return TRUE; }
        break;
    case WM_CTLCOLORLISTBOX:
        if (st) return SendMessageW(st->dlg, m, w, l);
        break;
    case WM_SIZE:
        if (st) MoveWindow(st->list, 0, 0, LOWORD(l), HIWORD(l), FALSE);
        return 0;
    }
    return DefWindowProcW(h, m, w, l);
}

static LRESULT CALLBACK list_proc(HWND h, UINT m, WPARAM w, LPARAM l, UINT_PTR id, DWORD_PTR ref)
{
    State *st = (State *)ref;
    switch (m) {
    case WM_MOUSEACTIVATE:
        return MA_NOACTIVATE;
    case WM_MOUSEMOVE: {
        DWORD r = (DWORD)SendMessageW(h, LB_ITEMFROMPOINT, 0, l);
        if (!HIWORD(r) && (int)LOWORD(r) != (int)SendMessageW(h, LB_GETCURSEL, 0, 0))
            SendMessageW(h, LB_SETCURSEL, LOWORD(r), 0);
        return 0;
    }
    case WM_LBUTTONDOWN:
    case WM_LBUTTONDBLCLK: {
        DWORD r = (DWORD)SendMessageW(h, LB_ITEMFROMPOINT, 0, l);
        if (!HIWORD(r)) apply(st, LOWORD(r));
        return 0;
    }
    case WM_NCDESTROY:
        fs_remove(h, list_proc, id);
        break;
    }
    return fs_def(h, m, w, l);
}

static LRESULT CALLBACK edit_proc(HWND h, UINT m, WPARAM w, LPARAM l, UINT_PTR id, DWORD_PTR ref)
{
    State *st = (State *)ref;
    int open = IsWindowVisible(st->pop);
    switch (m) {
    case WM_GETDLGCODE:
        if (open && l && ((MSG *)l)->message == WM_KEYDOWN && (((MSG *)l)->wParam == VK_RETURN || ((MSG *)l)->wParam == VK_ESCAPE))
            return DLGC_WANTALLKEYS;
        break;
    case WM_KEYDOWN:
        if (open) {
            int cur = (int)SendMessageW(st->list, LB_GETCURSEL, 0, 0), cnt = (int)SendMessageW(st->list, LB_GETCOUNT, 0, 0);
            switch (w) {
            case VK_DOWN: cur++; break;
            case VK_UP: cur--; break;
            case VK_NEXT: cur += MAX_ROWS - 1; break;
            case VK_PRIOR: cur -= MAX_ROWS - 1; break;
            case VK_RETURN: apply(st, cur); return 0;
            case VK_ESCAPE: hide_list(st); return 0;
            default: return fs_def(h, m, w, l);
            }
            SendMessageW(st->list, LB_SETCURSEL, cur < 0 ? 0 : cur >= cnt ? cnt - 1 : cur, 0);
            return 0;
        }
        if (w == VK_DOWN && GetWindowTextLengthW(h)) { search(st); return 0; }
        break;
    case WM_CHAR:
        if (w == L'\r' || w == L'\n' || w == 27) return 0;
        break;
    case WM_PAINT: {
        LRESULT r = fs_def(h, m, w, l);
        if (!GetWindowTextLengthW(h)) {
            HDC dc = GetDC(h);
            SendMessageW(st->dlg, WM_CTLCOLOREDIT, (WPARAM)dc, (LPARAM)h);
            COLORREF c = mix(GetTextColor(dc), GetBkColor(dc));
            HGDIOBJ of = SelectObject(dc, st->font);
            RECT rc;
            GetClientRect(h, &rc);
            rc.left += LOWORD(SendMessageW(h, EM_GETMARGINS, 0, 0)) + 1;
            SetBkMode(dc, TRANSPARENT);
            SetTextColor(dc, c);
            DrawTextW(dc, st->mode == MODE_LIST ? L"Поиск" : L"Поиск по всем категориям", -1, &rc, DT_SINGLELINE | DT_VCENTER | DT_NOPREFIX);
            SelectObject(dc, of);
            ReleaseDC(h, dc);
        }
        return r;
    }
    case WM_KILLFOCUS:
        hide_list(st);
        break;
    case WM_MOUSEWHEEL:
        if (open) return SendMessageW(st->list, m, w, l);
        break;
    case WM_NCDESTROY:
        fs_remove(h, edit_proc, id);
        break;
    }
    return fs_def(h, m, w, l);
}

static LRESULT CALLBACK dlg_proc(HWND h, UINT m, WPARAM w, LPARAM l, UINT_PTR id, DWORD_PTR ref)
{
    State *st = (State *)ref;
    switch (m) {
    case WM_COMMAND:
        if (LOWORD(w) == IDC_SEARCH && HIWORD(w) == EN_CHANGE && (HWND)l == st->edit) {
            search(st);
            return 0;
        }
        break;
    case WM_MOVE:
        hide_list(st);
        break;
    case WM_ACTIVATE:
        if (LOWORD(w) == WA_INACTIVE) hide_list(st);
        break;
    case WM_NCDESTROY:
        if (IsWindow(st->pop)) DestroyWindow(st->pop);
        fs_remove(h, dlg_proc, id);
        RemovePropW(h, PROP);
        free_list_index(st);
        free(st);
        break;
    }
    return fs_def(h, m, w, l);
}

static int is_class(HWND h, const wchar_t *cls)
{
    wchar_t b[32];
    return h && GetClassNameW(h, b, 32) && !lstrcmpiW(b, cls);
}

static State *create_ui(HWND dlg, int mode, HWND font_from, HWND after, int x, int y, int w, int h)
{
    State *st = calloc(1, sizeof *st);
    st->dlg = dlg; st->mode = mode;
    st->font = (HFONT)SendMessageW(font_from, WM_GETFONT, 0, 0);
    st->edit = CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT", L"", WS_CHILD | WS_VISIBLE | WS_TABSTOP | ES_AUTOHSCROLL,
                               x, y, w, h, dlg, (HMENU)(INT_PTR)IDC_SEARCH, g_inst, NULL);
    st->pop = CreateWindowExW(WS_EX_TOOLWINDOW | WS_EX_NOACTIVATE, POPUP_CLASS, NULL, WS_POPUP | WS_BORDER,
                              0, 0, 10, 10, dlg, NULL, g_inst, NULL);
    if (st->pop) {
        SetWindowLongPtrW(st->pop, GWLP_USERDATA, (LONG_PTR)st);
        st->list = CreateWindowExW(0, L"LISTBOX", NULL, WS_CHILD | WS_VISIBLE | WS_VSCROLL | LBS_OWNERDRAWFIXED | LBS_NOINTEGRALHEIGHT,
                                   0, 0, 8, 8, st->pop, (HMENU)1, g_inst, NULL);
    }
    if (!st->edit || !st->list) {
        if (st->edit) DestroyWindow(st->edit);
        if (st->pop) DestroyWindow(st->pop);
        free(st);
        return NULL;
    }
    SetPropW(dlg, PROP, (HANDLE)st);
    SendMessageW(st->edit, WM_SETFONT, (WPARAM)st->font, 0);
    SendMessageW(st->list, WM_SETFONT, (WPARAM)st->font, 0);
    SetWindowPos(st->edit, after, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE);

    HDC dc = GetDC(st->list);
    HGDIOBJ of = SelectObject(dc, st->font);
    TEXTMETRICW tm;
    GetTextMetricsW(dc, &tm);
    SelectObject(dc, of);
    ReleaseDC(st->list, dc);
    SendMessageW(st->list, LB_SETITEMHEIGHT, 0, tm.tmHeight + 6);

    fs_set(dlg, dlg_proc, 1, (DWORD_PTR)st);
    fs_set(st->edit, edit_proc, 1, (DWORD_PTR)st);
    fs_set(st->list, list_proc, 1, (DWORD_PTR)st);
    return st;
}

static void child_rect(HWND dlg, HWND c, RECT *r)
{
    GetWindowRect(c, r);
    MapWindowPoints(NULL, dlg, (POINT *)r, 2);
}

static int attach_model(HWND dlg)
{
    HWND cat = GetDlgItem(dlg, IDC_CAT), item = GetDlgItem(dlg, IDC_ITEM);
    if (!is_class(cat, L"ComboBox") || !is_class(item, L"ComboBox") || !is_class(GetDlgItem(dlg, IDC_IMPLIST), L"ComboBox") ||
        !is_class(GetDlgItem(dlg, IDC_PATH), L"Edit") || !is_class(GetDlgItem(dlg, IDC_STD), L"Button") ||
        !is_class(GetDlgItem(dlg, IDC_IMP), L"Button") || !is_class(GetDlgItem(dlg, IDC_CUSTOM), L"Button"))
        return 0;
    RECT rc, ri;
    child_rect(dlg, cat, &rc);
    child_rect(dlg, item, &ri);
    int x = rc.right + 6, wd = ri.right - x;
    if (wd < 120)
        return 0;
    State *st = create_ui(dlg, MODE_MODEL, cat, cat, x, rc.top, wd, rc.bottom - rc.top);
    if (st) { st->cat = cat; st->item = item; }
    return 1;
}

static int attach_list(HWND dlg)
{
    int combos = 0, lists = 0, other = 0;
    HWND lb = NULL, combo = NULL;
    for (HWND c = GetWindow(dlg, GW_CHILD); c; c = GetWindow(c, GW_HWNDNEXT)) {
        if (is_class(c, L"ComboBox")) { combos++; if (!combo) combo = c; }
        else if (is_class(c, L"ListBox")) { lists++; lb = c; }
        else if (!is_class(c, L"Button")) other++;
    }
    int ok = is_class(GetDlgItem(dlg, IDOK), L"Button") || is_class(GetDlgItem(dlg, 10), L"Button");
    int cancel = is_class(GetDlgItem(dlg, IDCANCEL), L"Button") || is_class(GetDlgItem(dlg, 11), L"Button");
    if (combos != 2 || lists != 1 || other || !ok || !cancel)
        return 0;
    LONG style = GetWindowLongW(lb, GWL_STYLE);
    if (!(style & LBS_NOTIFY) || (style & (LBS_OWNERDRAWFIXED | LBS_OWNERDRAWVARIABLE | LBS_MULTIPLESEL | LBS_EXTENDEDSEL)))
        return 0;
    RECT rl, rcb;
    child_rect(dlg, lb, &rl);
    child_rect(dlg, combo, &rcb);
    int eh = rcb.bottom - rcb.top, gap = 4;
    if (rl.bottom - rl.top < 4 * eh || rl.right - rl.left < 120)
        return 0;
    SetWindowPos(lb, NULL, rl.left, rl.top + eh + gap, rl.right - rl.left, rl.bottom - rl.top - eh - gap, SWP_NOZORDER | SWP_NOACTIVATE);
    State *st = create_ui(dlg, MODE_LIST, lb, combo, rl.left, rl.top, rl.right - rl.left, eh);
    if (st)
        st->lb = lb;
    return 1;
}

static void attach(HWND dlg)
{
    if (GetPropW(dlg, PROP) || GetDlgItem(dlg, IDC_SEARCH))
        return;
    if (!attach_model(dlg))
        attach_list(dlg);
}

static void CALLBACK on_show(HWINEVENTHOOK hk, DWORD ev, HWND h, LONG obj, LONG child, DWORD tid, DWORD t)
{
    (void)hk; (void)ev; (void)tid; (void)t;
    if (h && obj == OBJID_WINDOW && child == CHILDID_SELF && GetClassLongW(h, GCW_ATOM) == 32770)
        attach(h);
}

__declspec(dllexport) int ModelSearchInit(void)
{
    static int done;
    if (done)
        return done;
    WNDCLASSW wc = { CS_DROPSHADOW, pop_proc, 0, 0, g_inst, NULL, LoadCursor(NULL, IDC_ARROW), NULL, NULL, POPUP_CLASS };
    if (!RegisterClassW(&wc))
        return done = -2;
    done = SetWinEventHook(EVENT_OBJECT_SHOW, EVENT_OBJECT_SHOW, g_inst, on_show, GetCurrentProcessId(), 0,
                               WINEVENT_INCONTEXT) ? 1 : -1;
    return done;
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
