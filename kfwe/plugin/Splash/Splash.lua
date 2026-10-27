local ffi = require "ffi"

local loader = {}

loader.load = function(path)
	local ok, dll = pcall(ffi.load, path:string())
	if not ok then
		log.error('Splash: load failed: ' .. tostring(dll))
		return false
	end
	pcall(ffi.cdef, 'int KFSplashStart(void);')
	loader.dll = dll
	return dll.KFSplashStart() == 1
end

loader.unload = function()
end

return loader
