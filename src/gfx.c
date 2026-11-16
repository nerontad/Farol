/* FAROL - capa de dibujo 2D sobre sceGu */
#include <pspkernel.h>
#include <pspdisplay.h>
#include <pspgu.h>
#include <string.h>
#include <math.h>
#ifdef AUTOTEST
#include <pspiofilemgr.h>
#include <stdio.h>
#endif
#include "gfx.h"
#include "font5x7.h"

#define BUF_W   512
#define ATLAS_W 128
#define ATLAS_H 128
#define CELL_W  8
#define CELL_H  10
#define ADV     6      /* avance de una letra, en pixeles a escala 1 */
#define ADV_ICON 8

/* Solo se dibuja la parte util de cada celda: asi un glifo girado nunca roza a su vecino */
#define QUAD_W  (CELL_W - 1)
#define QUAD_H  (CELL_H - 1)

/* Indices de glifo dentro del atlas: 0..95 = ASCII 32..127 */
enum {
    GL_A_ACUTE = 96, GL_E_ACUTE, GL_I_ACUTE, GL_O_ACUTE, GL_U_ACUTE,
    GL_N_TILDE, GL_N_TILDE_UP, GL_Q_INV, GL_EX_INV, GL_U_DIAER,
    GL_ICON0
};

typedef struct { unsigned int color; short x, y, z; } VertC;
typedef struct { short u, v; unsigned int color; short x, y, z; } VertT;
/* versiones con coordenadas en coma flotante, para lo que va girado o escalado */
typedef struct { unsigned int color; float x, y, z; } VertCF;
typedef struct { float u, v; unsigned int color; float x, y, z; } VertTF;

#define FMT_C  (GU_COLOR_8888 | GU_VERTEX_16BIT | GU_TRANSFORM_2D)
#define FMT_T  (GU_TEXTURE_16BIT | GU_COLOR_8888 | GU_VERTEX_16BIT | GU_TRANSFORM_2D)
#define FMT_CF (GU_COLOR_8888 | GU_VERTEX_32BITF | GU_TRANSFORM_2D)
#define FMT_TF (GU_TEXTURE_32BITF | GU_COLOR_8888 | GU_VERTEX_32BITF | GU_TRANSFORM_2D)

static unsigned int __attribute__((aligned(16))) gu_list[262144];
static unsigned int __attribute__((aligned(16))) atlas[ATLAS_W * ATLAS_H];
/* Segunda textura: las ilustraciones (iconos de 16x16 con 1 px de margen entre celdas) */
#define ART_W     256
#define ART_H     128
#define ART_PITCH 18
#define ART_COLS  (ART_W / ART_PITCH)
#define ART_SIZE  16
static unsigned int __attribute__((aligned(16))) art_tex[ART_W * ART_H];
static int cur_tex = 0;     /* 0 = fuente, 1 = ilustraciones */

static int tex_on = 0;
static int draw_buf = 0;    /* 0 o 1: en cual de los dos framebuffers se esta dibujando */
static int shown_buf = 1;

/* transformacion local (giro + escala alrededor de un punto) y temblor global */
static int xf_on = 0;
static float xf_cx, xf_cy, xf_a = 1.0f, xf_b = 0.0f;
static int sh_x = 0, sh_y = 0;

static const unsigned char QUAD_CORNER[6] = { 0, 1, 2, 2, 1, 3 };

static const char *const ICONS[ICON_COUNT][7] = {
    { "...#...", "..###..", ".#####.", "#######", "##.#.##", "...#...", "..###.." }, /* pica */
    { ".##.##.", "#######", "#######", "#######", ".#####.", "..###..", "...#..." }, /* corazon */
    { "..###..", "..###..", "##.#.##", "#######", "##.#.##", "...#...", "..###.." }, /* trebol */
    { "...#...", "..###..", ".#####.", "#######", ".#####.", "..###..", "...#..." }, /* diamante */
    { "#.....#", ".#...#.", "..#.#..", "...#...", "..#.#..", ".#...#.", "#.....#" }, /* cruz */
    { "..###..", ".#...#.", "#.....#", "#.....#", "#.....#", ".#...#.", "..###.." }, /* circulo */
    { "#######", "#.....#", "#.....#", "#.....#", "#.....#", "#.....#", "#######" }, /* cuadrado */
    { "...#...", "...#...", "..#.#..", "..#.#..", ".#...#.", ".#...#.", "#######" }, /* triangulo */
};

/* ------------------------------------------------------------------ atlas */

static void put(int g, int col, int row)
{
    int cx = (g % 16) * CELL_W + col;
    int cy = (g / 16) * CELL_H + row;
    atlas[cy * ATLAS_W + cx] = 0xFFFFFFFFu;
}

/* Copia un glifo 5x8 a la celda (la fila 0 de la celda queda libre para tildes) */
static void put_cols(int g, const unsigned char *cols, int flip)
{
    int c, r;
    for (c = 0; c < 5; c++)
        for (r = 0; r < 8; r++)
            if ((cols[c] >> r) & 1)
                put(g, c, 1 + ((flip && r < 7) ? 6 - r : r));
}

static void put_accented(int g, char base)
{
    put_cols(g, font5x7[base - 32], 0);
    put(g, 3, 0);
    put(g, 2, 1);
}

static void build_atlas(void)
{
    int i, r, c;
    memset(atlas, 0, sizeof(atlas));
    for (i = 0; i < 96; i++) put_cols(i, font5x7[i], 0);

    put_accented(GL_A_ACUTE, 'a');
    put_accented(GL_E_ACUTE, 'e');
    put_accented(GL_I_ACUTE, 'i');
    put_accented(GL_O_ACUTE, 'o');
    put_accented(GL_U_ACUTE, 'u');

    put_cols(GL_N_TILDE, font5x7['n' - 32], 0);
    put(GL_N_TILDE, 1, 0); put(GL_N_TILDE, 4, 0);
    put(GL_N_TILDE, 0, 1); put(GL_N_TILDE, 2, 1); put(GL_N_TILDE, 3, 1);

    put_cols(GL_N_TILDE_UP, font5x7['N' - 32], 0);
    put(GL_N_TILDE_UP, 1, 0); put(GL_N_TILDE_UP, 2, 0); put(GL_N_TILDE_UP, 3, 0);

    put_cols(GL_Q_INV, font5x7['?' - 32], 1);
    put_cols(GL_EX_INV, font5x7['!' - 32], 1);

    put_cols(GL_U_DIAER, font5x7['u' - 32], 0);
    put(GL_U_DIAER, 1, 1); put(GL_U_DIAER, 3, 1);

    for (i = 0; i < ICON_COUNT; i++)
        for (r = 0; r < 7; r++)
            for (c = 0; c < 7; c++)
                if (ICONS[i][r][c] == '#') put(GL_ICON0 + i, c, 1 + r);

    sceKernelDcacheWritebackInvalidateAll();
}

/* ------------------------------------------------------------ inicio/fin */

static void blend_normal(void)
{
    sceGuBlendFunc(GU_ADD, GU_SRC_ALPHA, GU_ONE_MINUS_SRC_ALPHA, 0, 0);
}

void gfx_init(void)
{
    void *fb0 = (void *)0;
    void *fb1 = (void *)(BUF_W * SCR_H * 4);

    build_atlas();

    sceGuInit();
    sceGuStart(GU_DIRECT, gu_list);
    sceGuDrawBuffer(GU_PSM_8888, fb0, BUF_W);
    sceGuDispBuffer(SCR_W, SCR_H, fb1, BUF_W);
    sceGuOffset(2048 - (SCR_W / 2), 2048 - (SCR_H / 2));
    sceGuViewport(2048, 2048, SCR_W, SCR_H);
    sceGuScissor(0, 0, SCR_W, SCR_H);
    sceGuEnable(GU_SCISSOR_TEST);
    sceGuDisable(GU_DEPTH_TEST);
    sceGuDisable(GU_CULL_FACE);
    sceGuShadeModel(GU_SMOOTH);
    sceGuEnable(GU_BLEND);
    blend_normal();
    sceGuDisable(GU_TEXTURE_2D);
    sceGuFinish();
    sceGuSync(0, 0);
    sceDisplayWaitVblankStart();
    sceGuDisplay(GU_TRUE);
}

void gfx_begin(unsigned int clearColor)
{
    sceGuStart(GU_DIRECT, gu_list);
    sceGuClearColor(clearColor);
    sceGuClear(GU_COLOR_BUFFER_BIT);

    sceGuTexMode(GU_PSM_8888, 0, 0, 0);
    sceGuTexImage(0, ATLAS_W, ATLAS_H, ATLAS_W, atlas);
    sceGuTexFunc(GU_TFX_MODULATE, GU_TCC_RGBA);
    sceGuTexFilter(GU_NEAREST, GU_NEAREST);
    sceGuTexWrap(GU_CLAMP, GU_CLAMP);
    sceGuDisable(GU_TEXTURE_2D);
    tex_on = 0;
    cur_tex = 0;
    blend_normal();
    xf_on = 0;
    sh_x = sh_y = 0;
}

static void bind_tex(int t)
{
    if (t == cur_tex) return;
    if (t) sceGuTexImage(0, ART_W, ART_H, ART_W, art_tex);
    else sceGuTexImage(0, ATLAS_W, ATLAS_H, ATLAS_W, atlas);
    cur_tex = t;
}

static void (*frame_hook)(void) = NULL;

void gfx_set_frame_hook(void (*fn)(void))
{
    frame_hook = fn;
}

void gfx_end(void)
{
    sceGuFinish();
    sceGuSync(0, 0);
    if (frame_hook) frame_hook();    /* aqui trabaja la utilidad de guardado del sistema */
    sceDisplayWaitVblankStart();
    sceGuSwapBuffers();
    shown_buf = draw_buf;
    draw_buf ^= 1;
}

#ifdef AUTOTEST
void gfx_dump(const char *name)
{
    char path[64];
    const unsigned int *fb = (const unsigned int *)(0x44000000 + shown_buf * (BUF_W * SCR_H * 4));
    SceUID fd;
    int y;

    snprintf(path, sizeof(path), "ms0:/farol_%s.raw", name);
    fd = sceIoOpen(path, PSP_O_WRONLY | PSP_O_CREAT | PSP_O_TRUNC, 0777);
    if (fd < 0) return;
    for (y = 0; y < SCR_H; y++)
        sceIoWrite(fd, fb + y * BUF_W, SCR_W * 4);
    sceIoClose(fd);
}
#endif

static void set_tex(int on)
{
    if (on == tex_on) return;
    if (on) sceGuEnable(GU_TEXTURE_2D);
    else sceGuDisable(GU_TEXTURE_2D);
    tex_on = on;
}

/* ---------------------------------------------------------------- efectos */

void gfx_shake(int dx, int dy)
{
    sh_x = dx;
    sh_y = dy;
}

void gfx_xform(float cx, float cy, float angle, float scale)
{
    xf_cx = cx;
    xf_cy = cy;
    xf_a = cosf(angle) * scale;
    xf_b = sinf(angle) * scale;
    xf_on = 1;
}

void gfx_xform_off(void)
{
    xf_on = 0;
}

void gfx_additive(int on)
{
    if (on) sceGuBlendFunc(GU_ADD, GU_SRC_ALPHA, GU_FIX, 0, 0xFFFFFFFF);
    else blend_normal();
}

static void xf_point(float x, float y, float *ox, float *oy)
{
    float dx = x - xf_cx, dy = y - xf_cy;
    *ox = xf_cx + dx * xf_a - dy * xf_b + (float)sh_x;
    *oy = xf_cy + dx * xf_b + dy * xf_a + (float)sh_y;
}

/* Las cuatro esquinas de un rectangulo, ya transformadas */
static void xf_corners(int x, int y, int w, int h, float *px, float *py)
{
    xf_point((float)x, (float)y, &px[0], &py[0]);
    xf_point((float)(x + w), (float)y, &px[1], &py[1]);
    xf_point((float)x, (float)(y + h), &px[2], &py[2]);
    xf_point((float)(x + w), (float)(y + h), &px[3], &py[3]);
}

/* ------------------------------------------------------------ primitivas */

void gfx_rect(int x, int y, int w, int h, unsigned int color)
{
    if (w <= 0 || h <= 0) return;
    set_tex(0);
    if (xf_on) {
        float px[4], py[4];
        VertCF *v = (VertCF *)sceGuGetMemory(6 * sizeof(VertCF));
        int i;
        xf_corners(x, y, w, h, px, py);
        for (i = 0; i < 6; i++) {
            v[i].color = color;
            v[i].x = px[QUAD_CORNER[i]];
            v[i].y = py[QUAD_CORNER[i]];
            v[i].z = 0.0f;
        }
        sceGuDrawArray(GU_TRIANGLES, FMT_CF, 6, 0, v);
    } else {
        VertC *v = (VertC *)sceGuGetMemory(2 * sizeof(VertC));
        x += sh_x;
        y += sh_y;
        v[0].color = color; v[0].x = (short)x;       v[0].y = (short)y;       v[0].z = 0;
        v[1].color = color; v[1].x = (short)(x + w); v[1].y = (short)(y + h); v[1].z = 0;
        sceGuDrawArray(GU_SPRITES, FMT_C, 2, 0, v);
    }
}

void gfx_frame(int x, int y, int w, int h, int t, unsigned int color)
{
    gfx_rect(x, y, w, t, color);
    gfx_rect(x, y + h - t, w, t, color);
    gfx_rect(x, y + t, t, h - 2 * t, color);
    gfx_rect(x + w - t, y + t, t, h - 2 * t, color);
}

/* Degradado vertical. Es para fondos: no le afectan ni el temblor ni la transformacion. */
void gfx_grad(int x, int y, int w, int h, unsigned int top, unsigned int bottom)
{
    VertC *v;
    if (w <= 0 || h <= 0) return;
    set_tex(0);
    v = (VertC *)sceGuGetMemory(4 * sizeof(VertC));
    v[0].color = top;    v[0].x = (short)x;       v[0].y = (short)y;       v[0].z = 0;
    v[1].color = top;    v[1].x = (short)(x + w); v[1].y = (short)y;       v[1].z = 0;
    v[2].color = bottom; v[2].x = (short)x;       v[2].y = (short)(y + h); v[2].z = 0;
    v[3].color = bottom; v[3].x = (short)(x + w); v[3].y = (short)(y + h); v[3].z = 0;
    sceGuDrawArray(GU_TRIANGLE_STRIP, FMT_C, 4, 0, v);
}

/* ------------------------------------------------------------------ texto */

/* Decodifica el siguiente caracter (UTF-8 basico) y devuelve su glifo */
static int next_glyph(const unsigned char **ps)
{
    const unsigned char *s = *ps;
    int c = *s++;
    int g = '?' - 32;

    if (c >= 1 && c <= ICON_COUNT) {
        g = GL_ICON0 + c - 1;
    } else if (c < 0x80) {
        g = (c >= 32) ? c - 32 : 0;
    } else if (c == 0xC3 && *s) {
        int d = *s++;
        switch (d) {
        case 0xA1: g = GL_A_ACUTE; break;
        case 0xA9: g = GL_E_ACUTE; break;
        case 0xAD: g = GL_I_ACUTE; break;
        case 0xB3: g = GL_O_ACUTE; break;
        case 0xBA: g = GL_U_ACUTE; break;
        case 0xB1: g = GL_N_TILDE; break;
        case 0x91: g = GL_N_TILDE_UP; break;
        case 0xBC: g = GL_U_DIAER; break;
        case 0x81: g = 'A' - 32; break;
        case 0x89: g = 'E' - 32; break;
        case 0x8D: g = 'I' - 32; break;
        case 0x93: g = 'O' - 32; break;
        case 0x9A: g = 'U' - 32; break;
        default: break;
        }
    } else if (c == 0xC2 && *s) {
        int d = *s++;
        if (d == 0xBF) g = GL_Q_INV;
        else if (d == 0xA1) g = GL_EX_INV;
        else g = 0;
    } else {
        while ((*s & 0xC0) == 0x80) s++;
    }
    *ps = s;
    return g;
}

static int glyph_adv(int g)
{
    return (g >= GL_ICON0) ? ADV_ICON : ADV;
}

int gfx_text_width(const char *s, int scale)
{
    const unsigned char *p = (const unsigned char *)s;
    int w = 0;
    while (*p) w += glyph_adv(next_glyph(&p));
    return w * scale;
}

/* Glifos pasados por la transformacion: dos triangulos por letra */
static int draw_glyphs_xf(int x, int y, int scale, unsigned int color, const unsigned char *p, const unsigned char *end)
{
    int max = (int)(end - p);
    int n = 0, x0 = x, i;
    VertTF *v = (VertTF *)sceGuGetMemory(max * 6 * sizeof(VertTF));

    while (p < end && *p) {
        int g = next_glyph(&p);
        if (g != 0) {
            float u0 = (float)((g % 16) * CELL_W), v0 = (float)((g / 16) * CELL_H);
            float px[4], py[4], tu[4], tv[4];
            xf_corners(x, y, QUAD_W * scale, QUAD_H * scale, px, py);
            tu[0] = u0;          tv[0] = v0;
            tu[1] = u0 + QUAD_W; tv[1] = v0;
            tu[2] = u0;          tv[2] = v0 + QUAD_H;
            tu[3] = u0 + QUAD_W; tv[3] = v0 + QUAD_H;
            for (i = 0; i < 6; i++) {
                VertTF *q = &v[n * 6 + i];
                q->u = tu[QUAD_CORNER[i]];
                q->v = tv[QUAD_CORNER[i]];
                q->color = color;
                q->x = px[QUAD_CORNER[i]];
                q->y = py[QUAD_CORNER[i]];
                q->z = 0.0f;
            }
            n++;
        }
        x += glyph_adv(g) * scale;
    }
    if (n > 0) {
        set_tex(1);
        bind_tex(0);
        sceGuDrawArray(GU_TRIANGLES, FMT_TF, n * 6, 0, v);
    }
    return x - x0;
}

static int draw_glyphs(int x, int y, int scale, unsigned int color, const unsigned char *p, const unsigned char *end)
{
    int max = (int)(end - p);
    int n = 0, x0 = x;
    VertT *v;

    if (max <= 0) return 0;
    if (xf_on) return draw_glyphs_xf(x, y, scale, color, p, end);

    v = (VertT *)sceGuGetMemory(max * 2 * sizeof(VertT));
    while (p < end && *p) {
        int g = next_glyph(&p);
        if (g != 0) {
            short u = (short)((g % 16) * CELL_W);
            short w = (short)((g / 16) * CELL_H);
            VertT *a = &v[n * 2], *b = &v[n * 2 + 1];
            a->u = u;                    a->v = w;
            a->color = color;
            a->x = (short)(x + sh_x);    a->y = (short)(y + sh_y);    a->z = 0;
            b->u = (short)(u + QUAD_W);  b->v = (short)(w + QUAD_H);
            b->color = color;
            b->x = (short)(x + sh_x + QUAD_W * scale);
            b->y = (short)(y + sh_y + QUAD_H * scale);
            b->z = 0;
            n++;
        }
        x += glyph_adv(g) * scale;
    }
    if (n > 0) {
        set_tex(1);
        bind_tex(0);
        sceGuDrawArray(GU_SPRITES, FMT_T, n * 2, 0, v);
    }
    return x - x0;
}

int gfx_text(int x, int y, int scale, unsigned int color, const char *s)
{
    const unsigned char *p = (const unsigned char *)s;
    return draw_glyphs(x, y, scale, color, p, p + strlen(s));
}

void gfx_text_center(int cx, int y, int scale, unsigned int color, const char *s)
{
    gfx_text(cx - gfx_text_width(s, scale) / 2, y, scale, color, s);
}

void gfx_text_right(int rx, int y, int scale, unsigned int color, const char *s)
{
    gfx_text(rx - gfx_text_width(s, scale), y, scale, color, s);
}

/* Texto con salto de linea por palabras. Devuelve el numero de lineas. */
int gfx_text_wrap(int x, int y, int maxw, int scale, unsigned int color, const char *s)
{
    const unsigned char *p = (const unsigned char *)s;
    int cx = 0, lines = 1;
    int space = ADV * scale;

    while (*p) {
        const unsigned char *start, *q;
        int w = 0;
        while (*p == ' ') p++;
        if (!*p) break;
        start = p;
        q = p;
        while (*q && *q != ' ') w += glyph_adv(next_glyph(&q)) * scale;
        if (cx > 0 && cx + space + w > maxw) {
            cx = 0;
            y += CELL_H * scale;
            lines++;
        }
        if (cx > 0) cx += space;
        draw_glyphs(x + cx, y, scale, color, start, q);
        cx += w;
        p = q;
    }
    return lines;
}

/* ---------------------------------------------------------- ilustraciones */

void gfx_art_clear(void)
{
    memset(art_tex, 0, sizeof(art_tex));
}

void gfx_art_put(int cell, int x, int y, unsigned int color)
{
    int cx = (cell % ART_COLS) * ART_PITCH + 1 + x;
    int cy = (cell / ART_COLS) * ART_PITCH + 1 + y;
    if (x < 0 || y < 0 || x >= ART_SIZE || y >= ART_SIZE || cy >= ART_H) return;
    art_tex[cy * ART_W + cx] = color;
}

void gfx_art_commit(void)
{
    sceKernelDcacheWritebackInvalidateAll();
}

/* Dibuja la ilustracion `cell` (16x16) ampliada `scale` veces y tenida con `tint` */
void gfx_art(int cell, int x, int y, int scale, unsigned int tint)
{
    int u0 = (cell % ART_COLS) * ART_PITCH + 1, v0 = (cell / ART_COLS) * ART_PITCH + 1;
    int w = ART_SIZE * scale;

    set_tex(1);
    bind_tex(1);
    if (xf_on) {
        float px[4], py[4], tu[4], tv[4];
        VertTF *v = (VertTF *)sceGuGetMemory(6 * sizeof(VertTF));
        int i;
        xf_corners(x, y, w, w, px, py);
        tu[0] = (float)u0;              tv[0] = (float)v0;
        tu[1] = (float)(u0 + ART_SIZE); tv[1] = (float)v0;
        tu[2] = (float)u0;              tv[2] = (float)(v0 + ART_SIZE);
        tu[3] = (float)(u0 + ART_SIZE); tv[3] = (float)(v0 + ART_SIZE);
        for (i = 0; i < 6; i++) {
            v[i].u = tu[QUAD_CORNER[i]];
            v[i].v = tv[QUAD_CORNER[i]];
            v[i].color = tint;
            v[i].x = px[QUAD_CORNER[i]];
            v[i].y = py[QUAD_CORNER[i]];
            v[i].z = 0.0f;
        }
        sceGuDrawArray(GU_TRIANGLES, FMT_TF, 6, 0, v);
    } else {
        VertT *v = (VertT *)sceGuGetMemory(2 * sizeof(VertT));
        v[0].u = (short)u0;              v[0].v = (short)v0;
        v[0].color = tint;
        v[0].x = (short)(x + sh_x);      v[0].y = (short)(y + sh_y);      v[0].z = 0;
        v[1].u = (short)(u0 + ART_SIZE); v[1].v = (short)(v0 + ART_SIZE);
        v[1].color = tint;
        v[1].x = (short)(x + sh_x + w);  v[1].y = (short)(y + sh_y + w);  v[1].z = 0;
        sceGuDrawArray(GU_SPRITES, FMT_T, 2, 0, v);
    }
}

void gfx_icon(int x, int y, int scale, int icon, unsigned int color)
{
    unsigned char s[2];
    s[0] = (unsigned char)(icon + 1);
    s[1] = 0;
    /* el dibujo empieza en la fila 1 de la celda */
    draw_glyphs(x, y - scale, scale, color, s, s + 1);
}
