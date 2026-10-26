local ffi = require "ffi"

local loader = {}

local function ensure_config_key()
	local tpp = global_config["ThirdPartyPlugin"]
	if tpp and tpp["EnableStormFastHeap"] then
		return
	end
	local ok, err = pcall(function()
		local path = fs.ydwe_path() / "bin" / "EverConfig.cfg"
		local f = io.open(path, "rb")
		if not f then return end
		local text = f:read("a")
		f:close()
		if text:find("EnableStormFastHeap", 1, true) then return end
		local nl = text:find("\r\n", 1, true) and "\r\n" or "\n"
		local new, n = text:gsub("(%[ThirdPartyPlugin%][^\r\n]*\r?\n)", "%1EnableStormFastHeap = 1" .. nl, 1)
		if n == 0 then
			new = text .. nl .. "[ThirdPartyPlugin]" .. nl .. "EnableStormFastHeap = 1" .. nl
		end
		f = io.open(path, "wb")
		if not f then return end
		f:write(new)
		f:close()
		log.info('StormFastHeap: added EnableStormFastHeap = 1 to EverConfig.cfg')
	end)
	if not ok then
		log.error('StormFastHeap: cannot update EverConfig.cfg: ' .. tostring(err))
	end
end

loader.load = function(path)
	ensure_config_key()
	local tpp = global_config["ThirdPartyPlugin"]
	if tpp and tpp["EnableStormFastHeap"] == "0" then
		log.info('StormFastHeap: disabled in settings')
		return false
	end
	local ok, dll = pcall(ffi.load, path:string())
	if not ok then
		log.error('StormFastHeap: load failed: ' .. tostring(dll))
		return false
	end
	pcall(ffi.cdef, [[
		int Initialize(void);
		int Stats(int which);
	]])
	loader.dll = dll
	local r = dll.Initialize()
	if r ~= 1 then
		log.error('StormFastHeap: not installed, code ' .. tostring(r))
		return false
	end
	log.info(('StormFastHeap: installed, code patches mask=%d'):format(dll.Stats(3)))
	return true
end

loader.unload = function()
	if loader.dll then
		log.info(('StormFastHeap: allocs=%d foreign_frees=%d'):format(loader.dll.Stats(0), loader.dll.Stats(1)))
		loader.dll.Stats(2)
		log.info(('StormFastHeap: var index hits=%d fallbacks=%d mismatches=%d builds=%d'):format(loader.dll.Stats(4), loader.dll.Stats(5), loader.dll.Stats(6), loader.dll.Stats(7)))
		log.info(('StormFastHeap: object editor index hits=%d misses=%d builds=%d orig=%d'):format(loader.dll.Stats(8), loader.dll.Stats(9), loader.dll.Stats(10), loader.dll.Stats(11)))
		log.info(('StormFastHeap: combo resizes skipped=%d'):format(loader.dll.Stats(12)))
		log.info(('StormFastHeap: CALLWNDPROC hooks replaced=%d messages delivered=%d'):format(loader.dll.Stats(13), loader.dll.Stats(14)))
		log.info(('StormFastHeap: fast subclass windows=%d, custom list cache hits=%d builds=%d'):format(loader.dll.Stats(15), loader.dll.Stats(16), loader.dll.Stats(17)))
	end
end

return loader