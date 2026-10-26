local stormlib = require 'ffi.stormlib'
local w3x2lni = require 'compiler.w3x2lni.init'
local ev = require 'ev'
local map_packer = require 'w3x2lni.map_packer'

local mapdump = require 'mapdump'
local subprocess = require 'bee.subprocess'

local mt = {}
mt.__index = mt

-- command_line это пусть до ydwe.exe
-- current_dir это целый путь до ydwe.exe + загрузка тест карты
function mt:process_create(app, current_dir)
	if self.h then
		self.h:kill()
		self.h = nil
	end

	local process = subprocess.spawn {
		app:string(),
		cwd = current_dir:string(),
		hideWindow = true,
	}
	if not process then
		log.error('Executed failed: ', app:string())
		return
	end
	log.trace('Executed: ', app:string())
	self.h = process
end

function mt:__gc()
	if self.h then
		self.h:kill()
		self.h = nil
	end
end

local ydhost = setmetatable({}, mt)

local function getplayernum(mappath)
	local ok, result = pcall(function()
		local map = stormlib.open(mappath, true)
		if not map then
    		return 0
		end
		local w3i = map:load_file('war3map.w3i')
		local w2l = w3x2lni()
		local tbl = w2l:frontend_w3i(w3i)
		local n = 0
		if tbl.CONFIG.FIX_FORCE_SETTING == 0 then
			n = tbl.PLAYER.PLAYER_COUNT
		else
			for i = 1, tbl.PLAYER.PLAYER_COUNT do
    			if tbl['PLAYER'..i].TYPE == 1 then
					n = n + 1
				end
			end
		end
		map:close()
		return n
	end)
	if ok then return result end
	return 0
end

local function single_test(mappath)
	local args = {}
	args[#args + 1] = (fs.ydwe_path() / 'ydwe.exe'):string()
	args[#args + 1] = "-war3"
	args[#args + 1] = "-loadfile"
	args[#args + 1] = mappath:string()
	return args
end

local function host_copy_dll(curdir)
	pcall(fs.copy_file, fs.ydwe_path() / 'bin' / 'vcruntime140.dll', curdir / 'vcruntime140.dll', true)
	pcall(fs.copy_file, fs.ydwe_path() / 'bin' / 'msvcp140.dll', curdir / 'msvcp140.dll', true)
end

local function host_save_config(curdir, mappath, autostart)
	local ver = global_config_war3_version()
	local jasspath
	local mapsave_number = tonumber(global_config["MapSave"]["Option"])
	if mapsave_number == 3 then
		jasspath = fs.ydwe_path() / "compiler" / "jass" / "20" -- Reforged
	else
		jasspath = fs.ydwe_path() / "compiler" / "jass" / "24"
	end
	local of = io.open(curdir / 'map.cfg', 'wb')
	local ok, e = pcall(mapdump, mappath, jasspath,
		function(s)
			of:write(s .. '\n')
		end
	)
	of:close()
	if not ok then
		log.error('')
	end
	local tbl = {
		lan_war3version = ver.minor,
		bot_defaultgamename = mappath:filename():string(),
		bot_autostart = autostart,
		bot_mappath = fs.relative(mappath, fs.war3_path()):string(),
		bot_mapcfgpath = 'map.cfg',
	}
	local str = ''
	for k, v in pairs(tbl) do
		str = str .. tostring(k) .. ' = ' .. tostring(v) .. '\n'
	end
	io.save(curdir / 'ydhost.cfg', str)
end

local function host_test(mappath)
	local args = {}
	local host_test = tonumber(global_config["HostTest"]["Option"])
	local curdir = fs.ydwe_path() / 'plugin' / 'ydhost'
	host_copy_dll(curdir)
	host_save_config(curdir, mappath, host_test + 1)
	ydhost:process_create(curdir / 'ydhost.exe', curdir)

	args[#args + 1] = (fs.ydwe_path() / 'ydwe.exe'):string()
	args[#args + 1] = "-war3"
	args[#args + 1] = "-auto"
	if host_test == 0 then
		return args, 1
	end
	return args, getplayernum(mappath)
end

local function is_lni(path)
    if path:filename():string() ~= '.w3x' then
        return false
    end
    local f = io.open(path)
    if not f then
        return false
    end
    if f:seek('set', 8) and 'W2L\x01' == f:read(4) then
        f:close()
        return true
    end
    f:close()
    return false
end

local current_map_path
ev.on('打开地图', function (map_path)
	current_map_path = fs.path(map_path)
end)

-- This function is used when testing the map.
-- event_data - Event Parameters，table，Contains the following values
--	map_path - The saved map path, string type
--	application_name - Application name, string object
--	command_line - command line, string object
-- Return: non-negative indicates success, negative indicates failure. If successful and without special circumstances, 0 should be returned in principle
function event.EVENT_TEST_MAP(event_data)
	log.debug("********************* on test start *********************")

	-- Refresh configuration data
	global_config_reload()

	-- Get the name of the map currently being tested
	local mappath = fs.path(event_data.map_path)
	log.info('Current map path ' .. current_map_path:string())
	log.debug("Testing " .. mappath:string())
    log.debug("Testing " .. event_data.command_line)

	-- If the map has not been saved before testing, convert it here
	if not event_data.save then
		local mapSlk = "0" ~= global_config["MapTest"]["EnableMapSlk"]
		-- If it is an lni map, you need to repackage it
		if is_lni(current_map_path) then
			if mapSlk then
				if not map_packer('slk', current_map_path, mappath) then
					log.inifo('Slk map failed!')
					return -1
				end
			else
				if not map_packer('obj', current_map_path, mappath) then
					log.inifo('Pack map failed!')
					return -1
				end
			end
		else
			if mapSlk then
				if not map_packer('slk', current_map_path, mappath) then
					log.inifo('Slk map failed!')
					return -1
				end
			end
		end
	end

	-- Additional command line
	local mapsave_number = tonumber(global_config["MapSave"]["Option"])
	if mapsave_number == 3 then -- Reforged mode
		log.debug('TEST ')
		log.debug("YDWE PATH"..tostring(123))
		log.debug("YDWE PATH"..tostring(fs.ydwe_path()))

		local refPath = dofile(tostring(fs.ydwe_path()).."/ReforgedPath.txt")
		mappath = mappath:string()
		log.debug('Reforged Path '..refPath)
		log.debug('Map Path '..mappath)
		log.debug("Attempt to run map test in Reforged mode")
		io.popen('""'..refPath..'" -launch -loadfile "'..mappath..'""')
		log.debug("Run map test in Reforged mode is successful")
		--os.execute('""C:/Program Files (x86)/Warcraft III/_retail_/x86_64/Warcraft III.exe" -launch -loadfile "C:/Users/user/Downloads/Warcraft III 1.26/Maps/Test/WorldEditTestMap.w3x""')
		return 0
	end

	local args, n

	log.debug("Testing " .. tostring(global_config["MapTest"]["EnableHost"]))
	local playas = require('playas').for_test(current_map_path)
	local playas_file = fs.ydwe_path() / 'bin' / 'playas_now.txt'
	if playas then
		log.info('Test as player', playas)
		io.save(playas_file, tostring(playas))
	else
		pcall(fs.remove, playas_file)
	end
	if global_config["MapTest"]["EnableHost"] == "1" and not playas then
		args, n = host_test(mappath)
	else
		args, n = single_test(mappath), 1
	end
	args[#args + 1] = "-closew2l"

	local result = true
	for i = 1, n do
		if not subprocess.spawn(args) then
			log.error(string.format("Executed %s failed", table.concat(args, ' ')))
			result = false
			break
		end
		log.trace(string.format("Executed %s.", table.concat(args, ' ')))
	end

	log.debug("********************* on test end *********************")
	if result then return 0 else return -1 end
end
