import re, os
K = r'C:\Games\Warcraft 3\KFWE'

def edit(rel, fn):
    p = os.path.join(K, rel)
    raw = open(p, 'rb').read()
    bom = raw.startswith(b'\xef\xbb\xbf')
    s = raw.decode('utf-8-sig')
    t = fn(s)
    open(p, 'wb').write((b'\xef\xbb\xbf' if bom else b'') + t.encode('utf-8'))
    print(rel, 'изменено' if t != s else 'без изменений')

def brand(x):
    return re.sub(r'YDWE(?![A-Za-z_])', 'KFWE', x.replace('YDWE PK', 'KFWE'))

def menu(s):
    s = s.replace('\tmenu:add(LNG.MENU_CREDITS, open_thanks)\n', '')
    return s.replace('https://discord.gg/sVkB27JwG8', 'https://discord.gg/2Chc8rbXXD')

def startup(s):
    s = s.replace('"    Дискорд сервер PK:    https://discord.gg/JREJ6p9TK2"', '"    Дискорд сервер KFWE:  https://discord.gg/2Chc8rbXXD"')
    return s.replace("LNG.DELETE_DANGER_DIR, 'UI'), 'YDWE')", "LNG.DELETE_DANGER_DIR, 'UI'), 'KFWE')")

def lng(s):
    out = '\n'.join(l if l.lstrip().startswith('[') else brand(l) for l in s.split('\n'))
    return out.replace('Открыть KFWE Discord сервер', 'Открыть Discord сервер KFWE')

def xml(s):
    return re.sub(r'((?:text|tooltip)=")([^"]*)(")', lambda m: m.group(1) + brand(m.group(2)) + m.group(3), s)

edit(r'script\ydwe\ydwe_on_menu.lua', menu)
edit(r'script\ydwe\ydwe_on_startup.lua', startup)
edit(r'script\common\ffi\gui.lua', lambda s: s.replace("fmt:format(...), 'YDWE',", "fmt:format(...), 'KFWE',"))
for L in ('en-US', 'zh-CN'):
    edit(os.path.join('share', 'locale', L, 'script.lng'), lng)
    edit(os.path.join('share', 'skins', L, 'DialogConfig.xml'), xml)
