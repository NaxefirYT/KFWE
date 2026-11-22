local autonull = {}

function autonull.types(common)
    local parent = {}
    for t, p in common:gmatch('%f[%w_]type%s+([%w_]+)%s+extends%s+([%w_]+)') do
        parent[t] = p
    end
    local root = parent.agent and 'agent' or 'handle'
    local types = { handle = true, [root] = true }
    for t in pairs(parent) do
        local p = t
        for _ = 1, 64 do
            if p == nil or p == root then
                break
            end
            p = parent[p]
        end
        if p == root then
            types[t] = true
        end
    end
    return types
end

local function mask(line, instr)
    local out = {}
    local i, n = 1, #line
    local cut
    while i <= n do
        if instr then
            local j = line:find('[\\"]', i)
            if not j then
                break
            end
            if line:sub(j, j) == '\\' then
                i = j + 2
            else
                instr = false
                out[#out + 1] = '""'
                i = j + 1
            end
        else
            local j = line:find('["\'/]', i)
            if not j then
                out[#out + 1] = line:sub(i)
                break
            end
            out[#out + 1] = line:sub(i, j - 1)
            local c = line:sub(j, j)
            if c == '"' then
                instr = true
                i = j + 1
            elseif c == "'" then
                local k = j + 1
                while k <= n and line:sub(k, k) ~= "'" do
                    if line:sub(k, k) == '\\' then
                        k = k + 1
                    end
                    k = k + 1
                end
                out[#out + 1] = '0'
                i = k + 1
            elseif line:sub(j + 1, j + 1) == '/' then
                cut = j
                break
            else
                out[#out + 1] = '/'
                i = j + 1
            end
        end
    end
    return table.concat(out), instr, cut
end

local function header(c)
    return c:match('^%s*function%s+[%w_]+%s+takes%s.-%sreturns%s+([%w_]+)')
        or c:match('^%s*constant%s+function%s+[%w_]+%s+takes%s.-%sreturns%s+([%w_]+)')
end

function autonull.transform(code, types)
    local eol = code:find('\r\n', 1, true) and '\r' or ''
    local lines = {}
    for line in (code .. '\n'):gmatch('(.-)\n') do
        lines[#lines + 1] = line
    end
    local info = {}
    local instr = false
    for i, line in ipairs(lines) do
        local start = instr
        local m, cut
        m, instr, cut = mask(line, instr)
        info[i] = { code = not start and m or nil, open = instr, cut = cut }
    end

    local out = {}
    local temps, temp_order = {}, {}
    local stats = { funcs = 0, sets = 0 }
    local globals_end, first_func

    local function temp(t)
        if not temps[t] then
            temps[t] = 'kfwe__ret_' .. t
            temp_order[#temp_order + 1] = t
        end
        return temps[t]
    end

    local function process(a, b, ret)
        local is_local, order, assigned = {}, {}, {}
        for k = a + 1, b - 1 do
            local c = info[k].code
            if c then
                local t, name, rest = c:match('^%s*local%s+([%w_]+)%s+([%w_]+)(.*)$')
                if t then
                    if name ~= 'array' and types[t] then
                        is_local[name] = true
                        order[#order + 1] = name
                        local init = rest:match('^%s*=%s*(.-)%s*$')
                        if init and init ~= 'null' then
                            assigned[name] = true
                        end
                    end
                else
                    local name, val = c:match('^%s*set%s+([%w_]+)%s*=%s*(.-)%s*$')
                    if name and is_local[name] and val ~= 'null' then
                        assigned[name] = true
                    end
                end
            end
        end
        local candidates = {}
        for _, name in ipairs(order) do
            if assigned[name] then
                candidates[#candidates + 1] = name
            end
        end
        if #candidates == 0 then
            return nil
        end

        local function pending(from)
            local done = {}
            for k = from, a + 1, -1 do
                local c = info[k].code
                if not c then
                    break
                end
                local name = c:match('^%s*set%s+([%w_]+)%s*=%s*null%s*$')
                if name then
                    done[name] = true
                elseif not c:match('^%s*$') then
                    break
                end
            end
            local list = {}
            for _, name in ipairs(candidates) do
                if not done[name] then
                    list[#list + 1] = name
                end
            end
            return list
        end

        local buf = { lines[a] }
        local added = 0
        local function nulls(indent, list)
            for _, name in ipairs(list) do
                buf[#buf + 1] = indent .. 'set ' .. name .. ' = null' .. eol
            end
            added = added + #list
        end

        local body_indent
        for k = a + 1, b - 1 do
            local c = info[k].code
            if not body_indent and c and not c:match('^%s*$') then
                body_indent = c:match('^(%s*)')
            end
            local indent, rest
            if c then
                indent, rest = c:match('^(%s*)return%f[^%w_](.*)$')
            end
            local list = indent and pending(k - 1)
            if not list or #list == 0 then
                buf[#buf + 1] = lines[k]
            else
                local uses = false
                local set = {}
                for _, name in ipairs(list) do
                    set[name] = true
                end
                for id in rest:gmatch('%f[%w_][%a_][%w_]*') do
                    if set[id] then
                        uses = true
                        break
                    end
                end
                if uses then
                    if info[k].open then
                        return nil
                    end
                    local line = lines[k]
                    local s = line:find('return', 1, true)
                    local expr = line:sub(s + 6, (info[k].cut or #line + 1) - 1):match('^%s*(.-)%s*$')
                    local tmp = temp(ret)
                    buf[#buf + 1] = indent .. 'set ' .. tmp .. ' = ' .. expr .. eol
                    nulls(indent, list)
                    buf[#buf + 1] = indent .. 'return ' .. tmp .. eol
                else
                    nulls(indent, list)
                    buf[#buf + 1] = lines[k]
                end
            end
        end

        if ret == 'nothing' then
            local k = b - 1
            while k > a and info[k].code and info[k].code:match('^%s*$') do
                k = k - 1
            end
            local last = info[k].code
            if k == a or not (last and last:match('^%s*return%f[^%w_]')) then
                nulls(body_indent or '    ', pending(b - 1))
            end
        end
        buf[#buf + 1] = lines[b]

        if added == 0 then
            return nil
        end
        stats.funcs = stats.funcs + 1
        stats.sets = stats.sets + added
        return buf
    end

    local i, n = 1, #lines
    while i <= n do
        local c = info[i].code
        local ret = c and header(c)
        if ret then
            first_func = first_func or #out + 1
            local j = i + 1
            while j <= n and not (info[j].code and info[j].code:match('^%s*endfunction%f[^%w_]')) do
                j = j + 1
            end
            if j > n then
                for k = i, n do
                    out[#out + 1] = lines[k]
                end
                break
            end
            local buf = process(i, j, ret)
            if buf then
                table.move(buf, 1, #buf, #out + 1, out)
            else
                table.move(lines, i, j, #out + 1, out)
            end
            i = j + 1
        else
            if not globals_end and not first_func and c and c:match('^%s*endglobals%f[^%w_]') then
                globals_end = #out + 1
            end
            out[#out + 1] = lines[i]
            i = i + 1
        end
    end

    local decls = {}
    for _, t in ipairs(temp_order) do
        local name = temps[t]
        if not code:find('%f[%w_]' .. name .. '%f[^%w_]') then
            decls[#decls + 1] = t .. ' ' .. name .. eol
        end
    end
    if #decls > 0 then
        if not globals_end then
            table.insert(decls, 1, 'globals' .. eol)
            decls[#decls + 1] = 'endglobals' .. eol
            globals_end = first_func or 1
        end
        table.move(out, globals_end, #out, globals_end + #decls)
        table.move(decls, 1, #decls, globals_end, out)
    end

    return table.concat(out, '\n'), stats
end

local function pjass(op, common, blizzard, path)
    local subprocess = require 'bee.subprocess'
    local exe, flags = 'pjass-latest.exe', {}
    if op.option.pjass == '1' then
        exe = 'pjass-classic.exe'
    elseif op.option.runtime_version ~= 24 then
        flags[1] = '+rb'
    end
    local process = subprocess.spawn {
        (fs.ydwe_path() / 'compiler' / 'pjass' / exe):string(),
        flags,
        common:string(),
        blizzard:string(),
        path:string(),
        stdout = true,
        stderr = true,
        hideWindow = true,
    }
    if not process then
        return false
    end
    local out = process.stdout:read 'a'
    process.stderr:read 'a'
    if process:wait() ~= 0 then
        log.error('Auto-null pjass:', out)
        return false
    end
    return true
end

function autonull:compile(op)
    local jasshelper = require 'compiler.jasshelper'
    local code = io.load(op.input)
    local source = op.source and io.load(op.source)
    if not code or (source and source:find('//%s*@nonull%f[^%w_]')) then
        return false
    end
    local common = jasshelper:prepare_common_j(op.map_path, op.option.runtime_version)
    local blizzard = jasshelper:prepare_blizzard_j(op.map_path, op.option.runtime_version)
    local common_text = io.load(common)
    if not common_text then
        return false
    end
    local key = table.concat({
        tostring(op.option.pjass),
        tostring(op.option.runtime_version),
        common_text,
        io.load(blizzard) or '',
        code,
    }, '\0')
    local key_path = op.log / 'autonull_cache.key'
    local cached = op.log / 'autonull_cache.j'
    if io.load(key_path) == key and fs.exists(cached) then
        fs.copy_file(cached, op.output, true)
        return true
    end
    fs.remove(key_path)
    local ok, result, stats = pcall(autonull.transform, code, autonull.types(common_text))
    if not ok then
        log.error('Auto-null failed:', result)
        return false
    end
    if stats.sets == 0 then
        return false
    end
    io.save(op.output, result)
    if not pjass(op, common, blizzard, op.output) then
        log.error('Auto-null: the result did not pass pjass, the map is saved without it')
        return false
    end
    log.info(('Auto-null: %d sets in %d functions'):format(stats.sets, stats.funcs))
    fs.copy_file(op.output, cached, true)
    io.save(key_path, key)
    return true
end

return autonull
