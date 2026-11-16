#!/usr/bin/env python3
"""Convierte tools/art.txt (iconos de 16x16 dibujados con letras) en src/art.h.

Uso:  python tools/mkart.py [vista_previa.png]

Cada icono empieza con una linea "= nombre" seguida de 16 filas de 16 letras.
Cada letra es un color de la paleta de abajo; "." es transparente y "v" es un
color variable que el juego sustituye (asi un mismo dibujo sirve para varios trucos).
"""
import os
import struct
import sys
import zlib

SIZE = 16
PALETTE = {
    'k': (28, 22, 28),     # contorno
    'w': (248, 244, 232),  # blanco
    'g': (160, 160, 172),  # gris
    'd': (92, 90, 104),    # gris oscuro
    'r': (214, 52, 62),    # rojo
    'R': (140, 30, 46),    # rojo oscuro
    'o': (238, 142, 42),   # naranja
    'y': (246, 206, 82),   # oro
    'Y': (190, 138, 40),   # oro oscuro
    'G': (64, 176, 96),    # verde
    'h': (30, 112, 70),    # verde oscuro
    'b': (72, 146, 228),   # azul
    'B': (40, 80, 160),    # azul oscuro
    'p': (164, 104, 224),  # morado
    'n': (136, 88, 52),    # marron
    'N': (86, 56, 36),     # marron oscuro
    's': (238, 204, 164),  # crema
    'c': (130, 224, 238),  # celeste
    'v': (255, 0, 255),    # variable (solo se ve asi en la vista previa)
}

here = os.path.dirname(os.path.abspath(__file__))


def load():
    icons, name, rows, problems = [], None, [], []
    with open(os.path.join(here, 'art.txt'), encoding='utf-8') as f:
        lines = [ln.rstrip('\n') for ln in f]
    lines.append('= __fin__')
    for ln in lines:
        if ln.startswith('#') or (not ln.strip() and not rows):
            continue
        if ln.startswith('='):
            if name is not None:
                if len(rows) != SIZE:
                    problems.append('%s: %d filas' % (name, len(rows)))
                rows = (rows + ['.' * SIZE] * SIZE)[:SIZE]
                icons.append((name, rows))
            name, rows = ln[1:].strip(), []
            continue
        row = ln.strip()
        if not row:
            continue
        if len(row) != SIZE:
            problems.append('%s fila %d: %d letras (%s)' % (name, len(rows), len(row), row))
            row = (row + '.' * SIZE)[:SIZE]
        for ch in row:
            if ch != '.' and ch not in PALETTE:
                problems.append('%s fila %d: letra desconocida %r' % (name, len(rows), ch))
        rows.append(''.join(ch if ch == '.' or ch in PALETTE else '.' for ch in row))
    return icons, problems


def write_header(icons):
    keys = ''.join(PALETTE)
    out = ['/* GENERADO por tools/mkart.py a partir de tools/art.txt. No editar a mano. */',
           '#ifndef ART_H', '#define ART_H', '',
           '#define ART_SIZE %d' % SIZE,
           '#define ART_COUNT %d' % len(icons), '',
           '/* letra -> color (R, G, B). La "v" es el color variable. */',
           'static const char ART_KEYS[] = "%s";' % keys,
           'static const unsigned char ART_RGB[][3] = {']
    out += ['    { %3d, %3d, %3d },   /* %s */' % (PALETTE[k] + (k,)) for k in keys]
    out += ['};', '', 'enum {']
    out += ['    ART_%s,' % n.upper() for n, _ in icons]
    out += ['};', '', 'static const char *const ART_ROWS[ART_COUNT][ART_SIZE] = {']
    for n, rows in icons:
        out.append('    { /* %s */' % n)
        out += ['      "%s",' % r for r in rows]
        out.append('    },')
    out += ['};', '', '#endif', '']
    with open(os.path.join(here, '..', 'src', 'art.h'), 'w', encoding='utf-8', newline='\n') as f:
        f.write('\n'.join(out))


def write_preview(icons, path, scale=4, cols=13):
    cell = SIZE * scale + 8
    rows_n = (len(icons) + cols - 1) // cols
    w, h = cols * cell, rows_n * cell
    px = [[(48, 34, 42)] * w for _ in range(h)]
    for i, (_, rows) in enumerate(icons):
        ox, oy = (i % cols) * cell + 4, (i // cols) * cell + 4
        for y, row in enumerate(rows):
            for x, ch in enumerate(row):
                if ch == '.':
                    continue
                for dy in range(scale):
                    line = px[oy + y * scale + dy]
                    for dx in range(scale):
                        line[ox + x * scale + dx] = PALETTE[ch]
    raw = b''.join(b'\0' + bytes(c for p in row for c in p) for row in px)

    def chunk(tag, d):
        return struct.pack('>I', len(d)) + tag + d + struct.pack('>I', zlib.crc32(tag + d) & 0xFFFFFFFF)

    with open(path, 'wb') as f:
        f.write(b'\x89PNG\r\n\x1a\n' + chunk(b'IHDR', struct.pack('>IIBBBBB', w, h, 8, 2, 0, 0, 0))
                + chunk(b'IDAT', zlib.compress(raw, 9)) + chunk(b'IEND', b''))


def main():
    icons, problems = load()
    for p in problems:
        print('AVISO:', p)
    write_header(icons)
    print('%d iconos -> src/art.h (%d avisos)' % (len(icons), len(problems)))
    if len(sys.argv) > 1:
        write_preview(icons, sys.argv[1])
        print('vista previa:', sys.argv[1])
    return 1 if problems else 0


if __name__ == '__main__':
    sys.exit(main())
