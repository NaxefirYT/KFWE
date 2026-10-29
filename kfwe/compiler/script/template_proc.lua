local subprocess = require 'bee.subprocess'

local template = {}

function template:compile(op)
    log.trace("Template compilation start (separate process).")
    local root = fs.ydwe_path()
    local result_path = op.log / 'template_result.lua'
    fs.remove(result_path)
    local process = subprocess.spawn {
        (root / 'bin' / 'lua.exe'):string(),
        '-E',
        (fs.ydwe_devpath() / 'compiler' / 'script' / 'template_worker.lua'):string(),
        root:string(),
        op.map_path:string(),
        op.input:string(),
        op.output:string(),
        result_path:string(),
        console = 'disable',
        stdout = true,
        stderr = true,
        cwd = fs.war3_path():string(),
    }
    if not process then
        log.error("Template process did not start.")
        gui.error_message(nil, "Не удалось запустить компиляцию шаблонов (bin\\lua.exe).")
        return false
    end
    process.stdout:read 'a'
    local err = process.stderr:read 'a'
    local exit_code = process:wait()
    local chunk = loadfile(result_path:string())
    local res = chunk and chunk()
    if not res then
        log.error("Template process failed, exit code " .. tostring(exit_code) .. ": " .. err)
        gui.error_message(nil, err ~= '' and err or LNG.UNKNOWN)
        return false
    end
    if res.error then
        log.error("Template error processing: " .. res.error)
        gui.error_message(nil, res.error)
    end
    if res.report then
        gui.message(nil, res.report)
    end
    for _, imp in ipairs(res.imports) do
        log.info('Import customdata', imp[1], imp[2])
        we.import_customdata(imp[1], fs.path(imp[2]))
    end
    if res.ok then
        log.debug("Template compilation succeeded.")
    end
    return res.ok
end

return template
