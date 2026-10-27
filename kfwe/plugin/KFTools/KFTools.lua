local ffi = require "ffi"

local loader = {}

loader.load = function(path)
	local ok, dll = pcall(ffi.load, path:string())
	if not ok then
		log.error('KFTools: load failed: ' .. tostring(dll))
		return false
	end
	pcall(ffi.cdef, [[
		int KFToolsInit(void);
		int KFToolsCopy(int with_z);
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

loader.unload = function()
end

return loader
