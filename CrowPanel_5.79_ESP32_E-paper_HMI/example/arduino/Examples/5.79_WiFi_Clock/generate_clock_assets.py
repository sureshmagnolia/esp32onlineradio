import os
from PIL import Image, ImageDraw, ImageFont

def generate_header():
    header_path = r'd:\ESP32Radio\CrowPanel_5.79_ESP32_E-paper_HMI\example\arduino\Examples\5.79_WiFi_Clock\MassiveFont.h'
    f_base = ImageFont.truetype('C:/Windows/Fonts/bahnschrift.ttf', 295)
    f_ampm = ImageFont.truetype('C:/Windows/Fonts/bahnschrift.ttf', 52)
    f_text = ImageFont.truetype('C:/Windows/Fonts/bahnschrift.ttf', 30)

    out = open(header_path, 'w')
    out.write('#pragma once\n#include <Arduino.h>\n\n')

    # Structure definition
    out.write('''struct ClockGlyph {
    char c;
    const uint8_t* data;
    int w;
    int h;
    int y_offset;
};

''')

    # 1. Massive Time Digits 0-9
    out.write('// === Massive Time Digits (0-9) ===\n')
    massive_map = []
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
        im_bw = im_scaled.point(lambda p: 0 if p < 128 else 255, '1')

        name = f'glyph_time_{ord(c)}'
        massive_map.append((c, name, w_new, h_new, 0))

        out.write(f'const uint8_t {name}[] PROGMEM = {{\n')
        bytes_arr = []
        curr = 0
        bit_cnt = 0
        for y in range(h_new):
            for x in range(w_new):
                # 0 is black, 255 is white
                # We want 1 for black pixel, 0 for white
                is_black = (im_bw.getpixel((x, y)) == 0)
                if is_black:
                    curr |= (1 << (7 - bit_cnt))
                bit_cnt += 1
                if bit_cnt == 8:
                    bytes_arr.append(curr)
                    curr = 0
                    bit_cnt = 0
        if bit_cnt > 0:
            bytes_arr.append(curr)

        for i, b in enumerate(bytes_arr):
            out.write(f'0x{b:02X}, ')
            if (i + 1) % 16 == 0:
                out.write('\n')
        out.write('\n};\n\n')

    out.write('const ClockGlyph massive_time_digits[] = {\n')
    for c, name, w, h, y_off in massive_map:
        out.write(f"    {{'{c}', {name}, {w}, {h}, {y_off}}},\n")
    out.write('    {0, nullptr, 0, 0, 0}\n};\n\n')

    # 2. AM/PM Glyphs ('A', 'M', 'P')
    out.write('// === AM/PM Glyphs ===\n')
    ampm_map = []
    for c in 'AMP':
        bb = f_ampm.getbbox(c)
        w = bb[2] - bb[0]
        h = bb[3] - bb[1]
        im_c = Image.new('L', (w, h), 255)
        dr_c = ImageDraw.Draw(im_c)
        dr_c.text((-bb[0], -bb[1]), c, font=f_ampm, fill=0)
        im_bw = im_c.point(lambda p: 0 if p < 128 else 255, '1')

        name = f'glyph_ampm_{ord(c)}'
        ampm_map.append((c, name, w, h, 0))

        out.write(f'const uint8_t {name}[] PROGMEM = {{\n')
        bytes_arr = []
        curr = 0
        bit_cnt = 0
        for y in range(h):
            for x in range(w):
                is_black = (im_bw.getpixel((x, y)) == 0)
                if is_black:
                    curr |= (1 << (7 - bit_cnt))
                bit_cnt += 1
                if bit_cnt == 8:
                    bytes_arr.append(curr)
                    curr = 0
                    bit_cnt = 0
        if bit_cnt > 0:
            bytes_arr.append(curr)

        for i, b in enumerate(bytes_arr):
            out.write(f'0x{b:02X}, ')
            if (i + 1) % 16 == 0:
                out.write('\n')
        out.write('\n};\n\n')

    out.write('const ClockGlyph ampm_glyphs[] = {\n')
    for c, name, w, h, y_off in ampm_map:
        out.write(f"    {{'{c}', {name}, {w}, {h}, {y_off}}},\n")
    out.write('    {0, nullptr, 0, 0, 0}\n};\n\n')

    # 3. Text Glyphs (A-Z, 0-9)
    out.write('// === Text Glyphs (A-Z, 0-9) ===\n')
    text_chars = 'ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789'
    text_map = []
    for c in text_chars:
        bb = f_text.getbbox(c)
        w = bb[2] - bb[0]
        h = bb[3] - bb[1]
        im_c = Image.new('L', (w, h), 255)
        dr_c = ImageDraw.Draw(im_c)
        dr_c.text((-bb[0], -bb[1]), c, font=f_text, fill=0)
        im_bw = im_c.point(lambda p: 0 if p < 128 else 255, '1')

        name = f'glyph_text_{ord(c)}'
        text_map.append((c, name, w, h, 0))

        out.write(f'const uint8_t {name}[] PROGMEM = {{\n')
        bytes_arr = []
        curr = 0
        bit_cnt = 0
        for y in range(h):
            for x in range(w):
                is_black = (im_bw.getpixel((x, y)) == 0)
                if is_black:
                    curr |= (1 << (7 - bit_cnt))
                bit_cnt += 1
                if bit_cnt == 8:
                    bytes_arr.append(curr)
                    curr = 0
                    bit_cnt = 0
        if bit_cnt > 0:
            bytes_arr.append(curr)

        for i, b in enumerate(bytes_arr):
            out.write(f'0x{b:02X}, ')
            if (i + 1) % 16 == 0:
                out.write('\n')
        out.write('\n};\n\n')

    out.write('const ClockGlyph text_glyphs[] = {\n')
    for c, name, w, h, y_off in text_map:
        out.write(f"    {{'{c}', {name}, {w}, {h}, {y_off}}},\n")
    out.write('    {0, nullptr, 0, 0, 0}\n};\n\n')

    out.close()
    print('Generated MassiveFont.h successfully.')

if __name__ == '__main__':
    generate_header()
