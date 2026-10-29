local ydwe, map_path, input, output, result_path = ...

package.path = ydwe .. '\\script\\common\\?.lua;' .. ydwe .. '\\script\\ydwe\\?.lua'
package.cpath = ydwe .. '\\bin\\?.dll'

local logfile = io.open(ydwe .. '\\logs\\template.log', 'w')
local function writer(level)
    return function (...)
        if not logfile then return end
        local t = table.pack(...)
        for i = 1, t.n do t[i] = tostring(t[i]) end
        logfile:write(os.date('%H:%M:%S '), level, '  ', table.concat(t, '\t'), '\n')
        logfile:flush()
    end
end
package.loaded['log'] = { trace = writer 'trace', debug = writer 'debug', info = writer 'info', warn = writer 'warn', error = writer 'error' }

require 'utiliy'

table.insert(package.searchers, function (name)
    local filename
    if name == 'slk' then
        filename = fs.ydwe_devpath() / 'compiler' / 'script' / 'slk.lua'
    elseif name:sub(1, 9) == 'compiler.' then
        filename = fs.ydwe_devpath() / 'compiler' / 'script' / (name:sub(10):gsub('%.', '/') .. '.lua')
    else
        return ("\n\tno file '%s'"):format(name)
    end
    local f, err = loadfile(filename:string())
    if not f then
        error(("error loading module '%s' from file '%s':\n\t%s"):format(name, filename:string(), err))
    end
    return f, filename:string()
end)

local result = { ok = false, imports = {} }
gui = {
    error_message = function (_, fmt, ...)
        result.error = select('#', ...) > 0 and fmt:format(...) or tostring(fmt)
    end,
    message = function (_, text)
        result.report = text
    end,
}
we = {
    import_customdata = function (type, path)
        result.imports[#result.imports + 1] = { type, path:string() }
    end,
}

local ok, err = xpcall(function ()
    local template = require 'compiler.template'
    result.ok = template:compile { input = fs.path(input), output = fs.path(output), map_path = fs.path(map_path) }
    require 'ev'.emit('编译地图', result.ok)
end, debug.traceback)
if not ok then
    log.error(err)
    result.ok = false
    result.error = tostring(err)
end

local lines = { 'return {', ('ok = %s,'):format(result.ok) }
if result.error then lines[#lines + 1] = ('error = %q,'):format(result.error) end
if result.report then lines[#lines + 1] = ('report = %q,'):format(result.report) end
lines[#lines + 1] = 'imports = {'
for _, imp in ipairs(result.imports) do
    lines[#lines + 1] = ('{ %d, %q },'):format(imp[1], imp[2])
end
lines[#lines + 1] = '},'
lines[#lines + 1] = '}'
io.save(fs.path(result_path), table.concat(lines, '\n'))
if logfile then logfile:close() end
os.exit(result.ok and 0 or 1)
