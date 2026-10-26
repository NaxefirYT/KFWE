local ffi = require "ffi"

local loader = {}

loader.load = function(path)
	local ok, dll = pcall(ffi.load, path:string())
	if not ok then
		log.error('ModelSearch: load failed: ' .. tostring(dll))
		return false
	end
	pcall(ffi.cdef, 'int ModelSearchInit(void);')
	loader.dll = dll
	local r = dll.ModelSearchInit()
	if r ~= 1 then
		log.error('ModelSearch: init failed, code ' .. tostring(r))
		return false
	end
	return true
end

loader.unload = function()
end

return loader
