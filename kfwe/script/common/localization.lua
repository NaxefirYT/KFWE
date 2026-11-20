local i18n = require "i18n"

i18n.initialize(fs.ydwe_devpath() / "share" / "locale")
i18n.set_domain("script")

log.info('[i18n]current language', i18n.get_language())

local open = io.open

local function read_file(path)
    local f = open(path:string(), 'rb')
    if not f then
        return ''
    end
    local s = f:read('a')
    f:close()
    return (s:gsub('^\xEF\xBB\xBF', ''))
end

local function write_file(path, s)
    local f = open(path:string(), 'wb')
    if f then
        f:write(s)
        f:close()
    end
end

local ESC = { n = '\n', t = '\t', s = ' ', ['\\'] = '\\' }

local function unescape(s)
    return (s:gsub('\\(.)', function (c) return ESC[c] or '\\' .. c end))
end

local function parse(text)
    local t = {}
    for line in text:gmatch('[^\n]+') do
        line = line:gsub('^%s+', ''):gsub('%s+$', '')
        local eq = line:sub(1, 1) ~= '#' and line:find(' = ', 1, true)
        if eq then
            local k, v = line:sub(1, eq - 1):gsub('%s+$', ''), line:sub(eq + 3):gsub('^%s+', '')
            if k ~= '' and v ~= '' then
                t[unescape(k)] = unescape(v)
            end
        end
    end
    return t
end

kflang = {}

local root = fs.ydwe_devpath()
local locale = root / 'share' / 'locale'
local code = i18n.get_language()

local function dict_text(c)
    return c ~= 'ru' and read_file(locale / c / 'kfwe.txt') or ''
end

local dict = parse(dict_text(code))

kflang.code = code

function kflang.list()
    local list = { { code = 'ru', name = 'Русский' } }
    for path in locale:list_directory() do
        local c = path:filename():string()
        if c ~= 'ru' and fs.exists(path / 'kfwe.txt') then
            list[#list + 1] = { code = c, name = parse(dict_text(c))['@name'] or c }
        end
    end
    table.sort(list, function (a, b) return (a.code == 'ru') ~= (b.code == 'ru') and a.code == 'ru' or (a.code ~= 'ru' and b.code ~= 'ru' and a.code < b.code) end)
    return list
end

local function stamp(...)
    local h = 2166136261
    for _, s in ipairs { ... } do
        for i = 1, #s do
            h = ((h ~ s:byte(i)) * 16777619) & 0xFFFFFFFF
        end
    end
    return ('%08x'):format(h)
end

local XML_IN = { amp = '&', lt = '<', gt = '>', quot = '"', apos = "'" }
local XML_OUT = { ['&'] = '&amp;', ['<'] = '&lt;', ['>'] = '&gt;', ['"'] = '&quot;', ['\n'] = '&#10;' }

local function xml_tr(d, v)
    local raw = v:gsub('&#(%d+);', function (n) return utf8.char(tonumber(n)) end):gsub('&(%a+);', function (e) return XML_IN[e] end)
    local t = d[raw]
    return t and (t:gsub('[&<>"\n]', XML_OUT)) or v
end

local function set_radios(xml, list)
    local first, indent
    local out = xml:gsub('\n([ \t]*)<RadioButton[^\n]-group="Language"[^\n]-/>[^\n]*', function (ind)
        if first then
            return ''
        end
        first, indent = true, ind
        return '\n\0'
    end)
    if not first then
        return xml
    end
    local lines = {}
    for _, l in ipairs(list) do
        lines[#lines + 1] = ('\n%s<RadioButton class="NormalRadioButton" text="%s" width="212" group="Language" name="Language-%s" />'):format(indent, (l.name:gsub('[&<>"]', XML_OUT)), l.code)
    end
    return (out:gsub('\n\0', function () return table.concat(lines) end))
end

local function marked(path, mark)
    return read_file(path):find(mark, 1, true) ~= nil
end

local function make_dialog(src, list, c, d, s)
    local dst = root / 'share' / 'skins' / c / 'DialogConfig.xml'
    local mark = '<!-- kflang ' .. s .. ' -->'
    if marked(dst, mark) then
        return
    end
    local out = src:gsub('(%s[tT]ext=")([^"]*)"', function (a, v) return a .. xml_tr(d, v) .. '"' end)
        :gsub('(%stooltip=")([^"]*)"', function (a, v) return a .. xml_tr(d, v) .. '"' end)
    out = set_radios(out, list):gsub('^(<%?xml.-%?>)', function (decl) return decl .. mark end, 1)
    fs.create_directories(dst:parent_path())
    write_file(dst, out)
end

local function make_lng(name, c, d, s)
    local dst = locale / c / (name .. '.lng')
    local mark = '-- kflang ' .. s
    if marked(dst, mark) then
        return
    end
    local out, entries, cur = { mark }, {}, nil
    local text = read_file(locale / 'ru' / (name .. '.lng')):gsub('\r\n', '\n')
    for line in (text .. '\n'):gmatch('(.-)\n') do
        local key = line:match('^%[(.+)%]%s*$')
        if key then
            cur = { key = key, lines = {} }
            entries[#entries + 1] = cur
        elseif cur then
            cur.lines[#cur.lines + 1] = line
        end
    end
    for _, e in ipairs(entries) do
        local value = table.concat(e.lines, '\n'):gsub('%s+$', '')
        out[#out + 1] = '[' .. e.key .. ']'
        out[#out + 1] = d[value] or value
    end
    write_file(dst, table.concat(out, '\n') .. '\n')
end

function kflang.sync()
    local list = kflang.list()
    local names = {}
    for _, l in ipairs(list) do
        names[#names + 1] = l.code .. '=' .. l.name
    end
    names = table.concat(names, ';')
    local master = root / 'share' / 'skins' / 'ru' / 'DialogConfig.xml'
    local src = read_file(master)
    if src == '' then
        return
    end
    local fixed = set_radios(src, list)
    if fixed ~= src then
        write_file(master, fixed)
        src = fixed
    end
    local lngs = {}
    for _, name in ipairs { 'script', 'config', 'startup' } do
        lngs[name] = read_file(locale / 'ru' / (name .. '.lng'))
    end
    for _, l in ipairs(list) do
        if l.code ~= 'ru' then
            local text = dict_text(l.code)
            local d = parse(text)
            make_dialog(src, list, l.code, d, stamp(src, text, names))
            for name, ru in pairs(lngs) do
                make_lng(name, l.code, d, stamp(ru, text))
            end
        end
    end
end

function TR(s)
    return dict[s] or s
end

LNG = setmetatable({}, { __index = function (_, str)
    local res = i18n.get_text(str)
    if res == str then
        log.error('[i18n]unknown text:', str)
    end
    return dict[(res:gsub('\r\n', '\n'))] or res
end})
