-- Main window handle
local main_window_handle = nil

-- Start configuration dialog
local function launch_config()
	pcall(kflang.sync)
	sys.spawn {
		(fs.ydwe_path() / "bin" / "YDWEConfig.exe"):string(),
		cwd = fs.ydwe_path(),
	}
end

-- Start wc3
local function launch_warcraft3()
	local mapsave_number = tonumber(global_config["MapSave"]["Option"])
	sys.spawn { -- теперь всегда запускаем не рефорж ибо в таком запуске он не имеет смысла
		(fs.ydwe_path() / "ydwe.exe"):string(),
		"-war3",
		cwd = fs.ydwe_path(),
	}
end

local function close_warcraft3()
	local ffi = require 'ffi'
	pcall(ffi.cdef, [[
		void* OpenEventA(unsigned long dwDesiredAccess, int bInheritHandle, const char* lpName);
		int SetEvent(void* hEvent);
		int ResetEvent(void* hEvent);
		int CloseHandle(void* hObject);
	]])
	local ev = ffi.C.OpenEventA(0x0002, 0, 'KFWE_close_war3')
	if ev == nil then return end
	ffi.C.SetEvent(ev)
	require 'ffi.sleep' (500)
	ffi.C.ResetEvent(ev)
	ffi.C.CloseHandle(ev)
end

-- 打开官网
local function open_offical_site()
	os.execute('explorer "https://discord.gg/4EHuGnhyGe"')
end

local function open_thanks()
    local thanks = require 'gui_thanks'
    thanks()
end

-- 初始化菜单
-- event_data - 事件参数，table，包含以下值
--	main_window_handle - 主窗口的handle
--	main_menu_handle - 主菜单的handle
-- 返回值：一律返回0
function event.EVENT_INIT_MENU(event_data)
	log.debug("********************* on menuinit start *********************")

	local menu = gui.menu(event_data.main_menu_handle, '&KFWE')
	menu:add(LNG.MENU_CONIFG, launch_config)
	menu:separator()
	menu:add(LNG.MENU_LAUNCH_WAR3, launch_warcraft3)
	menu:add(TR('Закрыть все окна WC3'), close_warcraft3)
	require('playas').attach(menu, gui.on_command)
	local tools = plugin.loaders.KFTools
	if tools then
		menu:separator()
		menu:add(TR('Копировать координаты\tCtrl+Shift+X'), tools.copy)
		menu:separator()
		menu:add(TR('Проверить обновления'), tools.check_update)
		menu:add(TR('Что нового'), tools.changelog)
	else
		menu:separator()
	end
	menu:add(LNG.MENU_OPEN_OFFICIAL_SITE, open_offical_site)

	main_window_handle = event_data.main_window_handle

	log.debug("********************* on menuinit end *********************")

	return 0
end
