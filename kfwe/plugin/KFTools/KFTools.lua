local ffi = require "ffi"

local loader = {}

local function ensure_update_key()
	local ft = global_config["FeatureToggle"]
	if ft and ft["DisableAutoUpdate"] then
		return
	end
	local ok, err = pcall(function()
		local path = fs.ydwe_path() / "bin" / "EverConfig.cfg"
		local f = io.open(path, "rb")
		if not f then return end
		local text = f:read("a")
		f:close()
		if text:find("DisableAutoUpdate", 1, true) then return end
		local nl = text:find("\r\n", 1, true) and "\r\n" or "\n"
		local new, n = text:gsub("(%[FeatureToggle%][^\r\n]*\r?\n)", "%1DisableAutoUpdate = 0" .. nl, 1)
		if n == 0 then
			new = text .. nl .. "[FeatureToggle]" .. nl .. "DisableAutoUpdate = 0" .. nl
		end
		f = io.open(path, "wb")
		if not f then return end
		f:write(new)
		f:close()
		log.info('KFTools: added DisableAutoUpdate = 0 to EverConfig.cfg')
	end)
	if not ok then
		log.error('KFTools: cannot update EverConfig.cfg: ' .. tostring(err))
	end
end

loader.load = function(path)
	ensure_update_key()
	local ok, dll = pcall(ffi.load, path:string())
	if not ok then
		log.error('KFTools: load failed: ' .. tostring(dll))
		return false
	end
	pcall(ffi.cdef, [[
		int KFToolsInit(void);
		int KFToolsCopy(int with_z);
		int KFToolsCheckUpdate(void);
		int KFToolsShowChangelog(void);
	]])
	loader.dll = dll
	local r = dll.KFToolsInit()
	if r ~= 1 then
		log.error('KFTools: init failed, code ' .. tostring(r))
		return false
	end
	return true
end

loader.copy = function()
	loader.dll.KFToolsCopy(0)
end

loader.check_update = function()
	loader.dll.KFToolsCheckUpdate()
end

loader.changelog = function()
	loader.dll.KFToolsShowChangelog()
end

loader.unload = function()
end

return loader
