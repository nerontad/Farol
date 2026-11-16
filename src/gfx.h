/* FAROL - capa de dibujo 2D sobre sceGu */
#ifndef GFX_H
#define GFX_H

#define SCR_W 480
#define SCR_H 272

/* Colores en formato ABGR (el nativo de la PSP) */
#define RGBA(r, g, b, a) (((unsigned int)(a) << 24) | ((unsigned int)(b) << 16) | ((unsigned int)(g) << 8) | (unsigned int)(r))
#define RGB(r, g, b) RGBA(r, g, b, 255)

/* Simbolos que se pueden incrustar en cualquier texto (siempre 3 digitos octales) */
#define G_SPADE    "\001"
#define G_HEART    "\002"
#define G_CLUB     "\003"
#define G_DIAMOND  "\004"
#define G_CROSS    "\005"
#define G_CIRCLE   "\006"
#define G_SQUARE   "\007"
#define G_TRIANGLE "\010"

enum { ICON_SPADE, ICON_HEART, ICON_CLUB, ICON_DIAMOND, ICON_CROSS, ICON_CIRCLE, ICON_SQUARE, ICON_TRIANGLE, ICON_COUNT };

void gfx_init(void);
void gfx_begin(unsigned int clearColor);
void gfx_end(void);
/* Funcion que se llama en cada fotograma, entre el final del dibujo y el cambio de buffer */
void gfx_set_frame_hook(void (*fn)(void));
#ifdef AUTOTEST
void gfx_dump(const char *name);   /* vuelca el ultimo fotograma a ms0:/farol_<name>.raw */
#endif

/* Efectos. El temblor desplaza todo lo que se dibuje despues (menos gfx_grad);
   la transformacion gira y escala alrededor de (cx, cy) hasta gfx_xform_off. */
void gfx_shake(int dx, int dy);
void gfx_xform(float cx, float cy, float angle, float scale);
void gfx_xform_off(void);
void gfx_additive(int on);

void gfx_rect(int x, int y, int w, int h, unsigned int color);
void gfx_frame(int x, int y, int w, int h, int t, unsigned int color);
void gfx_grad(int x, int y, int w, int h, unsigned int top, unsigned int bottom);

int  gfx_text_width(const char *s, int scale);
int  gfx_text(int x, int y, int scale, unsigned int color, const char *s);
void gfx_text_center(int cx, int y, int scale, unsigned int color, const char *s);
void gfx_text_right(int rx, int y, int scale, unsigned int color, const char *s);
int  gfx_text_wrap(int x, int y, int maxw, int scale, unsigned int color, const char *s);
void gfx_icon(int x, int y, int scale, int icon, unsigned int color);

/* Ilustraciones: una textura aparte con celdas de 16x16 que rellena el juego al arrancar */
void gfx_art_clear(void);
void gfx_art_put(int cell, int x, int y, unsigned int color);
void gfx_art_commit(void);
void gfx_art(int cell, int x, int y, int scale, unsigned int tint);

#endif
