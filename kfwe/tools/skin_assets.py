from PIL import Image, ImageDraw
import os

OUT = r'C:\Games\Warcraft 3\KFWE\share\skins\Common\kf'
os.makedirs(OUT, exist_ok=True)
S = 4

ACC, ACC_HI, ACC_LO = (124, 92, 255), (143, 116, 255), (106, 75, 240)
TRACK, TRACK_HI = (58, 61, 70), (72, 76, 87)
KNOB = (240, 241, 245)
RING, RING_HI = (96, 101, 114), (130, 136, 150)

def img(w, h):
    return Image.new('RGBA', (w * S, h * S), (0, 0, 0, 0))

def save(im, name, w, h):
    im.resize((w, h), Image.LANCZOS).save(os.path.join(OUT, name))

def rrect(d, box, r, fill=None, outline=None, width=1):
    d.rounded_rectangle([v * S for v in box], radius=r * S, fill=fill, outline=outline, width=width * S)

def switch(name, on, hot):
    im = img(36, 20); d = ImageDraw.Draw(im)
    rrect(d, (0, 0, 35.99, 19.99), 10, fill=(ACC_HI if hot else ACC) if on else (TRACK_HI if hot else TRACK))
    cx = 26 if on else 10
    d.ellipse([(cx - 7) * S, 3 * S, (cx + 7) * S, 17 * S], fill=KNOB)
    save(im, name, 36, 20)
for on in (0, 1):
    for hot in (0, 1):
        switch('switch_%s%s.png' % ('on' if on else 'off', '_hot' if hot else ''), on, hot)

def radio(name, on, hot):
    im = img(18, 18); d = ImageDraw.Draw(im)
    if on:
        d.ellipse([0, 0, 18 * S - 1, 18 * S - 1], fill=ACC_HI if hot else ACC)
        d.ellipse([5 * S, 5 * S, 13 * S, 13 * S], fill=KNOB)
    else:
        d.ellipse([S, S, 17 * S, 17 * S], outline=RING_HI if hot else RING, width=2 * S)
    save(im, name, 18, 18)
for on in (0, 1):
    for hot in (0, 1):
        radio('radio_%s%s.png' % ('on' if on else 'off', '_hot' if hot else ''), on, hot)

def tab(name, fill, bar):
    im = img(48, 38); d = ImageDraw.Draw(im)
    rrect(d, (0, 0, 47.99, 37.99), 9, fill=fill)
    if bar:
        rrect(d, (0, 9, 3.5, 29), 1.75, fill=ACC)
    save(im, name, 48, 38)
tab('tab_hot.png', (38, 40, 47), False)
tab('tab_sel.png', (44, 38, 72), True)

def button(name, fill, outline=None):
    im = img(48, 34); d = ImageDraw.Draw(im)
    rrect(d, (0.5, 0.5, 47.5, 33.5), 8, fill=fill, outline=outline, width=1)
    save(im, name, 48, 34)
button('btn.png', (38, 40, 47), (62, 66, 76))
button('btn_hot.png', (48, 51, 60), (82, 87, 100))
button('btn_push.png', (32, 34, 40), (62, 66, 76))
button('btn_pri.png', ACC)
button('btn_pri_hot.png', ACC_HI)
button('btn_pri_push.png', ACC_LO)

def field(name, outline):
    im = img(48, 30); d = ImageDraw.Draw(im)
    rrect(d, (0.5, 0.5, 47.5, 29.5), 7, fill=(43, 45, 51), outline=outline, width=1)
    save(im, name, 48, 30)
field('field.png', (62, 66, 76))
field('field_hot.png', ACC)

Image.new('RGBA', (1, 1), (46, 48, 56, 255)).save(os.path.join(OUT, 'line.png'))
Image.new('RGBA', (1, 1), (0, 0, 0, 0)).save(os.path.join(OUT, 'empty.png'))
def thumb(name, c):
    im = img(8, 40); d = ImageDraw.Draw(im)
    rrect(d, (1, 0, 6.99, 39.99), 3, fill=c)
    save(im, name, 8, 40)
thumb('thumb.png', (66, 70, 80)); thumb('thumb_hot.png', (96, 101, 114)); thumb('thumb_push.png', ACC)
print('ok', sorted(os.listdir(OUT)))
