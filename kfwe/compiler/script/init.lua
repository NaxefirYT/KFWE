local inject_code = require "compiler.inject_code"
local wave = require "compiler.wave"
local template = require "compiler.template_proc"
local jasshelper = require "compiler.jasshelper"
local ev = require 'ev'

local classic_names = {
    { 'CreateUnits', 'CreateAllUnits' },
    { 'CreateItems', 'CreateAllItems' },
    { 'CreateDestructables', 'CreateAllDestructables' },
}

local function add_classic_names(path)
    local code = io.load(path)
    if not code then
        return
    end
    local changed = false
    for _, n in ipairs(classic_names) do
        local s = code:find('function ' .. n[1] .. ' takes nothing returns nothing', 1, true)
        local e = s and code:find('\nendfunction', s, true)
        if e and not code:find('function ' .. n[2] .. ' takes', 1, true) then
            e = e + #'\nendfunction' - 1
            code = code:sub(1, e) .. '\nfunction ' .. n[2] .. ' takes nothing returns nothing\n'
                .. '    call ' .. n[1] .. '()\nendfunction' .. code:sub(e + 1)
            changed = true
        end
    end
    if changed then
        log.info('Added CreateAll* aliases for MapHelper script')
        io.save(path, code)
    end
end

local function jasshelper_cached(op)
    local code = io.load(op.input)
    local trigs = {}
    if code then
        code = code:gsub('\n[ \t]*trigger gg_trg_[%w_]+ *= *null', function (s)
            trigs[#trigs + 1] = s
            return ''
        end)
        table.sort(trigs)
        code = code .. table.concat(trigs)
    end
    local key = code
        and not code:find('//! import', 1, true)
        and not code:find('//! external', 1, true)
        and not code:find('//! loaddata', 1, true)
        and table.concat({
            tostring(op.option.enable_jasshelper),
            tostring(op.option.enable_jasshelper_debug),
            tostring(op.option.enable_jasshelper_optimization),
            tostring(op.option.pjass),
            tostring(op.option.runtime_version),
            io.load(jasshelper:prepare_common_j(op.map_path, op.option.runtime_version)) or '',
            io.load(jasshelper:prepare_blizzard_j(op.map_path, op.option.runtime_version)) or '',
            code,
        }, '\0')
    local key_path = op.log / 'jasshelper_cache.key'
    local cached = op.log / 'jasshelper_cache.j'
    if key and io.load(key_path) == key and fs.exists(cached) then
        fs.copy_file(cached, op.output, true)
        log.info('JassHelper skipped: script unchanged')
        return true
    end
    fs.remove(key_path)
    if not jasshelper:compile(op) then
        return false
    end
    if key then
        fs.copy_file(op.output, cached, true)
        io.save(key_path, key)
    end
    return true
end

local function update_script(map_path, input, process_function)
    fs.copy_file(map_path / 'war3map.j', input, true)
    add_classic_names(input)
    local output = process_function(input)
    if not output then
        log.error("Compile failed.")
        return false
    end
    fs.copy_file(output, map_path / 'war3map.j', true)
    return true
end

local function make_option(config, war3ver)
	local option = {}
	-- 是否启用JassHelper
	option.enable_jasshelper = config.ScriptCompiler.EnableJassHelper ~= "0"
	-- 是否是调试模式
	option.enable_jasshelper_debug = config.ScriptCompiler.EnableJassHelperDebug == "1"
	-- 是否优化地图
	option.enable_jasshelper_optimization = config.ScriptCompiler.EnableJassHelperOptimization ~= "0"
	-- pjass的版本
	option.pjass = config.PJass.Option
	-- 代码注入选项
    option.script_injection = tonumber(config.ScriptInjection.Option)
	-- 目标魔兽版本
	local save_type = tonumber(config.MapSave.Option)
	if save_type == 1 then
		-- 固定旧版本
		option.runtime_version = 20
	elseif save_type == 2 then
		-- 固定新版本
		option.runtime_version = 24
	else
		-- 按照当前版本或者双份
		option.runtime_version = war3ver
	end
	return option
end

local compiler = {}

function compiler:compile(map_path, config, war3ver)
    inject_code:initialize()
    
    local option = make_option(config, war3ver)
    log.trace("Compile to version " .. tostring(option.runtime_version))

    local compile_t = {
        option = option,
        map_path = map_path,
        log = fs.ydwe_path() / "logs",
    }

    return update_script(map_path, compile_t.log / "1_war3map.j",
        function (input)
            compile_t.input = input
            compile_t.output = nil
            if option.enable_jasshelper then
                if option.script_injection == 0 then
                    compile_t.output = compile_t.log / "2_inject.j"
                    if not inject_code:compile(compile_t) then
                        return
                    end
                    compile_t.input = compile_t.output
                end

                -- Wave预处理
                compile_t.output = compile_t.log / "3_wave.j"
                if not wave:compile(compile_t) then
                    return
                end
                compile_t.input = compile_t.output

                compile_t.output = compile_t.log / "4_template.j"
                if not template:compile(compile_t) then
                    ev.emit('编译地图', false)
                    collectgarbage 'collect'
                    return
                end
                ev.emit('编译地图', true)
                collectgarbage 'collect'
            end

            compile_t.input = compile_t.output
            compile_t.output = compile_t.log / "6_vjass.j"
            if not jasshelper_cached(compile_t) then
                return
            end
            return compile_t.output
        end
    )
end

return compiler
