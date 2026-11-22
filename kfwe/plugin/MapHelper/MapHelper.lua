local ffi = require "ffi"

local loader = {}

loader.load = function(path)
	local tp = global_config["ThirdPartyPlugin"]
	if not tp or tostring(tp["EnableMapHelper"]) ~= "1" then
		log.info('MapHelper disabled')
		return false
	end
	loader.path = path:string()
	return true
end

loader.attach = function()
	if loader.dll or not loader.path then
		return
	end
	local ok, dll = pcall(ffi.load, loader.path)
	if not ok then
		log.error('MapHelper: load failed: ' .. tostring(dll))
		loader.path = nil
		return
	end
	loader.dll = dll
	log.info('MapHelper attached')
end

loader.unload = function()
end

return loader
