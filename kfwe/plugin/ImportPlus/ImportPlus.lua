local ffi = require "ffi"

local loader = {}

loader.load = function(path)
	local ok, dll = pcall(ffi.load, path:string())
	if not ok then
		log.error('ImportPlus: load failed: ' .. tostring(dll))
		return false
	end
	pcall(ffi.cdef, [[
		int ImportPlusInit(void);
		void ImportPlusSetMap(const char *path);
	]])
	loader.dll = dll
	local r = dll.ImportPlusInit()
	if r ~= 1 then
		log.error('ImportPlus: init failed, code ' .. tostring(r))
		return false
	end
	require('ev').on('virtual_mpq: open map', function(map)
		dll.ImportPlusSetMap(type(map) == 'string' and map or map:string())
	end)
	return true
end

loader.unload = function()
end

return loader
