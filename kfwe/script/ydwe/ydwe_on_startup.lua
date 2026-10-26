require "w3x2lni.check_object"
require 'w3x2lni.open_map'
local uiloader = require "uiloader"
local stormlib = require 'ffi.stormlib'

-- 版本信息
ydwe_version = sys.version {}
war3_version = sys.war3_version {}

local function initialize_reg()
    local registry = require "bee.registry"
	local reg = registry.open [[HKEY_CURRENT_USER\Software\Blizzard Entertainment\WorldEdit]]
	-- Do not play user agreement
	reg["Has Been Run"] = { registry.REG_DWORD, 1 }
	if not reg["Visible UI Elements"] then
		-- Turn off brush table
		reg["Visible UI Elements"] = { registry.REG_DWORD, 0x2B }
	end
	if not reg["New Map On Startup"] then
		reg["New Map On Startup"] = { registry.REG_DWORD, 1 }
	end
	-- Some UI colors
	local reg = registry.open [[HKEY_CURRENT_USER\Software\Blizzard Entertainment\WorldEdit\Trigger Display Colors]]
	reg["TC_YDHIDE"] = { registry.REG_DWORD, 0xffAC6E10 }
	reg["TC_UNKNOWUI"] = { registry.REG_DWORD, 0xffff0000 }
	reg["TC_COMMENT"] = { registry.REG_DWORD, 0xff008000 }
	reg["TC_CUSTOM"] = { registry.REG_DWORD, 0xffA210A4 }
end

local function initialize_dark_mode()
	local registry = require "bee.registry"
	local disable = tonumber(global_config["ThirdPartyPlugin"]["EnableDarkMode"]) == 0
	log.debug("Dark Mode is", disable and "disabled" or "enabled")
	local colorsClassic = {
		['TT_PARTENABLED_UNUSED'] = 0, -- Trigger Editor - Triggers - Partially Enabled (Initially Off)
		['TT_ENABLED_UNUSED'] = 0, -- Trigger Editor - Triggers - Allow (Initially Off)
		['TC_DISABLED'] = 0, -- Trigger Editor - Features - Disable (Initially Off)
		['TT_DISABLED_UNUSED'] = 0, -- Trigger Editor - Triggers - Disable (Initially Off)
		['TT_COMMENT'] = 0, -- Trigger Editor - Triggers - Comments
		['OEF_FROMMAP'] = 0xffA210A4, -- Object Editor - Map Data

		['TC_COMMENT'] = 0xff008000, -- Comment
		['TC_CUSTOM'] = 0xffA210A4, -- Custom code
	}
	local colorsNew = {
		['TT_PARTENABLED_UNUSED'] = 0xFFC08080, -- Trigger Editor - Triggers - Partially Enabled (Initially Off)
		['TT_ENABLED_UNUSED'] = 0xFFC08080, -- Trigger Editor - Triggers - Allow (Initially Off)
		['TC_DISABLED'] = 0xFFC08080, -- Trigger Editor - Features - Disable (Initially Off)
		['TT_DISABLED_UNUSED'] = 0XFFFF80FF, -- Trigger Editor - Triggers - Disable (Initially Off)
		['TT_COMMENT'] = 0XFF50FF50, -- Trigger Editor - Triggers - Comments
		['OEF_FROMMAP'] = 0XFF50FF50, -- Object Editor - Map Data

		['TC_COMMENT'] = 0XFF50FF50, -- Comment
		['TC_CUSTOM'] = 0xff008000, -- Custom code
	}
	local reg = registry.open [[HKEY_CURRENT_USER\Software\Blizzard Entertainment\WorldEdit\Trigger Display Colors]]
	if disable then
		for key, value in pairs(colorsClassic) do
			reg[key] = {registry.REG_DWORD, value}
			--log.debug('Dark Mode set color ', key, tonumber(value))
		end
	else
		for key, value in pairs(colorsNew) do
			reg[key] = {registry.REG_DWORD, value}
		end
	end
end

-- Check whether there are any folders in the World of Warcraft directory that may cause conflicts
local function check_conflicting_ui()
	log.trace("check_conflicting_ui")
	if global_config["FeatureToggle"] and global_config["FeatureToggle"]["DisableUiDirCheck"] == "1" then return end
	local file_list = {"ui/loading-yuexie.blp", "ui/miscdata.txt", "ui/triggerdata.txt", "ui/triggerstrings.txt", "ui/worldeditdata.txt", "ui/worldeditstrings.txt"}
	local found = false

	for index, file in ipairs(file_list) do
		if fs.exists(fs.war3_path() / file) then
			found = true
			break
		end
	end

	if found then
		if gui.yesno_message(nil, string.format(LNG.DELETE_DANGER_DIR, 'UI'), 'KFWE') then
			for index, file in ipairs(file_list) do
				log.debug("remove file " .. (fs.war3_path() / file):string())
				pcall(fs.remove_all, fs.war3_path() / file)
			end
		end
	end
end

local function check_conflicting_units()
	log.trace("check_conflicting_units")
	if global_config["FeatureToggle"] and global_config["FeatureToggle"]["DisableUiDirCheck"] == "1" then return end
	local units_dir = fs.war3_path() / 'Units'
	local found = false

	if not fs.exists(units_dir) then
		return
	end
	for file in units_dir:list_directory() do
		if not fs.is_directory(file) then
			found = true
			break
		end
	end

	if found then
		if gui.yesno_message(nil, LNG.DELETE_DANGER_DIR, 'Units') then
			for file in units_dir:list_directory() do
				if not fs.is_directory(file) then
					log.debug("remove file " .. file:string())
					pcall(fs.remove_all, file)
				end
			end
		end
	end
end

-- 清除可能引起冲突的文件
local function clear_potential_conflicting()
	log.trace("clear_potential_conflicting")
	-- 需要清理的文件列表
	local file_list = {"MiscData.txt", "TriggerData.txt", "TriggerStrings.txt", "WorldEditData.txt", "WorldEditLayout.txt", "WorldEditStrings.txt", "war3map.j", "blizzard.j", "Scripts/blizzard.j", "common.j", "Scripts/common.j"}

	for index, file in ipairs(file_list) do
		if fs.exists(fs.war3_path() / file) then
			log.debug("remove file " .. (fs.war3_path() / file):string())
			pcall(fs.remove, fs.war3_path() / file)
		end
	end
end

-- 从魔兽中得到是否是1.24。通过搜索common.j有没有导出StringHash
-- 返回值：两个，魔兽版本，错误信息。如果没有错误，错误信息为nil
local function get_war3_version_from_script()
	local err = "Cannot extract file from warcraft"
	local common_j_path = fs.ydwe_path() / "logs" / "common.j"
	local mpq = stormlib.open(fs.war3_path() / 'War3Patch.mpq', true)
	if mpq then
		if mpq:has_file("common.j") then
			mpq:extract("common.j", common_j_path)
		elseif mpq:has_file("scripts\\common.j") then
			mpq:extract("scripts\\common.j", common_j_path)
		else
			return war3_version, err
		end
		mpq:close()

		local s, e = io.load(common_j_path)
		if not s then
			return war3_version, e
		end

		if s:find("StringHash") then
			return war3_version:new(), nil
		else
			return war3_version:old(), nil
		end
	end

	return war3_version, err
end

-- Detect the version of wc3
local function check_war3_version()
	log.trace("check_war3_version")
	local ydwe_path = fs.__ydwe_path:string()
	local ydwePath = fs.ydwe_path()
	local mapsave_number = tonumber(global_config["MapSave"]["Option"])
	local mapHelper = (tonumber(global_config["ThirdPartyPlugin"]["EnableMapHelper"]) or 0)
	local war3path = fs.war3_path()
	--return tbl['Enable']['yd_jass_api.dll'] ~= '0' (PK Так было раньше)
    if war3_version.major ~= 1 or war3_version.minor < 20 or war3_version.minor > 27 then
        log.error("Unsupported War3", war3_version.major, war3_version.minor)
        gui.error_message(nil, LNG.UNSUPPORTED_WAR3_VERSION, war3_version.major, war3_version.minor)
        os.exit(true, -1)
    end
	--os.execute('IF EXIST'.."C:\\Users\\user\\Downloads\\WC3Tools\\Warcraft III 1.26")
	--log.warn("mapHelper: " .. tostring(fs.war3_path()))

	--if fs.exists(path / 'jass' / 'Base' /'PK_Init.j') then
	-- Сначала удаляем в любом случае
	--[[if fs.exists(war3path / 'MapHelper.asi') then
		fs.remove(war3path / 'MapHelper.asi')
	end
	if fs.exists(war3path / 'MapHelper.json') then
		fs.remove(war3path / 'MapHelper.json')
	end]]
	-- Теперь создаем если включен
	if tostring(mapHelper) == "1" then
		log.info("Map Helper Enabled")
		fs.copy_file(ydwePath / 'plugin' / 'tesh' / 'data' / 'MapHelper.asi', war3path / 'MapHelper.asi', true)
		fs.copy_file(ydwePath / 'plugin' / 'tesh' / 'data' / 'MapHelper.json', war3path / 'MapHelper.json', true)
	else
		log.info("Map Helper Disabled")
	end
	-- Check for inconsistencies between game.dll and war3patch.mpq caused by "version converter" etc.
	if mapsave_number == 0 then
		-- Detect the version represented by the script file contained in wc3
		local script_war3_version, e = get_war3_version_from_script()

		if e then
			log.warn("Cannot get warcraft 3 version from script: " .. e)
		else
			-- If the two are inconsistent, prompt
			if war3_version:is_new() ~= script_war3_version:is_new() then
				gui.error_message(nil, LNG.ERROR_WAR3_VERSION)
			end
		end
	elseif mapsave_number == 3 then -- Reforged (PK Edit)
		fs.copy_file(ydwePath / 'plugin' / 'tesh' / 'data' / 'Ref' / 'tesh_keywords.db', ydwePath / 'plugin' / 'tesh' / 'data' / 'tesh_keywords.db', true)
		fs.copy_file(ydwePath / 'plugin' / 'tesh' / 'data' / 'Ref' / 'WorldEditStrings.txt', ydwePath / 'share' / 'en-US' / 'mpq' / 'UI' / 'WorldEditStrings.txt', true)
	elseif mapsave_number == 1 then -- 1.26 (PK Edit)
		fs.copy_file(ydwePath / 'plugin' / 'tesh' / 'data' / '126' / 'tesh_keywords.db', ydwePath / 'plugin' / 'tesh' / 'data' / 'tesh_keywords.db', true)
		fs.copy_file(ydwePath / 'plugin' / 'tesh' / 'data' / '126' / 'WorldEditStrings.txt', ydwePath / 'share' / 'en-US' / 'mpq' / 'UI' / 'WorldEditStrings.txt', true)
	elseif mapsave_number == 2 then -- Japi/Dzapi (PK Edit)
		fs.copy_file(ydwePath / 'plugin' / 'tesh' / 'data' / 'Japi' / 'tesh_keywords.db', ydwePath / 'plugin' / 'tesh' / 'data' / 'tesh_keywords.db', true)
		fs.copy_file(ydwePath / 'plugin' / 'tesh' / 'data' / 'Japi' / 'WorldEditStrings.txt', ydwePath / 'share' / 'en-US' / 'mpq' / 'UI' / 'WorldEditStrings.txt', true)
	end
end

-- 显示制作者和感谢信息
function show_credit()
	we.message_show("    ------------------------------------------------------------")
	we.message_show("           Добро пожаловать в KFWE, лучший редактор карт!           ")
	we.message_show("    ------------------------------------------------------------")
	we.message_show("    Автор KFWE:           Naxefir")
	we.message_show("    Автор оригинала PK:   Enemy1PK (discord: enemy1pk)")
	we.message_show("    Создатели YDWE:       actboy168, sumneko")
	we.message_show("    Последняя версия PK:  www.hiveworkshop.com/threads/ydwe-pk-world-editor.351329")
	we.message_show("    Дискорд сервер KFWE:  https://discord.gg/2Chc8rbXXD")
	we.message_show("    *** Особая благодарность ***")
	we.message_show("    Dracol1ch, Prometheus, Super.Skill, sotzaii_shuen")
	we.message_show("    ...")
	we.message_show("    И всем пользователям & помощникам, включая")
	we.message_show("    ВАС")
end

local function initialize_font()
	if global_config.Font.FontEnable ~= "1" then
		return
	end

	local name = global_config.Font.FontName
	local size = tonumber(global_config.Font.FontSize)
	if name == '' then name = 'system' end
	if size == nil then size = 12 end

	local font = (require 'ffi.font')(name, size)
	if font ~= 0 then
		log.info('Font: ', name, size)
		we.set_font(font)
	end
end

-- Download the MPQ from the main program's mpq directory
-- mpqname - MPQ file name
-- Return value: MPQ handle
local function load_virtual_mpq(mpqname, priority)
	local mpq = fs.ydwe_path() / "share" / mpqname
	if fs.exists(mpq) then
		virtual_mpq.open_path(mpq, priority)
	end
end

-- 本函数在编辑器启动时调用，可以在本函数中载入一些插件
-- event_data - 事件参数
-- 	暂无内容
-- 返回值：返回非负数表示成功，负数表示失败
function event.EVENT_WE_START(event_data)
	log.debug("********************* on startup start *********************")

	-- read version
	ydwe_version = sys.version { file = fs.ydwe_path() / "bin" / "ydbase.dll" }
	war3_version = sys.war3_version { file = fs.war3_path() / "game.dll" }

	log.debug("ydwe version " .. tostring(ydwe_version))
	log.debug("war3 version " .. tostring(war3_version))

	local suc, gitlog = pcall(require, 'gitlog')
	if suc then
		log.debug("commit: " .. gitlog.commit)
		log.debug("data: " .. gitlog.date)
	end

	-- 刷新配置数据
	global_config_reload()

	-- 检测UI和Units目录
	check_conflicting_ui()
	check_conflicting_units()

	-- 检查魔兽目录下是否有可能引起冲突的文件夹
	clear_potential_conflicting()

	-- 检测魔兽的版本
	check_war3_version()

    -- Load Patch MPQ
    load_virtual_mpq("mpq/war3", 14)
    local lang = (require "i18n").get_language()
    if lang ~= 'zh-CN' then
        load_virtual_mpq(lang .. "/mpq", 14)
    end
    load_virtual_mpq("zh-CN/mpq", 14)

	-- 加载插件
	plugin:load_all()

	-- 初始化UI加载器
	uiloader()

	initialize_reg()
	initialize_dark_mode()
	initialize_font()

	-- 显示感谢信息
	show_credit()

	log.debug("********************* on startup end *********************")

	return 0
end

-- 本函数在编辑器关闭时调用，可以在本函数中作一些清理工作
-- event_data - 事件参数。table类型，暂时没有内容。
-- 返回值：一定要返回0
function event.EVENT_WE_EXIT(event_data)
	log.debug("********************* on exit start *********************")

	plugin:unload_all()

	log.debug("********************* on exit end *********************")
	local war3path = fs.war3_path()
	if fs.exists(war3path / 'MapHelper.asi') then
		fs.remove(war3path / 'MapHelper.asi')
	end
	if fs.exists(war3path / 'MapHelper.json') then
		fs.remove(war3path / 'MapHelper.json')
	end
	return 0
end

-- 在WE载入MSS引擎解码器时调用，过滤载入的dll
-- event_data - 事件参数，table，包含以下值
--	library_name - 解码器路径，字符串
-- 返回非负数表示允许载入，负数表示不允许。无特殊情况一般返回0

local function is_game_mod(path)
	local p = path:lower():gsub('/', '\\')
	if not p:match('%.mix$') or p:find('\\redist\\', 1, true) then
		return false
	end
	local f = io.open(fs.path(path), 'rb')
	if not f then
		return false
	end
	local data = f:read('a')
	f:close()
	if not data then
		return false
	end
	data = data:lower()
	return data:find('game.dll', 1, true) ~= nil or data:find('g\0a\0m\0e\0.\0d\0l\0l\0', 1, true) ~= nil
end

function event.EVENT_MSS_LOAD(event_data)
	local ok, skip = pcall(is_game_mod, event_data.library_name)
	if not ok then
		log.error("MSS filter: " .. tostring(skip))
	elseif skip then
		log.info("Skip game mod " .. event_data.library_name)
		return -1
	end
	log.debug("Loading provider " .. event_data.library_name)
	return 0
end

local event = require 'ev'
virtual_mpq.event(function(name, ...)
    event.emit('virtual_mpq: ' .. name, ...)
end)