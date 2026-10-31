local ffi = require 'ffi'

pcall(ffi.cdef, [[
    void* __stdcall LoadLibraryA(const char* name);
    void* __stdcall GetProcAddress(void* module, const char* name);
    int __stdcall VirtualProtect(void* addr, size_t size, uint32_t prot, uint32_t* old);
    short __stdcall GetKeyState(int key);
]])

local function require_dpi_unaware(name)
    if package.loaded[name] then
        return package.loaded[name]
    end
    local stubs = {
        { 'user32.dll', 'SetProcessDPIAware', { 0xB8, 0x01, 0x00, 0x00, 0x00, 0xC3 } },
        { 'user32.dll', 'SetProcessDpiAwarenessInternal', { 0xB8, 0x01, 0x00, 0x00, 0x00, 0xC2, 0x04, 0x00 } },
        { 'shcore.dll', 'SetProcessDpiAwareness', { 0x31, 0xC0, 0xC2, 0x04, 0x00 } },
    }
    local patched = {}
    for _, s in ipairs(stubs) do
        local mod = ffi.C.LoadLibraryA(s[1])
        local fn = mod ~= nil and ffi.C.GetProcAddress(mod, s[2])
        if fn and fn ~= nil then
            local p = ffi.cast('uint8_t*', fn)
            local old = ffi.new('uint32_t[1]')
            local saved = {}
            ffi.C.VirtualProtect(p, #s[3], 0x40, old)
            for i = 1, #s[3] do saved[i] = p[i - 1]; p[i - 1] = s[3][i] end
            patched[#patched + 1] = { p, saved, old[0] }
        end
    end
    local ok, res = pcall(require, name)
    for _, e in ipairs(patched) do
        for i = 1, #e[2] do e[1][i - 1] = e[2][i] end
        ffi.C.VirtualProtect(e[1], #e[2], e[3], ffi.new('uint32_t[1]'))
    end
    if not ok then error(res) end
    return res
end

local gui = require_dpi_unaware 'yue.gui'

local C = {
    bg = '#1e1f24', head = '#2a2c33', accent = '#9d86ff', accent_hover = '#b3a1ff',
    text = '#f2f2f5', dim = '#8a8f9e', faint = '#4a4d58', ok = '#5fd38d', bad = '#ff6b6b',
    field = '#141519',
    btn = '#363944', btn_hover = '#454959', close_hover = '#c94f4f',
}
local CHARS = '0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ'
local last_used = {}

local function font(size, weight, name)
    return gui.Font.create(name or 'Segoe UI', size, weight or 'normal', 'normal')
end

local function label(text, size, color, weight)
    local l = gui.Label.create(text)
    l:setfont(font(size, weight))
    l:setcolor(color)
    return l
end

local function button(text, fg, color, hover, style, on)
    local b = label(text, 14, fg, 'bold')
    b:setbackgroundcolor(color)
    b:setstyle(style)
    function b:onmouseenter() self:setbackgroundcolor(hover) end
    function b:onmouseleave() self:setbackgroundcolor(color) end
    function b:onmousedown() on() return true end
    return b
end

local function successor(id)
    local t = { id:byte(1, 4) }
    for pos = 4, 2, -1 do
        local i = CHARS:find(string.char(t[pos]), 1, true)
        if i and i < #CHARS then
            t[pos] = CHARS:byte(i + 1)
            return string.char(table.unpack(t))
        end
        t[pos] = CHARS:byte(1)
    end
end

local function next_free(id, check)
    for _ = 1, 5000 do
        id = successor(id)
        if not id then return end
        if not check(id) then return id end
    end
end

local function key_char(key)
    if #key ~= 1 then
        return
    end
    local shift = ffi.C.GetKeyState(0x10) < 0
    if key:match('%a') then
        local caps = ffi.C.GetKeyState(0x14) % 2 == 1
        return (shift ~= caps) and key:upper() or key:lower()
    end
    if key:match('%d') and not shift then
        return key
    end
    if key == '-' and shift then
        return '_'
    end
end

return function(default, type_name, check)
    local function safe_check(r)
        local ok, err = xpcall(check, debug.traceback, r)
        if not ok then
            log.error(err)
            return nil
        end
        return err
    end

    local win = gui.Window.create { frame = false }
    win:sethasshadow(true)
    win:setresizable(false)
    win:setmaximizable(false)
    win:setminimizable(false)
    win:setalwaysontop(true)
    win:setcontentsize { width = 400, height = 304 }

    local root = gui.Container.create()
    root:setbackgroundcolor(C.bg)
    win:setcontentview(root)

    local head = gui.Container.create()
    head:setstyle { Height = 44, FlexDirection = 'row', AlignItems = 'center' }
    head:setbackgroundcolor(C.head)
    head:setmousedowncanmovewindow(true)
    root:addchildview(head)

    local title = label('Новый ID  ·  ' .. type_name, 15, C.accent, 'bold')
    title:setalign('start')
    title:setstyle { FlexGrow = 1, Height = 44, MarginLeft = 18 }
    title:setmousedowncanmovewindow(true)
    head:addchildview(title)

    local body = gui.Container.create()
    body:setstyle { FlexGrow = 1, Padding = 18, PaddingTop = 14 }
    root:addchildview(body)

    local caption = label('Введите 4 символа или оставьте пустым', 13, C.dim)
    caption:setalign('start')
    caption:setstyle { Height = 20 }
    body:addchildview(caption)

    local frame = gui.Container.create()
    frame:setstyle { Height = 46, MarginTop = 6, Padding = 2 }
    frame:setbackgroundcolor(C.accent)
    body:addchildview(frame)

    local field = gui.Container.create()
    field:setstyle { FlexGrow = 1, PaddingLeft = 14 }
    field:setbackgroundcolor(C.field)
    field:setfocusable(true)
    frame:addchildview(field)

    local value = gui.Label.create('')
    value:setfont(gui.Font.create('Consolas', 24, 'bold', 'normal'))
    value:setalign('start')
    value:setstyle { FlexGrow = 1 }
    field:addchildview(value)

    local status = label('', 13, C.dim, 'bold')
    status:setalign('start')
    status:setstyle { Height = 22, MarginTop = 8 }
    body:addchildview(status)

    local chips = gui.Container.create()
    chips:setstyle { FlexDirection = 'row', MarginTop = 6, MarginLeft = -5, MarginRight = -5 }
    body:addchildview(chips)

    local actions = gui.Container.create()
    actions:setstyle { FlexDirection = 'row', MarginTop = 8, MarginLeft = -5, MarginRight = -5 }
    body:addchildview(actions)

    local hint = label('Enter  создать      Esc  по умолчанию', 12, C.dim)
    hint:setstyle { Height = 20, MarginTop = 6 }
    body:addchildview(hint)

    local text = ''
    local result = default
    local closed = false
    local function close()
        if closed then return end
        closed = true
        win:close()
    end

    local function refresh()
        if text == '' then
            value:settext(default)
            value:setcolor(C.faint)
            status:settext('Будет использован ' .. default)
            status:setcolor(C.dim)
            return
        end
        value:settext(text .. (#text < 4 and '_' or ''))
        value:setcolor(C.text)
        local err = safe_check(text)
        if err then
            status:settext(err)
            status:setcolor(#text == 4 and C.bad or C.dim)
        else
            status:settext(text .. ' свободен')
            status:setcolor(C.ok)
        end
    end

    local function finish(r)
        if r == '' or r == default then
            result = default
            return close()
        end
        if safe_check(r) then
            return refresh()
        end
        result = r
        last_used[type_name] = r
        close()
    end

    local function fill(id)
        text = id
        refresh()
        field:focus()
    end

    local function paste()
        local ok, s = pcall(function() return gui.Clipboard.get():gettext() end)
        if ok and type(s) == 'string' then
            fill((s:gsub('[^%w_]', '')):sub(1, 4))
        end
    end

    local chip_style = { Height = 34, Width = 177, Margin = 5 }
    chips:addchildview(button('Предложенный  ' .. default, C.text, C.btn, C.btn_hover, chip_style, function() fill(default) end))
    local series = last_used[type_name] and next_free(last_used[type_name], safe_check)
    if series then
        chips:addchildview(button('Дальше по серии  ' .. series, C.text, C.btn, C.btn_hover, chip_style, function() fill(series) end))
    end

    local action_style = { Height = 42, Width = 177, Margin = 5 }
    actions:addchildview(button('Создать', '#16161a', C.accent, C.accent_hover, action_style, function() finish(text) end))
    actions:addchildview(button('По умолчанию', C.text, C.btn, C.btn_hover, action_style, function() finish(default) end))

    head:addchildview(button('×', C.dim, C.head, C.close_hover, { Width = 48, Height = 44 }, function() finish(default) end))

    function win:onclose()
        closed = true
        gui.MessageLoop.quit()
    end

    function field:onmousedown()
        self:focus()
        return true
    end

    function field:onkeydown(ev)
        local key = ev.key
        if key == 'Enter' then
            finish(text)
        elseif key == 'Escape' then
            finish(default)
        elseif key == 'Backspace' then
            text = text:sub(1, -2)
            refresh()
        elseif key == 'Delete' then
            text = ''
            refresh()
        elseif (key == 'V' or key == 'v') and ffi.C.GetKeyState(0x11) < 0 then
            paste()
        else
            local ch = key_char(key)
            if ch and #text < 4 then
                text = text .. ch
                refresh()
            end
        end
        return true
    end

    refresh()
    win:center()
    win:activate()
    field:focus()
    gui.MessageLoop.postdelayedtask(50, function() if not closed then field:focus() end end)
    gui.MessageLoop.run()
    return result
end
