local event = require 'ev'
local ydwe = fs.ydwe_devpath()
local lang = (require "i18n").get_language()
local path

event.on('virtual_mpq: open path', function(name)
    log.info('OpenPathAsArchive', name)
    path = name
end)
event.on('virtual_mpq: close path', function(name)
    log.info('ClosePathAsArchive', name)
    path = nil
end)

local chain = { lang }
if lang ~= 'ru' then
    chain[#chain + 1] = 'ru'
end
if lang ~= 'zh-CN' then
    chain[#chain + 1] = 'zh-CN'
end

local mt = {}

function mt:load(filename)
    if filename:sub(1, 5) == 'ydwe\\' then
        for _, l in ipairs(chain) do
            local res = io.load(ydwe / 'share' / l / filename)
            if res then
                return res
            end
        end
        return
    end
    for _, l in ipairs(chain) do
        if path then
            local res = io.load(ydwe / 'share' / l / 'mpq' / path / filename)
            if res then
                return res
            end
        end
        local res = io.load(ydwe / 'share' / l / 'mpq' / filename)
        if res then
            return res
        end
    end
    for _, l in ipairs(chain) do
        local res = io.load(ydwe / 'share' / 'internalRes' / l / 'mpq' / filename)
        if res then
            return res
        end
    end
    return io.load(ydwe / 'share' / 'internalRes' / 'en-US' / 'mpq' / filename)
end

return mt
