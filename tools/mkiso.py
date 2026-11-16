#!/usr/bin/env python3
"""Empaqueta el ejecutable de FAROL en una imagen ISO de PSP.

Uso:  python tools/mkiso.py farol.prx FAROL.iso

Solo usa la biblioteca estandar: genera PARAM.SFO, ICON0.PNG, UMD_DATA.BIN y
escribe un ISO9660 con la estructura PSP_GAME/SYSDIR/EBOOT.BIN que esperan
los custom firmware y PPSSPP.
"""
import os
import re
import struct
import sys
import time
import zlib

SECTOR = 2048
TITLE = "FAROL"
DISC_ID = "FARL00001"


# ------------------------------------------------------------------ PARAM.SFO

def make_sfo(entries):
    """entries: clave -> int | str | (str, longitud_maxima)."""
    keys = sorted(entries)
    index = b""
    key_table = b""
    data_table = b""
    for k in keys:
        v = entries[k]
        key_off = len(key_table)
        key_table += k.encode("ascii") + b"\0"
        if isinstance(v, int):
            fmt, used, maxlen = 0x0404, 4, 4
            data = struct.pack("<I", v)
        else:
            maxlen = None
            if isinstance(v, tuple):
                v, maxlen = v
            raw = v.encode("utf-8") + b"\0"
            used = len(raw)
            maxlen = maxlen or ((used + 3) & ~3)
            fmt = 0x0204
            data = raw.ljust(maxlen, b"\0")
        index += struct.pack("<HHIII", key_off, fmt, used, maxlen, len(data_table))
        data_table += data
    key_table += b"\0" * (-len(key_table) % 4)
    key_start = 20 + len(index)
    data_start = key_start + len(key_table)
    header = struct.pack("<4sIIII", b"\0PSF", 0x0101, key_start, data_start, len(keys))
    return header + index + key_table + data_table


# ------------------------------------------------------------------ ICON0.PNG

def load_font(header_path):
    with open(header_path, "r", encoding="utf-8") as f:
        vals = [int(h, 16) for h in re.findall(r"0x([0-9A-Fa-f]{2})", f.read())]
    return [vals[i * 5:i * 5 + 5] for i in range(96)]


def make_png(w, h, pixels):
    """pixels: lista de filas, cada fila una lista de (r, g, b)."""
    raw = b"".join(b"\0" + bytes(c for px in row for c in px) for row in pixels)

    def chunk(tag, data):
        return struct.pack(">I", len(data)) + tag + data + struct.pack(">I", zlib.crc32(tag + data) & 0xFFFFFFFF)

    return (b"\x89PNG\r\n\x1a\n"
            + chunk(b"IHDR", struct.pack(">IIBBBBB", w, h, 8, 2, 0, 0, 0))
            + chunk(b"IDAT", zlib.compress(raw, 9))
            + chunk(b"IEND", b""))


def make_icon(font):
    w, h = 144, 80
    top, bot = (52, 22, 38), (16, 8, 14)
    px = [[tuple(top[i] + (bot[i] - top[i]) * y // (h - 1) for i in range(3)) for _ in range(w)] for y in range(h)]

    def rect(x0, y0, rw, rh, col):
        for y in range(max(0, y0), min(h, y0 + rh)):
            for x in range(max(0, x0), min(w, x0 + rw)):
                px[y][x] = col

    def text(x, y, scale, col, s):
        for ch in s:
            cols = font[ord(ch) - 32]
            for c in range(5):
                for r in range(7):
                    if (cols[c] >> r) & 1:
                        rect(x + c * scale, y + r * scale, scale, scale, col)
            x += 6 * scale

    brass = (204, 160, 78)
    rect(0, 0, w, 2, brass)
    rect(0, h - 2, w, 2, brass)
    rect(0, 0, 2, h, brass)
    rect(w - 2, 0, 2, h, brass)
    text(14, 16, 4, (0, 0, 0), "FAROL")
    text(13, 14, 4, brass, "FAROL")
    text(22, 56, 1, (240, 232, 214), "ROGUELIKE DE POKER")
    return make_png(w, h, px)


# -------------------------------------------------------------------- ISO9660

def both32(v):
    return struct.pack("<I", v) + struct.pack(">I", v)


def both16(v):
    return struct.pack("<H", v) + struct.pack(">H", v)


def pad_sector(data):
    return data + b"\0" * (-len(data) % SECTOR)


class Node:
    def __init__(self, name, content):
        self.name = name                      # bytes; b"" para la raiz
        self.is_dir = isinstance(content, dict)
        self.children = []
        self.data = b"" if self.is_dir else content
        self.lba = 0
        self.size = 0
        self.parent = None
        if self.is_dir:
            for child_name in sorted(content):
                child = Node(child_name.encode("ascii"), content[child_name])
                child.parent = self
                self.children.append(child)


def dir_record(name, lba, size, is_dir, date7):
    rec = (b"\0" + both32(lba) + both32(size) + date7
           + bytes([2 if is_dir else 0, 0, 0]) + both16(1) + bytes([len(name)]) + name)
    if (len(rec) + 1) % 2:
        rec += b"\0"
    return bytes([len(rec) + 1]) + rec


def dir_extent(node, date7):
    parent = node.parent or node
    records = [dir_record(b"\x00", node.lba, node.size, True, date7),
               dir_record(b"\x01", parent.lba, parent.size, True, date7)]
    records += [dir_record(c.name, c.lba, c.size, c.is_dir, date7) for c in node.children]
    out = b""
    for rec in records:
        if (len(out) % SECTOR) + len(rec) > SECTOR:
            out = pad_sector(out)
        out += rec
    return pad_sector(out)


def build_iso(tree, volume_id, system_id="PSP GAME"):
    now = time.gmtime()
    date7 = struct.pack("BBBBBBb", now.tm_year - 1900, now.tm_mon, now.tm_mday,
                        now.tm_hour, now.tm_min, now.tm_sec, 0)
    date17 = time.strftime("%Y%m%d%H%M%S", now).encode("ascii") + b"00\0"
    nodate17 = b"0" * 16 + b"\0"

    root = Node(b"", tree)

    # directorios en anchura (el orden que exige la tabla de rutas)
    dirs = [root]
    for d in dirs:
        dirs.extend(c for c in d.children if c.is_dir)
    files = []

    def collect(d):
        for c in d.children:
            if c.is_dir:
                collect(c)
            else:
                files.append(c)
    collect(root)

    # el tamano de un directorio solo depende de los nombres, no de los LBA
    for d in dirs:
        d.size = len(dir_extent(d, date7))

    def path_table(big_endian):
        out = b""
        for d in dirs:
            name = d.name or b"\x00"
            parent_no = dirs.index(d.parent) + 1 if d.parent else 1
            out += bytes([len(name), 0])
            out += struct.pack(">I" if big_endian else "<I", d.lba)
            out += struct.pack(">H" if big_endian else "<H", parent_no)
            out += name + (b"\0" if len(name) % 2 else b"")
        return out

    pt_size = len(path_table(False))
    pt_sectors = (pt_size + SECTOR - 1) // SECTOR
    lba_pt_l = 18
    lba_pt_m = lba_pt_l + pt_sectors
    lba = lba_pt_m + pt_sectors
    for d in dirs:
        d.lba = lba
        lba += d.size // SECTOR
    for f in files:
        f.size = len(f.data)
        f.lba = lba
        lba += (f.size + SECTOR - 1) // SECTOR
    total = max(lba, 32)

    def a_str(s, n):
        return s.encode("ascii").ljust(n, b" ")

    pvd = bytearray(SECTOR)
    pvd[0:7] = b"\x01CD001\x01"
    pvd[8:40] = a_str(system_id, 32)
    pvd[40:72] = a_str(volume_id, 32)
    pvd[80:88] = both32(total)
    pvd[120:124] = both16(1)
    pvd[124:128] = both16(1)
    pvd[128:132] = both16(SECTOR)
    pvd[132:140] = both32(pt_size)
    pvd[140:144] = struct.pack("<I", lba_pt_l)
    pvd[148:152] = struct.pack(">I", lba_pt_m)
    pvd[156:190] = dir_record(b"\x00", root.lba, root.size, True, date7)
    pvd[190:318] = a_str("", 128)
    pvd[318:446] = a_str("", 128)
    pvd[446:574] = a_str("", 128)
    pvd[574:702] = a_str(system_id, 128)
    pvd[702:739] = a_str("", 37)
    pvd[739:776] = a_str("", 37)
    pvd[776:813] = a_str("", 37)
    pvd[813:830] = date17
    pvd[830:847] = date17
    pvd[847:864] = nodate17
    pvd[864:881] = nodate17
    pvd[881] = 1

    term = bytearray(SECTOR)
    term[0:7] = b"\xffCD001\x01"

    out = bytearray(16 * SECTOR)
    out += pvd + term
    out += pad_sector(path_table(False)) + pad_sector(path_table(True))
    for d in dirs:
        assert len(out) == d.lba * SECTOR
        out += dir_extent(d, date7)
    for f in files:
        assert len(out) == f.lba * SECTOR
        out += pad_sector(f.data)
    out += b"\0" * (total * SECTOR - len(out))
    return bytes(out)


def list_iso(image):
    """Relee la imagen y devuelve {ruta: (lba, tamano)}. Sirve de autocomprobacion."""
    pvd = image[16 * SECTOR:17 * SECTOR]
    assert pvd[0:6] == b"\x01CD001", "falta el descriptor de volumen"
    found = {}

    def walk(lba, size, prefix):
        data = image[lba * SECTOR:lba * SECTOR + size]
        pos = 0
        while pos < len(data):
            n = data[pos]
            if n == 0:
                pos = (pos // SECTOR + 1) * SECTOR
                continue
            rec = data[pos:pos + n]
            ext, = struct.unpack("<I", rec[2:6])
            ln, = struct.unpack("<I", rec[10:14])
            name = rec[33:33 + rec[32]]
            if name not in (b"\x00", b"\x01"):
                path = prefix + name.decode("ascii")
                if rec[25] & 2:
                    walk(ext, ln, path + "/")
                else:
                    found[path] = (ext, ln)
            pos += n

    root = pvd[156:190]
    walk(struct.unpack("<I", root[2:6])[0], struct.unpack("<I", root[10:14])[0], "")
    return found


# ----------------------------------------------------------------------- main

def main():
    if len(sys.argv) != 3:
        print(__doc__)
        return 2
    prx_path, iso_path = sys.argv[1], sys.argv[2]
    here = os.path.dirname(os.path.abspath(__file__))

    with open(prx_path, "rb") as f:
        prx = f.read()
    if prx[:4] not in (b"\x7fELF", b"~PSP"):
        print("ERROR: %s no parece un ejecutable de PSP" % prx_path)
        return 1

    sfo = make_sfo({
        "BOOTABLE": 1,
        "CATEGORY": "UG",
        "DISC_ID": DISC_ID,
        "DISC_NUMBER": 1,
        "DISC_TOTAL": 1,
        "DISC_VERSION": "1.00",
        "PARENTAL_LEVEL": 1,
        "PSP_SYSTEM_VER": "5.00",
        "REGION": 0x8000,
        "TITLE": (TITLE, 128),
    })
    icon = make_icon(load_font(os.path.join(here, "..", "src", "font5x7.h")))
    umd_data = ("%s-%s|0000000000000001|0001|G" % (DISC_ID[:4], DISC_ID[4:])).encode("ascii")

    tree = {
        "UMD_DATA.BIN": umd_data,
        "PSP_GAME": {
            "PARAM.SFO": sfo,
            "ICON0.PNG": icon,
            "SYSDIR": {
                "EBOOT.BIN": prx,
                "BOOT.BIN": prx,
            },
            "USRDIR": {},
        },
    }
    image = build_iso(tree, TITLE)

    listing = list_iso(image)
    for path, data in (("UMD_DATA.BIN", umd_data), ("PSP_GAME/PARAM.SFO", sfo),
                       ("PSP_GAME/ICON0.PNG", icon), ("PSP_GAME/SYSDIR/EBOOT.BIN", prx),
                       ("PSP_GAME/SYSDIR/BOOT.BIN", prx)):
        lba, size = listing[path]
        assert image[lba * SECTOR:lba * SECTOR + size] == data, "contenido incorrecto: " + path

    with open(iso_path, "wb") as f:
        f.write(image)
    print("ISO creado: %s (%d KB)" % (iso_path, len(image) // 1024))
    for path in sorted(listing):
        print("  %-28s %8d bytes  LBA %d" % (path, listing[path][1], listing[path][0]))
    return 0


if __name__ == "__main__":
    sys.exit(main())
