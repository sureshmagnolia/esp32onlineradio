from PIL import Image, ImageDraw, ImageFont

W, H = 792, 272
img = Image.new('1', (W, H), 1)
draw = ImageDraw.Draw(img)

f_title = ImageFont.truetype('C:/Windows/Fonts/bahnschrift.ttf', 36)
f_sub   = ImageFont.truetype('C:/Windows/Fonts/bahnschrift.ttf', 24)
f_mono  = ImageFont.truetype('C:/Windows/Fonts/bahnschrift.ttf', 22)

# Card border / header
draw.text((60, 30), 'CROWPANEL 5.79" E-PAPER CLOCK', font=f_title, fill=0)
draw.line([(60, 78), (732, 78)], fill=0, width=2)

# Checklist items
draw.text((60, 100), '> SYSTEM BOOT & HARDWARE INITIALIZATION... OK', font=f_mono, fill=0)
draw.text((60, 138), '> CONNECTING TO WI-FI (suresh)... CONNECTED [192.168.1.105]', font=f_mono, fill=0)
draw.text((60, 176), '> SYNCING NTP TIME (UTC+5:30 IST)... SYNCED [08:05 PM]', font=f_mono, fill=0)

draw.line([(60, 222), (732, 222)], fill=0, width=1)
draw.text((60, 232), 'LOADING CLOCK INTERFACE...', font=f_sub, fill=0)

img.save('d:/ESP32Radio/startup_preview.png')
print('Saved startup_preview.png')
