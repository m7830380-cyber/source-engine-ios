#!/usr/bin/env python3
"""Add touch-control icons to scripts/ios/bundle/extras_dir.vpk.

The touch icons (materials/vgui/touch/*) come from the source-engine port and
have no scoreboard icon; the scoreboard button used "change class". This draws
icons in the same style (a light grey ring at ~30% alpha, 6 px, and a glyph in
the same color), encodes them in the exact VTF layout of the existing icons
(7.5, 128x128 BGRA8888, 8 mips, 16x16 RGBA8888 thumbnail; the header is copied
from changeclass.vtf) and rewrites the single-file VPK v2 with Valve's MD5
section (directory, chunk hashes, whole file). Re-running replaces the icons.

    python3 scripts/ios/touch_icons.py
"""
import hashlib
import os
import struct
import sys

VPK = os.path.join(os.path.dirname(os.path.abspath(__file__)), 'bundle', 'extras_dir.vpk')
TEMPLATE = 'materials/vgui/touch/changeclass'
COLOR = (209, 209, 209)
ALPHA = 77
SIZE = 128
SS = 4  # supersampling per axis


def read_cstr(d, p):
    e = d.index(b'\0', p)
    return d[p:e].decode('latin1'), e + 1


def read_vpk(path):
    d = open(path, 'rb').read()
    sig, ver, tree = struct.unpack_from('<III', d, 0)
    assert sig == 0x55AA1234 and ver == 2, 'expected a VPK v2'
    data_size, arch_md5, other_md5, sig_size = struct.unpack_from('<IIII', d, 12)
    assert arch_md5 == 0 and sig_size == 0, 'multi-archive or signed VPKs are not handled'
    tree_start = 28
    data_start = tree_start + tree
    entries = {}
    p = tree_start
    while True:
        ext, p = read_cstr(d, p)
        if not ext:
            break
        while True:
            path_, p = read_cstr(d, p)
            if not path_:
                break
            while True:
                name, p = read_cstr(d, p)
                if not name:
                    break
                crc, pre, arc, off, ln, term = struct.unpack_from('<IHHIIH', d, p)
                p += 18
                predata = d[p:p + pre]
                p += pre
                assert arc == 0x7FFF, 'only data embedded in the directory file is handled'
                full = ('%s/%s.%s' % (path_, name, ext)).lstrip(' /')
                entries[full] = predata + d[data_start + off:data_start + off + ln]
    return entries


def write_vpk(path, entries):
    # group as ext -> dir -> [names]; Valve writes " " for an empty directory
    tree = {}
    for full in sorted(entries):
        d, fn = full.rsplit('/', 1) if '/' in full else (' ', full)
        name, ext = fn.rsplit('.', 1)
        tree.setdefault(ext, {}).setdefault(d, []).append((name, full))
    tb = bytearray()
    data = bytearray()
    for ext in sorted(tree):
        tb += ext.encode() + b'\0'
        for d in sorted(tree[ext]):
            tb += d.encode() + b'\0'
            for name, full in tree[ext][d]:
                blob = entries[full]
                import zlib
                crc = zlib.crc32(blob) & 0xFFFFFFFF
                tb += name.encode() + b'\0'
                tb += struct.pack('<IHHIIH', crc, 0, 0x7FFF, len(data), len(blob), 0xFFFF)
                data += blob
            tb += b'\0'
        tb += b'\0'
    tb += b'\0'
    header = struct.pack('<IIIIIII', 0x55AA1234, 2, len(tb), len(data), 0, 48, 0)
    out = bytearray(header) + tb + data
    out += hashlib.md5(bytes(tb)).digest()   # directory
    out += hashlib.md5(b'').digest()         # chunk hashes (none)
    out += hashlib.md5(bytes(out)).digest()  # everything so far
    open(path, 'wb').write(out)


def render(draw_glyph):
    """RGBA rows of a SIZExSIZE icon: ring plus glyph, supersampled."""
    n = SIZE * SS
    cov = bytearray(n * n)
    c = n / 2.0
    r_out, r_in = n / 2.0, n / 2.0 - 6 * SS
    for y in range(n):
        dy = y + 0.5 - c
        for x in range(n):
            dx = x + 0.5 - c
            rr = dx * dx + dy * dy
            if r_in * r_in <= rr <= r_out * r_out:
                cov[y * n + x] = 1

    def rect(x0, y0, x1, y1, fill=1):
        for y in range(int(y0 * SS), int(y1 * SS)):
            for x in range(int(x0 * SS), int(x1 * SS)):
                cov[y * n + x] = fill

    draw_glyph(rect)
    rgba = bytearray(SIZE * SIZE * 4)
    for y in range(SIZE):
        for x in range(SIZE):
            s = 0
            for sy in range(SS):
                row = (y * SS + sy) * n + x * SS
                s += sum(cov[row:row + SS])
            a = ALPHA * s // (SS * SS)
            o = (y * SIZE + x) * 4
            rgba[o:o + 4] = bytes((COLOR[0], COLOR[1], COLOR[2], a))
    return rgba


def podium(rect):
    # three steps: 2nd (left), 1st (middle, tallest), 3rd (right)
    base = 98
    rect(24, base - 34, 50, base)
    rect(51, base - 62, 77, base)
    rect(78, base - 22, 104, base)
    # a "1" cut out of the top step
    rect(62, base - 54, 67, base - 34, 0)
    rect(59, base - 54, 62, base - 50, 0)


def downsample(rgba, w, h):
    out = bytearray((w // 2) * (h // 2) * 4)
    for y in range(h // 2):
        for x in range(w // 2):
            for ch in range(4):
                s = sum(rgba[((2 * y + dy) * w + 2 * x + dx) * 4 + ch] for dy in (0, 1) for dx in (0, 1))
                out[(y * (w // 2) + x) * 4 + ch] = s // 4
    return out


def make_vtf(template, rgba):
    hdr_size = struct.unpack_from('<I', template, 12)[0]
    w, h = struct.unpack_from('<HH', template, 16)
    hi_fmt, mips, lo_fmt, lo_w, lo_h = struct.unpack_from('<iBiBB', template, 52)
    assert (w, h, hi_fmt, lo_fmt, lo_w, lo_h) == (SIZE, SIZE, 12, 0, 16, 16), 'unexpected template layout'
    levels = [rgba]
    cw = w
    while len(levels) < mips:
        levels.append(downsample(levels[-1], cw, cw))
        cw //= 2
    thumb = levels[3]  # 16x16, RGBA8888
    body = bytearray(template[:hdr_size]) + thumb
    for lvl in reversed(levels):  # smallest mip first
        body += bytes(b for i in range(0, len(lvl), 4) for b in (lvl[i + 2], lvl[i + 1], lvl[i], lvl[i + 3]))
    assert len(body) == len(template), (len(body), len(template))
    return bytes(body)


def main():
    entries = read_vpk(VPK)
    template = entries[TEMPLATE + '.vtf']
    vmt = entries[TEMPLATE + '.vmt'].decode('latin1')
    icons = {'scoreboard': podium}
    for name, glyph in icons.items():
        base = 'materials/vgui/touch/' + name
        entries[base + '.vtf'] = make_vtf(template, render(glyph))
        entries[base + '.vmt'] = vmt.replace('vgui/touch/changeclass', 'vgui/touch/' + name).encode('latin1')
        print('added', base)
    write_vpk(VPK, entries)
    check = read_vpk(VPK)
    assert set(check) == set(entries) and all(check[k] == entries[k] for k in entries)
    print('%s: %d files' % (VPK, len(check)))


if __name__ == '__main__':
    sys.exit(main())
