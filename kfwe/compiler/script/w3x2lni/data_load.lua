fs = require 'bee.filesystem'
local i18n = require 'i18n'

local function ydwePath(support_dev)
    local ydwe = fs.dll_path():parent_path():parent_path()
    if support_dev then
        local ydwedev = ydwe:parent_path():parent_path():parent_path()
        if fs.exists(ydwedev / "build.root") then
            return ydwedev / "Component"
        end
    end
    return ydwe
end

local ydwe = ydwePath(true)
local w2lPath = ydwe / 'plugin' / 'w3x2lni'
if not kflang then
    i18n.initialize(ydwe / 'share' / 'locale')
end
local currentLanguage = i18n.get_language()

return function (w2l, filename)
    if filename:sub(1, 3) == 'ui\\' then
        return io.load(ydwe / filename)
    end
    if filename:sub(1, 9) == 'prebuilt\\' then
        return io.load(w2lPath / 'data' / w2l.setting.data / filename)
    end
    local buf = io.load(ydwe / 'share' / currentLanguage / filename)
    if not buf and currentLanguage ~= 'zh-CN' then
        buf = io.load(ydwe / 'share' / 'zh-CN' / filename)
    end
    return buf
end
