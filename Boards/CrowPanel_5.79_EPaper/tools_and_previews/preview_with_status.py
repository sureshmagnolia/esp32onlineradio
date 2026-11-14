from PIL import Image, ImageDraw, ImageFont

W, H = 792, 272

f_base = ImageFont.truetype('C:/Windows/Fonts/bahnschrift.ttf', 295)
glyphs = {}
for c in '0123456789':
    bb = f_base.getbbox(c)
    w_orig = bb[2] - bb[0]
    h_orig = bb[3] - bb[1]
    
    im_c = Image.new('L', (w_orig, h_orig), 255)
    dr_c = ImageDraw.Draw(im_c)
    dr_c.text((-bb[0], -bb[1]), c, font=f_base, fill=0)
    
    w_new = int(w_orig * 0.68)
    h_new = h_orig
    im_scaled = im_c.resize((w_new, h_new), Image.Resampling.LANCZOS)
    glyphs[c] = im_scaled.point(lambda p: 0 if p < 128 else 255, '1')

img = Image.new('1', (W, H), 1)
draw = ImageDraw.Draw(img)

startX = 38
startY = 28
x = startX

for c in ' 8:05':
    if c == ' ':
        x += 65
    elif c == ':':
        cx = x + 18
        cy1 = startY + 54
        cy2 = startY + 160
        r = 11
        draw.ellipse([cx-r, cy1-r, cx+r, cy1+r], fill=0)
        draw.ellipse([cx-r, cy2-r, cx+r, cy2+r], fill=0)
        x += 38
    else:
        g = glyphs[c]
        img.paste(g, (x, startY))
        x += g.width + 12

div_x = 530
draw.line([(div_x, 24), (div_x, 248)], fill=0, width=2)

f_ampm = ImageFont.truetype('C:/Windows/Fonts/bahnschrift.ttf', 50)
f_day  = ImageFont.truetype('C:/Windows/Fonts/bahnschrift.ttf', 30)
f_date = ImageFont.truetype('C:/Windows/Fonts/bahnschrift.ttf', 26)
f_stat = ImageFont.truetype('C:/Windows/Fonts/bahnschrift.ttf', 18)

rx = 555
draw.text((rx, 26), 'PM', font=f_ampm, fill=0)
draw.text((rx, 114), 'THURSDAY', font=f_day, fill=0)
draw.text((rx, 186), '08 OCT 2026', font=f_date, fill=0)

# Status line with subtle wifi indicator
# draw small wifi dot
draw.ellipse([rx, 239, rx+8, 247], fill=0)
draw.text((rx + 15, 235), 'WIFI: suresh', font=f_stat, fill=0)

img.save('d:/ESP32Radio/main_clock_with_status.png')
print('Saved main_clock_with_status.png')
