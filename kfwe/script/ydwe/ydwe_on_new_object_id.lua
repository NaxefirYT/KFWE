fs = require 'bee.filesystem'
require "localization"
local ffi = require "ffi"
local root = fs.ydwe_devpath()
local ydpath = fs.ydwe_path()
local w3x2lni = require 'compiler.w3x2lni.init'

local defaults

local function get_default(type)
	if not defaults then
		defaults = w3x2lni():get_default()
	end
	return defaults[type]
end

local function from_objectid (id)
	return string.pack('<I4', id)
end

local function to_objectid (str)
	return string.unpack('<I4', str)
end

local object = {}

local object_type = {
	[0] = 'unit',
	[1] = 'item',
	[2] = 'destructable',
	[3] = 'doodad',
	[4] = 'ability',
	[5] = 'buff',
	[6] = 'upgrade',
}

function object:original_has (this_, id_string_)
	local this_ptr_ = ffi.cast('uint32_t*', this_)
	local ptr  = this_ptr_[7] + 4
	local size = this_ptr_[6]

	for i = 0, size-2 do
		local id = ffi.cast('uint32_t*', ptr)[0]
		if from_objectid(id) == id_string_ then
			return true
		end
		ptr = ptr + 24
	end

	return false
end

function object:custom_has (type_, id_string_)
	if not object_type[type_] then
		return false
	end
	if not get_default(object_type[type_])[id_string_] then
		return false
	end
	return true
end


local type_names = {
	[0] = 'Юнит',
	[1] = 'Предмет',
	[2] = 'Разрушаемый',
	[3] = 'Декорация',
	[4] = 'Способность',
	[5] = 'Эффект',
	[6] = 'Улучшение',
}

function event.EVENT_NEW_OBJECT_ID(event_data)
	log.debug("**************** on new object id start ****************")	
		
	local object_type = event_data.object_type
	local default_id = event_data.default_id
	global_config_reload()

	if tonumber(global_config["FeatureToggle"]["EnableManualNewId"]) == 0 then
		log.trace("Disable.")
		return default_id
	end
	local dialog = require 'gui_newobjectid'
	local res = dialog(from_objectid(default_id), type_names[object_type] or 'Объект', function(r)
		if #r ~= 4 then
			return 'Нужно ровно 4 символа'
		end
		if not r:match('^[%w_]+$') then
			return 'Только латиница, цифры и _'
		end
		if object:custom_has(object_type, r) then
			return r .. ' занят стандартным объектом'
		end
		if object:original_has(event_data.class, r) then
			return r .. ' уже есть на карте'
		end
	end)
	return to_objectid(res)
end
