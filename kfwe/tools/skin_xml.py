import re, os
K = r'C:\Games\Warcraft 3\KFWE\share'
SRC, DST = os.path.join(K, 'skins.bak-design'), os.path.join(K, 'skins')

BG, SIDE, TEXT, TEXT2, DIM, MUTED, ACC = '#FF1E1F24', '#FF17181C', '#FFE4E6EB', '#FFC3C7D0', '#FF5C616C', '#FF8B90A0', '#FF7C5CFF'

def img(name, extra=''):
    return "file='Common/kf/%s'%s" % (name, extra)

STYLES = f'''<?xml version="1.0" encoding="UTF-8"?>
<!-- KFWE: тёмное оформление окна настроек (картинки в Common/kf) -->
<Styles>
  <Include source="scrollbar.xml" />
  <Class name="Button" font="5" textcolor="{TEXT}" hottextcolor="#FFFFFFFF" focusedtextcolor="{TEXT}" pushedtextcolor="{TEXT2}" disabledtextcolor="{DIM}"
    normalimage   = "{img('btn.png', " corner='9,9,9,9'")}"
    hotimage      = "{img('btn_hot.png', " corner='9,9,9,9'")}"
    pushedimage   = "{img('btn_push.png', " corner='9,9,9,9'")}"
    disabledimage = "{img('btn.png', " corner='9,9,9,9' fade='120'")}"
  />
  <Class name="NormalRadioButton" font="2" textcolor="{TEXT2}" hottextcolor="#FFFFFFFF" selectedtextcolor="#FFFFFFFF" disabledtextcolor="{DIM}" textpadding="28,0,0,0" align="left"
    normalimage           = "{img('radio_off.png', " dest='0,4,18,22'")}"
    hotimage              = "{img('radio_off_hot.png', " dest='0,4,18,22'")}"
    pushedimage           = "{img('radio_off_hot.png', " dest='0,4,18,22'")}"
    disabledimage         = "{img('radio_off.png', " dest='0,4,18,22' fade='110'")}"
    selectedimage         = "{img('radio_on.png', " dest='0,4,18,22'")}"
    selectedhotimage      = "{img('radio_on_hot.png', " dest='0,4,18,22'")}"
    selectedpushedimage   = "{img('radio_on_hot.png', " dest='0,4,18,22'")}"
    selecteddisabledimage = "{img('radio_on.png', " dest='0,4,18,22' fade='110'")}"
  />
  <Class name="CheckBox" font="2" textcolor="{TEXT2}" hottextcolor="#FFFFFFFF" selectedtextcolor="{TEXT}" disabledtextcolor="{DIM}" textpadding="46,0,0,0" align="left"
    normalimage           = "{img('switch_off.png', " dest='0,3,36,23'")}"
    hotimage              = "{img('switch_off_hot.png', " dest='0,3,36,23'")}"
    pushedimage           = "{img('switch_off_hot.png', " dest='0,3,36,23'")}"
    disabledimage         = "{img('switch_off.png', " dest='0,3,36,23' fade='110'")}"
    selectedimage         = "{img('switch_on.png', " dest='0,3,36,23'")}"
    selectedhotimage      = "{img('switch_on_hot.png', " dest='0,3,36,23'")}"
    selectedpushedimage   = "{img('switch_on_hot.png', " dest='0,3,36,23'")}"
    selecteddisabledimage = "{img('switch_on.png', " dest='0,3,36,23' fade='110'")}"
  />
  <Class name="Label" height="20" textpadding="2,-4,2,-4" textcolor="{TEXT2}" disabledtextcolor="{DIM}" font="0" align="wrap" />
</Styles>
'''

def sb(kind):
    t = img('thumb.png', " corner='3,6,3,6'"); th = img('thumb_hot.png', " corner='3,6,3,6'"); tp = img('thumb_push.png', " corner='3,6,3,6'")
    e = img('empty.png')
    return f'''  <Class name="{kind}" width="8" height="8" showbutton1="false" showbutton2="false"
    button1normalimage="{e}" button2normalimage="{e}"
    thumbnormalimage="{t}" thumbhotimage="{th}" thumbpushedimage="{tp}" thumbdisabledimage="{t}"
    railnormalimage="{e}" bknormalimage="{e}" bkhotimage="{e}" bkpushedimage="{e}" bkdisabledimage="{e}"
  />
'''
SCROLL = '<?xml version="1.0" encoding="UTF-8"?>\n<ScrollbarConfig>\n' + sb('VScrollBar') + sb('HScrollBar') + '</ScrollbarConfig>\n'

FONTS = '''  <Font name="Segoe UI" size="16" default="true" />
  <Font name="Segoe UI" size="16" />
  <Font name="Segoe UI" size="15" />
  <Font name="Segoe UI Semibold" size="12" />
  <Font name="Segoe UI Semibold" size="21" />
  <Font name="Segoe UI Semibold" size="15" />
'''

CLASSES = f'''  <Class name="Title" bkcolor="{BG}" />
  <Class name="LabelTitle" font="3" textcolor="{MUTED}" height="28" textpadding="2,0,2,4" align="left"
    bkimage="{img('line.png', " dest='0,27,2000,28'")}" />
  <Class name="ListLabelElement" />
  <Class name="Background" bkcolor="{BG}" />
  <Class name="TabSelect" font="5" align="left" textpadding="18,0,0,0"
    textcolor="#FFA4A9B6" selectedtextcolor="#FFFFFFFF" hottextcolor="{TEXT}" focusedtextcolor="#FFA4A9B6" pushedtextcolor="{TEXT}" selectedpushedtextcolor="#FFFFFFFF"
    hotimage            = "{img('tab_hot.png', " corner='10,10,10,10'")}"
    pushedimage         = "{img('tab_hot.png', " corner='10,10,10,10'")}"
    selectedimage       = "{img('tab_sel.png', " corner='10,10,10,10'")}"
    selectedhotimage    = "{img('tab_sel.png', " corner='10,10,10,10'")}"
    selectedpushedimage = "{img('tab_sel.png', " corner='10,10,10,10'")}"
  />
'''

C8 = " corner='8,8,8,8'"; C9 = " corner='9,9,9,9'"
FLD, FLDH = img('field.png', C8), img('field_hot.png', C8)
PRI, PRIH, PRIP = img('btn_pri.png', C9), img('btn_pri_hot.png', C9), img('btn_pri_push.png', C9)
FIELD = (f'normalimage="{FLD}" hotimage="{FLDH}" '
         f'pushedimage="{FLDH}" textpadding="10,0,10,0" '
         f"itemtextcolor=\"{TEXT}\" itembkcolor=\"#FF2B2D33\" itemhottextcolor=\"#FFFFFFFF\" itemhotbkcolor=\"#FF363944\" "
         f"itemselectedtextcolor=\"#FFFFFFFF\" itemselectedbkcolor=\"{ACC}\"")

def widen(tag, extra, cap):
    def f(m):
        el = m.group(0)
        return re.sub(r'width="(\d+)"', lambda w: 'width="%d"' % min(cap, int(w.group(1)) + extra), el, count=1)
    return f

def dialog(s):
    s = re.sub(r'<Window [^>]*>', '<Window size="720,660" caption="0,0,0,52" roundcorner="12,12" >', s, count=1)
    a = s.index('  <Font '); b = s.index('  <Include source="styles.xml" />')
    s = s[:a] + FONTS + s[b:]
    a = s.index('  <Class name="Title"'); b = s.index('  <VerticalLayout class="Title"')
    s = s[:a] + CLASSES + '\n' + s[b:]
    m = re.search(r'<Label text="([^"]*)" height="36" align="center" />', s)
    title = 'Настройки'
    head = (f'<HorizontalLayout height="52" bkcolor="{SIDE}" inset="24,0,24,0" childpadding="10" >\n'
            f'      <Label text="KFWE" font="4" textcolor="{ACC}" width="64" height="52" align="left" textpadding="0,0,0,0" />\n'
            f'      <Label text="{title}" font="0" textcolor="{MUTED}" height="52" align="left" textpadding="0,2,0,0" />\n'
            f'    </HorizontalLayout>')
    s = s[:m.start()] + head + s[m.end():]
    s = re.sub(r'<VerticalLayout bkcolor="#FFEAECEC" width="225" inset="[^"]*">',
               f'<VerticalLayout bkcolor="{SIDE}" width="200" inset="12,10,12,10">', s, count=1)
    s = re.sub(r'(<RadioButton name="tab_\d"[^>]*?) textpadding="[^"]*" borderround="[^"]*"', r'\1', s)
    s = re.sub(r'(<RadioButton name="tab_\d"[^>]*?)width="\d+" height="\d+"', r'\1width="176" height="38"', s)
    s = s.replace('<Control height="1" />', '<Control height="4" />')
    s = re.sub(r'<VerticalLayout class="Background" inset="[23],(?:18|24),2,(?:18|24)"', '<VerticalLayout class="Background" inset="24,14,20,14"', s)
    s = re.sub(r'(<VerticalLayout height=")(\d+)("\s*>\s*<Label text="[^"]*"[^>]*class="LabelTitle")', lambda m: m.group(1) + str(int(m.group(2)) + 30) + m.group(3), s)
    s = re.sub(r'(<Label text=")([^"]*)("[^>]*class="LabelTitle")', lambda m: m.group(1) + m.group(2).upper() + m.group(3), s)
    s = s.replace('bkcolor="#FFEBEBEB" itembkcolor="#FFFAFCFC"', FIELD)
    s = s.replace('<Label text="" bkcolor="#FFEBEBEB" textcolor="#FF000000" font="2" name="Warcraft3Directory" />',
                  f'<Label text="" bkimage="{FLD}" textcolor="{TEXT}" font="2" textpadding="10,0,10,0" align="left" height="30" name="Warcraft3Directory" />')
    s = s.replace('<Button text="Путь" width="40" height="20" font="2" name="choose_war3_dir" />',
                  '<Button text="Путь" width="64" height="30" font="2" name="choose_war3_dir" />')
    s = s.replace('name="FontPreview" text="AaBbCc 0123456789 test" textcolor="#FF000000"',
                  f'name="FontPreview" text="AaBbCc 0123456789 test" textcolor="{TEXT}"')
    s = re.sub(r'<CheckBox [^>]*>', widen('CheckBox', 44, 440), s)
    s = re.sub(r'<RadioButton class="NormalRadioButton"[^>]*>', widen('Radio', 12, 440), s)
    s = re.sub(r'<HorizontalLayout class="Background" height="20"\s*>', f'<HorizontalLayout bkcolor="{SIDE}" height="30" inset="24,8,24,0" >', s)
    s = re.sub(r'(<Label text="[^"]*\(\*\)[^"]*") textcolor="#FF000000" font="2" />', r'\1 textcolor="#FF7D8290" font="2" align="left" />', s)
    s = re.sub(r'<HorizontalLayout class="Background" height="40"\s*childpadding="20" >\s*<Control bkcolor="#000000" width="90" inset="0,0,0,0" />',
               f'<HorizontalLayout bkcolor="{SIDE}" height="60" childpadding="10" inset="24,10,24,16" >\n      <Control />', s)
    s = re.sub(r'<Button text="([^"]*)"\s+width="\d+"\s+height="30"\s+name="(reset|cancel)" />', r'<Button text="\1" width="104" height="34" name="\2" />', s)
    s = re.sub(r'<Button text="([^"]*)"\s+width="\d+"\s+height="30"\s+name="ok" />',
               lambda m: (f'<Button text="{m.group(1)}" width="104" height="34" name="ok" textcolor="#FFFFFFFF" hottextcolor="#FFFFFFFF" pushedtextcolor="#FFFFFFFF" '
                          f'normalimage="{PRI}" hotimage="{PRIH}" '
                          f'pushedimage="{PRIP}" />'), s)
    return s

def rw(rel, text):
    open(os.path.join(DST, rel), 'w', encoding='utf-8', newline='\r\n').write(text)

rw('styles.xml', STYLES)
rw('scrollbar.xml', SCROLL)
for L in ('en-US', 'zh-CN'):
    src = open(os.path.join(DST, L, 'DialogConfig.xml'), encoding='utf-8-sig').read() if False else None
    cur = open(os.path.join(DST, L, 'DialogConfig.xml'), encoding='utf-8-sig').read()
    if 'Common/kf/' in cur:
        base = open(os.path.join(DST, L, 'DialogConfig.xml.pre-design'), encoding='utf-8-sig').read()
    else:
        base = cur
        open(os.path.join(DST, L, 'DialogConfig.xml.pre-design'), 'w', encoding='utf-8', newline='').write(cur)
    out = dialog(base)
    for need in ('Common/kf/tab_sel.png', 'Common/kf/field.png', 'btn_pri.png', 'width="176" height="38"', 'inset="24,14,20,14"'):
        assert need in out, (L, need)
    open(os.path.join(DST, L, 'DialogConfig.xml'), 'w', encoding='utf-8', newline='').write(out)
    print(L, 'ok')
