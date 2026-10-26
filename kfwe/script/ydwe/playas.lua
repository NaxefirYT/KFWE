local ffi = require 'ffi'
ffi.cdef[[
	unsigned int CreatePopupMenu();
	int GetMenuItemCount(unsigned int hMenu);
	int DeleteMenu(unsigned int hMenu, unsigned int uPosition, unsigned int uFlags);
]]
local uni = require 'ffi.unicode'
local ev = require 'ev'

local COLORS = { 'Красный', 'Синий', 'Бирюзовый', 'Фиолетовый', 'Жёлтый', 'Оранжевый',
                 'Зелёный', 'Розовый', 'Серый', 'Голубой', 'Тёмно-зелёный', 'Коричневый' }
local MF_STRING, MF_POPUP, MF_CHECKED, MF_SEPARATOR, MF_GRAYED, MF_BYPOSITION = 0, 0x10, 0x8, 0x800, 0x1, 0x400
local ID_BASE = 48700

local M = {}
local current_map
ev.on('打开地图', function (map_path) current_map = fs.path(map_path) end)

local function cfg_path() return fs.ydwe_path() / 'bin' / 'playas.cfg' end

local function load_all()
	local t = {}
	local buf = io.load(cfg_path())
	if buf then
		for path, n in buf:gmatch('([^\r\n]+)|(%d+)') do t[path] = tonumber(n) end
	end
	return t
end

local function key(map_path) return map_path and map_path:string():lower() or nil end

function M.get(map_path)
	local k = key(map_path or current_map)
	return k and load_all()[k] or nil
end

function M.set(map_path, n)
	local k = key(map_path); if not k then return end
	local t = load_all(); t[k] = n
	local lines = {}
	for path, v in pairs(t) do lines[#lines + 1] = path .. '|' .. v end
	io.save(cfg_path(), table.concat(lines, '\r\n'))
end

local function map_players(map_path)
	local ok, list = pcall(function ()
		local stormlib = require 'ffi.stormlib'
		local map = stormlib.open(map_path, true)
		if not map then return nil end
		local w3i = map:load_file 'war3map.w3i'
		map:close()
		local w2l = (require 'compiler.w3x2lni.init')()
		local info = w2l:frontend_w3i(w3i)
		local r = {}
		for i = 1, info.PLAYER.PLAYER_COUNT do
			local p = info['PLAYER' .. i]
			if p.TYPE == 1 then r[#r + 1] = p.PLAYER end
		end
		table.sort(r)
		return r
	end)
	if ok and list then return list end
	local all = {}
	for i = 0, 11 do all[#all + 1] = i end
	return all
end

local function append(h, flags, id, text)
	ffi.C.AppendMenuW(h, flags, id, (uni.u2w(text)))
end

function M.attach(parent_menu, on_command)
	local sub = ffi.C.CreatePopupMenu()
	append(parent_menu.handle, MF_STRING | MF_POPUP, sub, 'Тест за игрока')
	local function rebuild()
		while ffi.C.GetMenuItemCount(sub) > 0 do ffi.C.DeleteMenu(sub, 0, MF_BYPOSITION) end
		if not current_map then
			append(sub, MF_STRING | MF_GRAYED, ID_BASE, 'Сначала откройте карту')
			return
		end
		local cur = M.get(current_map)
		if cur == -1 then cur = nil end
		append(sub, MF_STRING | (cur == nil and MF_CHECKED or 0), ID_BASE, 'Как в карте (красный)')
		append(sub, MF_SEPARATOR, 0, '')
		for _, n in ipairs(map_players(current_map)) do
			append(sub, MF_STRING | (cur == n and MF_CHECKED or 0), ID_BASE + 1 + n, ('%s (игрок %d)'):format(COLORS[n + 1] or '?', n + 1))
		end
	end
	for i = 0, 12 do
		on_command(ID_BASE + i, function ()
			if current_map then M.set(current_map, i - 1); rebuild() end
		end)
	end
	ev.on('打开地图', function () pcall(rebuild) end)
	rebuild()
end

function M.for_test(map_path)
	local n = M.get(map_path)
	if n == nil or n < 0 then return nil end
	return n
end

return M
