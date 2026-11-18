/* FAROL - pantallas y flujo del juego */
#include <math.h>
#include <stdio.h>
#include <string.h>
#include "gfx.h"
#include "input.h"
#include "game.h"
#include "synth.h"
#include "save.h"
#include "art.h"
#include "ui.h"

/* Texto en el idioma activo: T("espanol", "english") */
#define T(es, en) (LANG ? (en) : (es))

/* ---------------------------------------------------------------- colores */

#define COL_BG_TOP   RGB(52, 22, 38)
#define COL_BG_BOT   RGB(16, 8, 14)
#define COL_FELT_TOP RGB(24, 82, 66)
#define COL_FELT_BOT RGB(10, 42, 36)
#define COL_PANEL    RGB(30, 20, 26)
#define COL_BOX      RGB(48, 34, 42)
#define COL_BRASS    RGB(204, 160, 78)
#define COL_TEXT     RGB(240, 232, 214)
#define COL_DIM      RGB(150, 138, 130)
#define COL_CHIPS    RGB(70, 146, 226)
#define COL_MULT     RGB(220, 74, 64)
#define COL_MONEY    RGB(240, 200, 72)
#define COL_GOOD     RGB(110, 200, 120)
#define COL_HOT      RGB(255, 140, 40)
#define COL_WHITE    RGB(255, 255, 255)
#define COL_SHADE    RGBA(0, 0, 0, 170)

static const unsigned int SUIT_COL[4] = {
    RGB(30, 30, 40), RGB(204, 36, 48), RGB(22, 112, 70), RGB(222, 118, 18)
};
/* los mismos palos, pero en tonos que se ven sobre fondo oscuro (confeti) */
static const unsigned int SUIT_BRIGHT[4] = {
    RGB(226, 226, 240), RGB(240, 70, 80), RGB(70, 206, 130), RGB(250, 156, 46)
};
static const unsigned int MARK_BG[MARK_COUNT] = {
    RGB(244, 238, 224), RGB(200, 208, 224), RGB(255, 204, 204), RGB(196, 238, 246), RGB(250, 226, 130)
};
static const unsigned int MARK_BAND[MARK_COUNT] = {
    0, RGB(118, 128, 150), RGB(200, 40, 60), RGB(40, 168, 200), RGB(208, 148, 20)
};
static const unsigned int RARITY_COL[RAR_COUNT] = {
    RGB(240, 232, 214), RGB(116, 176, 250), RGB(206, 130, 244)
};
static const unsigned int NOTCH_COL[NOTCH_COUNT] = {
    0, RGB(150, 84, 204), RGB(44, 74, 150), RGB(30, 150, 92)
};
#define COL_TRICK RGB(170, 130, 230)
static const char *const RANK_STR[15] = {
    "", "", "2", "3", "4", "5", "6", "7", "8", "9", "10", "J", "Q", "K", "A"
};
static const char *const SUIT_GLYPH[4] = { G_SPADE, G_HEART, G_CLUB, G_DIAMOND };

/* ----------------------------------------------------------------- layout */

#define AREA_X  132
#define AREA_W  346
#define FELT_Y  44
#define FELT_H  208
#define CARD_W  40
#define CARD_H  56
#define HAND_Y  168
#define PLAY_Y  74
#define PILE_X  (SCR_W + 8)   /* las cartas nuevas entran desde fuera de la pantalla */
#define PILE_Y  (HAND_Y - 24)
#define SLOT_W  40
#define RELIC_X(s) (134 + (s) * 43)
#define TRICK_X(k) (354 + (k) * 42)
#define TRICK_ART(id) (R_COUNT + (id))    /* celda de la ilustracion de un truco */
#define BOSS_ART(b) (R_COUNT + T_COUNT + (b) - 1)    /* celda del retrato de un rival */

/* ----------------------------------------------------------------- estado */

enum { ST_TITLE, ST_DECK, ST_COLLECTION, ST_OPTIONS, ST_INTRO, ST_PLAY, ST_WIN, ST_SHOP, ST_GAMEOVER, ST_VICTORY };
enum { PH_SELECT, PH_TOCENTER, PH_EVENTS, PH_TOTAL, PH_COUNT, PH_END };
enum { ROW_OFFER, ROW_REROLL, ROW_SELL, ROW_TRICK, ROW_NEXT };

typedef struct { int kind, idx; } ShopRow;

static Game g;
static int state, phase, frame, timer, stateAge;
static int cursor;
static int showInfo, paused, pauseSel;
static ScoreResult res;
static int evIdx, evTimer;
static double shownChips, shownMult;
static Reward reward;
static int shopSel;
static const char *toastMsg;
static int toastTimer;
static int titleSel, deckSel, diffSel, colSel, colPage;
static int infoPage, endSel;
static int newDiff;                  /* dificultad desbloqueada al ganar, o 0 */
static int update_info(void);
static int trickMode, trickSel;      /* menu de trucos abierto durante la ronda */
static int newUnlock;                /* baraja desbloqueada al acabar la partida, o -1 */
static int optSel;                   /* fila elegida en el menu de opciones */
static int seedEdit, seedCustom, seedDigit;
static unsigned int seedValue;       /* semilla elegida a mano para la proxima partida */
static int infoSel, infoHeld;        /* amuleto elegido (y cogido) en la pagina de informacion */

/* --------------------------------------------------- estado de los efectos */

#define MAX_PART  128
#define MAX_POP   10
#define MAX_GHOST 16
#define MAX_SCHED 24

typedef struct {
    float x, y, vx, vy, grav;
    int life, maxLife;
    unsigned char kind, size, add;      /* kind 0 = cuadrado; 1..4 = palo */
    unsigned int color;
} Particle;

typedef struct { float x, y; int age, life; unsigned int color; char text[48]; } Popup;
typedef struct { Card c; float x, y, vx, vy, rot, vr; int life; } Ghost;
typedef struct { int delay, wave; float freq, vol, secs, slide; } Sched;

static Particle parts[MAX_PART];
static Popup pops[MAX_POP];
static Ghost ghosts[MAX_GHOST];
static Sched sched[MAX_SCHED];

/* posicion, velocidad, giro y "salto" de cada carta, por id */
static float cpx[MAX_DECK], cpy[MAX_DECK], cvx[MAX_DECK], cvy[MAX_DECK], crot[MAX_DECK], cpop[MAX_DECK];
static short cdelay[MAX_DECK];
static unsigned char cplaced[MAX_DECK];

static float shake, heat, flash, bgT;
static int shakeX, shakeY;
static float chipsPulse, multPulse, scorePulse, moneyPulse, totalPop, selPulse;
static float relicPop[MAX_RELICS];
static double dispScore;
static float dispMoney;
static int moneyHidden;            /* dinero ya ganado que aun no se ha "contado" en pantalla */
static int handGone, hotHand;
static float bannerY, bannerVy;
static unsigned int vseed = 0x2468ACEu;
static float SEMI[49];

/* ------------------------------------------------------------- utilidades */

static void fmt_ll(char *out, long long v)
{
    char tmp[40];
    int n = 0, d = 0, i;
    if (v < 0) v = 0;
    do {
        if (d && d % 3 == 0) tmp[n++] = LANG ? ',' : '.';
        tmp[n++] = (char)('0' + (int)(v % 10));
        v /= 10;
        d++;
    } while (v > 0 && n < 36);
    for (i = 0; i < n; i++) out[i] = tmp[n - 1 - i];
    out[n] = 0;
}

static void fmt_mult(char *out, double m)
{
    long long t = (long long)(m * 10.0 + 0.5);
    fmt_ll(out, t / 10);
    if (t % 10) {
        int n = (int)strlen(out);
        out[n] = LANG ? '.' : ',';
        out[n + 1] = (char)('0' + (int)(t % 10));
        out[n + 2] = 0;
    }
}

/* Azar solo para lo visual: asi los efectos no alteran el azar de la partida */
static int vrand(void)
{
    vseed = vseed * 1103515245u + 12345u;
    return (int)((vseed >> 16) & 0x7FFF);
}
static float vrf(void) { return (float)vrand() / 32767.0f; }
static float vrs(void) { return vrf() * 2.0f - 1.0f; }

static float clampf(float v, float lo, float hi)
{
    return v < lo ? lo : (v > hi ? hi : v);
}

/* Llega a 1 pasandose un poco y volviendo */
static float ease_back(float t)
{
    float u = clampf(t, 0.0f, 1.0f) - 1.0f;
    return 1.0f + 2.70158f * u * u * u + 1.70158f * u * u;
}

static unsigned int col_alpha(unsigned int c, float a)
{
    int al = (int)((float)((c >> 24) & 255) * clampf(a, 0.0f, 1.0f));
    return (c & 0x00FFFFFFu) | ((unsigned int)al << 24);
}

static unsigned int col_lerp(unsigned int a, unsigned int b, float t)
{
    unsigned int out = 0;
    int sh;
    t = clampf(t, 0.0f, 1.0f);
    for (sh = 0; sh < 32; sh += 8) {
        int ca = (int)((a >> sh) & 255), cb = (int)((b >> sh) & 255);
        out |= (unsigned int)(ca + (int)((float)(cb - ca) * t)) << sh;
    }
    return out;
}

/* Texto centrado que baja a escala 1 si no cabe a escala 2 */
static void text_fit_center(int cx, int y, int maxw, unsigned int color, const char *s)
{
    if (gfx_text_width(s, 2) <= maxw) gfx_text_center(cx, y, 2, color, s);
    else gfx_text_center(cx, y + 5, 1, color, s);
}

static void toast(const char *m)
{
    toastMsg = m;
    toastTimer = 110;
}

/* ----------------------------------------------------------------- sonido */

static const int PENT[5] = { 0, 2, 4, 7, 9 };

/* Escala pentatonica: cada paso sube, y suena bien se toque lo que se toque */
static float scale_note(int n)
{
    if (n < 0) n = 0;
    if (n > 14) n = 14;
    return SEMI[12 * (n / 5) + PENT[n % 5]];
}

static void snd_later(int delay, int wave, float freq, float vol, float secs, float slide)
{
    int i;
    if (delay <= 0) {
        synth_note(wave, freq, vol, secs, slide);
        return;
    }
    for (i = 0; i < MAX_SCHED; i++) {
        if (sched[i].delay <= 0) {
            sched[i].delay = delay;
            sched[i].wave = wave;
            sched[i].freq = freq;
            sched[i].vol = vol;
            sched[i].secs = secs;
            sched[i].slide = slide;
            return;
        }
    }
}

static void snd_update(void)
{
    int i;
    for (i = 0; i < MAX_SCHED; i++) {
        if (sched[i].delay > 0 && --sched[i].delay == 0)
            synth_note(sched[i].wave, sched[i].freq, sched[i].vol, sched[i].secs, sched[i].slide);
    }
}

static void snd_move(void)      { synth_note(W_SQUARE, 1200.0f, 0.030f, 0.025f, 0.0f); }
static void snd_select(int on)  { synth_note(W_TRI, on ? 587.0f : 494.0f, 0.11f, 0.08f, on ? 80.0f : -80.0f); }
static void snd_error(void)     { synth_note(W_SQUARE, 116.0f, 0.09f, 0.16f, 0.0f); synth_note(W_SQUARE, 110.0f, 0.07f, 0.16f, 0.0f); }
static void snd_whoosh(void)    { synth_note(W_NOISE, 1800.0f, 0.07f, 0.16f, 60.0f); }
static void snd_discard(void)   { synth_note(W_NOISE, 4000.0f, 0.07f, 0.14f, -70.0f); }
static void snd_null(void)      { synth_note(W_TRI, 98.0f, 0.16f, 0.20f, -30.0f); }
static void snd_tick(float t)   { synth_note(W_SQUARE, 700.0f + 900.0f * clampf(t, 0.0f, 1.0f), 0.028f, 0.02f, 0.0f); }

static void snd_deal(int k)
{
    synth_note(W_NOISE, 5200.0f + (float)k * 350.0f, 0.045f, 0.03f, 0.0f);
    synth_note(W_TRI, 260.0f + (float)k * 26.0f, 0.05f, 0.05f, 0.0f);
}

static void snd_chips(int n)
{
    float f = 330.0f * scale_note(n);
    synth_note(W_TRI, f, 0.15f, 0.18f, 0.0f);
    synth_note(W_SINE, f * 2.0f, 0.05f, 0.12f, 0.0f);
}

static void snd_mult(int n)
{
    float f = 330.0f * scale_note(n);
    synth_note(W_SQUARE, f, 0.09f, 0.16f, 0.0f);
    synth_note(W_TRI, f * 1.4983f, 0.09f, 0.18f, 0.0f);
}

static void snd_xmult(int n)
{
    float f = 330.0f * scale_note(n);
    synth_note(W_SQUARE, f, 0.10f, 0.30f, 0.0f);
    synth_note(W_SQUARE, f * 1.4983f, 0.08f, 0.30f, 0.0f);
    synth_note(W_TRI, f * 2.0f, 0.10f, 0.35f, 0.0f);
    synth_note(W_NOISE, 3000.0f, 0.08f, 0.12f, -40.0f);
}

static void snd_money(void)
{
    synth_note(W_SINE, 1568.0f, 0.12f, 0.12f, 0.0f);
    snd_later(5, W_SINE, 2093.0f, 0.12f, 0.20f, 0.0f);
}

static void snd_buy(void)
{
    synth_note(W_NOISE, 8000.0f, 0.05f, 0.03f, 0.0f);
    synth_note(W_SINE, 1319.0f, 0.11f, 0.10f, 0.0f);
    snd_later(4, W_SINE, 1760.0f, 0.11f, 0.25f, 0.0f);
}

static void snd_total(int hot)
{
    static const int steps[4] = { 0, 4, 7, 12 };
    int i;
    for (i = 0; i < 4; i++)
        snd_later(i * 3, W_TRI, 392.0f * SEMI[steps[i]], 0.13f, 0.25f, 0.0f);
    if (hot) {
        snd_later(12, W_SQUARE, 392.0f * SEMI[19], 0.09f, 0.45f, 0.0f);
        snd_later(12, W_NOISE, 6000.0f, 0.08f, 0.40f, -20.0f);
    }
}

static void snd_win(void)
{
    static const int steps[5] = { 0, 4, 7, 12, 16 };
    int i;
    for (i = 0; i < 5; i++)
        snd_later(i * 5, W_TRI, 523.25f * SEMI[steps[i]], 0.14f, 0.30f, 0.0f);
    snd_later(27, W_SINE, 523.25f * SEMI[12], 0.09f, 0.70f, 0.0f);
    snd_later(27, W_SINE, 523.25f * SEMI[16], 0.08f, 0.70f, 0.0f);
    snd_later(27, W_SINE, 523.25f * SEMI[19], 0.08f, 0.70f, 0.0f);
}

static void snd_lose(void)
{
    static const float f[4] = { 392.0f, 329.6f, 261.6f, 196.0f };
    int i;
    for (i = 0; i < 4; i++) snd_later(i * 9, W_TRI, f[i], 0.14f, 0.45f, -6.0f);
}

static void snd_start(void)
{
    int i;
    for (i = 0; i < 4; i++) snd_later(i * 3, W_TRI, 330.0f * scale_note(i * 2), 0.12f, 0.20f, 0.0f);
}

static void snd_boss(void)
{
    synth_note(W_SQUARE, 82.4f, 0.10f, 0.70f, -3.0f);
    synth_note(W_SQUARE, 87.3f, 0.08f, 0.70f, -3.0f);
}

/* -------------------------------------------------------------- particulas */

static void part_add(float x, float y, float vx, float vy, float grav, int life,
                     int kind, int size, int add, unsigned int color)
{
    int i;
    for (i = 0; i < MAX_PART; i++) {
        Particle *p = &parts[i];
        if (p->life > 0) continue;
        p->x = x; p->y = y; p->vx = vx; p->vy = vy; p->grav = grav;
        p->life = p->maxLife = life;
        p->kind = (unsigned char)kind;
        p->size = (unsigned char)size;
        p->add = (unsigned char)add;
        p->color = color;
        return;
    }
}

static void sparks(float x, float y, int n, unsigned int color, float speed)
{
    int i;
    for (i = 0; i < n; i++)
        part_add(x, y, vrs() * speed, vrs() * speed - speed * 0.3f, 0.12f,
                 14 + vrand() % 14, 0, 2 + vrand() % 2, 1, color);
}

static void confetti(int n)
{
    int i;
    for (i = 0; i < n; i++) {
        int suit = vrand() % 4;
        part_add(vrf() * SCR_W, -10.0f - vrf() * 70.0f, vrs() * 1.2f, 1.5f + vrf() * 2.5f, 0.03f,
                 90 + vrand() % 40, 1 + suit, 2 + vrand() % 2, 0, SUIT_BRIGHT[suit]);
    }
}

static void embers(float x0, float x1, float y, int n)
{
    int i;
    for (i = 0; i < n; i++)
        part_add(x0 + vrf() * (x1 - x0), y, vrs() * 0.4f, -0.8f - vrf() * 1.4f, -0.02f,
                 18 + vrand() % 20, 0, 2, 1, (vrand() & 1) ? RGB(255, 176, 44) : RGB(255, 96, 32));
}

static void draw_particles(void)
{
    int pass, i;
    for (pass = 0; pass < 2; pass++) {
        if (pass) gfx_additive(1);
        for (i = 0; i < MAX_PART; i++) {
            const Particle *p = &parts[i];
            unsigned int col;
            if (p->life <= 0 || p->add != pass) continue;
            col = col_alpha(p->color, (float)p->life / (float)p->maxLife * 1.6f);
            if (p->kind == 0)
                gfx_rect((int)p->x - p->size / 2, (int)p->y - p->size / 2, p->size, p->size, col);
            else
                gfx_icon((int)p->x, (int)p->y, p->size, p->kind - 1, col);
        }
        if (pass) gfx_additive(0);
    }
}

static void popup_add(float x, float y, unsigned int color, const char *text)
{
    int i, best = 0;
    for (i = 0; i < MAX_POP; i++) {
        if (pops[i].age >= pops[i].life) { best = i; break; }
        if (pops[i].age > pops[best].age) best = i;
    }
    pops[best].x = x;
    pops[best].y = y;
    pops[best].age = 0;
    pops[best].life = 36;
    pops[best].color = color;
    snprintf(pops[best].text, sizeof(pops[best].text), "%s", text);
}

static void draw_popups(void)
{
    int i;
    for (i = 0; i < MAX_POP; i++) {
        const Popup *p = &pops[i];
        float a, s;
        int w, x, y;
        if (p->age >= p->life) continue;
        a = p->age < p->life - 10 ? 1.0f : (float)(p->life - p->age) / 10.0f;
        s = 1.0f + (p->age < 7 ? (float)(7 - p->age) * 0.13f : 0.0f);
        w = gfx_text_width(p->text, 1);
        x = (int)p->x - w / 2;
        y = (int)(p->y - (float)p->age * 0.45f);
        if (x < AREA_X + 3) x = AREA_X + 3;
        if (x + w > SCR_W - 5) x = SCR_W - 5 - w;
        if (s > 1.01f) gfx_xform((float)(x + w / 2), (float)(y + 5), 0.0f, s);
        gfx_rect(x - 3, y - 1, w + 5, 12, RGBA(0, 0, 0, (int)(205.0f * a)));
        gfx_text(x, y, 1, col_alpha(p->color, a), p->text);
        gfx_xform_off();
    }
}

static void ghost_add(Card c, float vx, float vy, float vr)
{
    int i;
    for (i = 0; i < MAX_GHOST; i++) {
        Ghost *gh = &ghosts[i];
        if (gh->life > 0) continue;
        gh->c = c;
        gh->x = cpx[c.id];
        gh->y = cpy[c.id];
        gh->rot = crot[c.id];
        gh->vx = vx;
        gh->vy = vy;
        gh->vr = vr;
        gh->life = 50;
        return;
    }
}

/* Todo lo que se mueve solo, se este en la pantalla que se este */
static void fx_update(void)
{
    int i;
    float target, d;

    snd_update();
    bgT += 1.0f + heat * 5.0f;
    heat *= 0.988f;
    synth_set_intensity(heat);

    if (shake > 0.4f) {
        shakeX = (int)(vrs() * shake);
        shakeY = (int)(vrs() * shake);
        shake *= 0.86f;
    } else {
        shake = 0.0f;
        shakeX = shakeY = 0;
    }
    flash *= 0.88f;
    chipsPulse *= 0.84f;
    multPulse *= 0.84f;
    scorePulse *= 0.86f;
    moneyPulse *= 0.86f;
    totalPop *= 0.86f;
    selPulse *= 0.80f;
    for (i = 0; i < MAX_RELICS; i++) relicPop[i] *= 0.88f;

    /* el dinero en pantalla va rodando hasta el valor real */
    target = (float)(g.money - moneyHidden);
    d = target - dispMoney;
    if (d > -0.5f && d < 0.5f) {
        dispMoney = target;
    } else {
        dispMoney += d * 0.22f + (d > 0.0f ? 0.4f : -0.4f);
        if ((d > 0.0f && dispMoney > target) || (d < 0.0f && dispMoney < target)) dispMoney = target;
        if ((frame & 3) == 0) snd_tick(0.8f);
    }

    for (i = 0; i < MAX_PART; i++) {
        Particle *p = &parts[i];
        if (p->life <= 0) continue;
        p->x += p->vx;
        p->y += p->vy;
        p->vy += p->grav;
        p->life--;
    }
    for (i = 0; i < MAX_POP; i++)
        if (pops[i].age < pops[i].life) pops[i].age++;
    for (i = 0; i < MAX_GHOST; i++) {
        Ghost *gh = &ghosts[i];
        if (gh->life <= 0) continue;
        gh->x += gh->vx;
        gh->y += gh->vy;
        gh->vy += 0.55f;
        gh->rot += gh->vr;
        gh->life--;
    }
}

/* ---------------------------------------------------------------- dibujo */

/* Fondo: degradado que se calienta con la accion y palos flotando despacio */
static void draw_bg(void)
{
    int k;
    gfx_grad(0, 0, SCR_W, SCR_H, col_lerp(COL_BG_TOP, RGB(112, 34, 30), heat * 0.55f), COL_BG_BOT);
    for (k = 0; k < 12; k++) {
        float sp = 0.10f + (float)(k % 4) * 0.045f;
        float y = 300.0f - fmodf(bgT * sp + (float)k * 61.0f, 370.0f);
        float x = (float)((k * 97) % 500) - 10.0f + sinf(bgT * 0.008f + (float)k * 1.3f) * 16.0f;
        int sc = 3 + (k % 3);
        gfx_xform(x + (float)sc * 3.5f, y + (float)sc * 3.5f, sinf(bgT * 0.006f + (float)k) * 0.22f, 1.0f);
        gfx_icon((int)x, (int)y, sc, k % 4, RGBA(255, 214, 170, 18 + (int)(clampf(heat, 0.0f, 1.0f) * 24.0f)));
        gfx_xform_off();
    }
}

static void draw_panel(int x, int y, int w, int h)
{
    gfx_rect(x, y, w, h, COL_PANEL);
    gfx_frame(x, y, w, h, 1, COL_BRASS);
}

static void draw_card(int x, int y, Card c, int dim)
{
    unsigned int bg = MARK_BG[c.mark], ink = SUIT_COL[c.suit], edge = RGB(40, 30, 30);
    if (dim) {
        bg = RGB(124, 120, 116);
        ink = RGB(72, 70, 68);
    }
    gfx_rect(x + 2, y + 3, CARD_W, CARD_H, RGBA(0, 0, 0, 80));
    gfx_rect(x + 1, y, CARD_W - 2, CARD_H, edge);
    gfx_rect(x, y + 1, CARD_W, CARD_H - 2, edge);
    gfx_rect(x + 1, y + 1, CARD_W - 2, CARD_H - 2, bg);
    if (c.mark && !dim) gfx_rect(x + 1, y + CARD_H - 5, CARD_W - 2, 4, MARK_BAND[c.mark]);
    gfx_text(x + 3, y + 1, 2, ink, RANK_STR[c.rank]);
    if (c.rank != 10) gfx_icon(x + CARD_W - 11, y + 4, 1, c.suit, ink);   /* el 10 no deja sitio */
    gfx_icon(x + 10, y + 26, 3, c.suit, ink);
    if (c.notch && !dim) {
        /* la muesca es un banderin de color en el borde izquierdo */
        gfx_rect(x + 1, y + 20, 5, 9, NOTCH_COL[c.notch]);
        gfx_rect(x + 6, y + 22, 2, 5, NOTCH_COL[c.notch]);
    }
}

/* Carta con giro y escala alrededor de su centro */
static void draw_card_x(float x, float y, float rot, float scale, Card c, int dim, int highlight)
{
    int ix = (int)floorf(x + 0.5f), iy = (int)floorf(y + 0.5f);
    int xf = rot > 0.004f || rot < -0.004f || scale > 1.004f || scale < 0.996f;
    if (xf) gfx_xform((float)ix + CARD_W * 0.5f, (float)iy + CARD_H * 0.5f, rot, scale);
    draw_card(ix, iy, c, dim);
    if (highlight) gfx_frame(ix - 1, iy - 1, CARD_W + 2, CARD_H + 2, 2, COL_MONEY);
    if (xf) gfx_xform_off();
}

/* Las pantallas de panel entran con un pequeno rebote */
static void popin_begin(void)
{
    if (stateAge < 12) gfx_xform(SCR_W * 0.5f, SCR_H * 0.5f, 0.0f, 0.80f + 0.20f * ease_back((float)stateAge / 12.0f));
}

static void draw_flash(void)
{
    if (flash > 0.02f) gfx_rect(0, 0, SCR_W, SCR_H, RGBA(255, 255, 255, (int)(clampf(flash, 0.0f, 1.0f) * 255.0f)));
}

static const char *round_name(void)
{
    if (g.round == 0) return T("Calentamiento", "Warm-up");
    if (g.round == 1) return T("Apuesta fuerte", "High Stakes");
    return BOSS_INFO[g.bossRule].name[LANG];
}

static void boss_desc(char *b, int n)
{
    if (g.bossRule == B_SUIT)
        snprintf(b, n, T("Las cartas de %s (%s) no puntúan.", "The %s (%s) don't score."), SUIT_NAME[LANG][g.bossSuit], SUIT_GLYPH[g.bossSuit]);
    else if (g.bossRule == B_COLOR)
        snprintf(b, n, g.bossSuit ? T("Las cartas rojas (" G_HEART G_DIAMOND ") no puntúan.", "Red cards (" G_HEART G_DIAMOND ") don't score.")
                                  : T("Las cartas negras (" G_SPADE G_CLUB ") no puntúan.", "Black cards (" G_SPADE G_CLUB ") don't score."));
    else
        snprintf(b, n, "%s", BOSS_INFO[g.bossRule].desc[LANG]);
}

static int hand_x(int i, int n)
{
    int step = 43, total;
    if (n > 1 && (n - 1) * step + CARD_W > AREA_W - 8) step = (AREA_W - 8 - CARD_W) / (n - 1);
    total = (n - 1) * step + CARD_W;
    return AREA_X + (AREA_W - total) / 2 + i * step;
}

static int play_x(int i, int n)
{
    int step = 46;
    int total = (n - 1) * step + CARD_W;
    return AREA_X + (AREA_W - total) / 2 + i * step;
}

/* ----------------------------------------------------------- transiciones */

static void set_state(int s)
{
    state = s;
    stateAge = 0;
    timer = 0;
}

static void enter_intro(void)
{
    set_state(ST_INTRO);
    if (g.bossRule != B_NONE) {
        snd_boss();
        shake += 4.0f;
    } else {
        snd_whoosh();
    }
}

/* Prepara la ronda siguiente y guarda: si se apaga la consola, se continua desde aqui */
static void start_intro(void)
{
    game_setup_round(&g);
    save_run(&g, RESUME_INTRO);
    enter_intro();
}

static void enter_shop(void)
{
    shopSel = 0;
    toastTimer = 0;
    set_state(ST_SHOP);
    snd_whoosh();
}

/* Fin de la partida: actualiza records y mira si se ha desbloqueado alguna baraja */
static void end_run(int won)
{
    int before[DECK_COUNT], d, diffBefore = save_profile()->maxDifficulty;
    for (d = 0; d < DECK_COUNT; d++) before[d] = save_deck_unlocked(d);
    if (won) {
        /* la partida sigue viva en modo infinito: queda guardada ya en la tienda siguiente */
        g.endless = 1;
        save_run_won(&g);
        game_shop_enter(&g);
        save_run(&g, RESUME_SHOP);
    } else {
        save_run_ended(&g);
    }
    newUnlock = -1;
    for (d = 0; d < DECK_COUNT; d++)
        if (!before[d] && save_deck_unlocked(d)) newUnlock = d;
    newDiff = save_profile()->maxDifficulty > diffBefore ? save_profile()->maxDifficulty : 0;
}

/* Las cartas recien robadas salen del mazo de una en una */
static void mark_new_cards(void)
{
    int i, k = 0;
    for (i = 0; i < g.handCount; i++) {
        int id = g.hand[i].id;
        if (cplaced[id]) continue;
        cplaced[id] = 1;
        cpx[id] = (float)PILE_X;
        cpy[id] = (float)PILE_Y;
        cvx[id] = cvy[id] = 0.0f;
        crot[id] = 0.6f;
        cpop[id] = 0.0f;
        cdelay[id] = (short)(1 + k * 4);
        k++;
    }
}

static void start_play(void)
{
    game_begin_round(&g);
    memset(cplaced, 0, sizeof(cplaced));
    memset(cdelay, 0, sizeof(cdelay));
    memset(ghosts, 0, sizeof(ghosts));
    mark_new_cards();
    cursor = 0;
    phase = PH_SELECT;
    paused = 0;
    showInfo = 0;
    toastTimer = 0;
    handGone = 0;
    hotHand = 0;
    trickMode = 0;
    dispScore = 0.0;
    set_state(ST_PLAY);
}

static void spring_to(int id, float tx, float ty, float trot)
{
    cvx[id] = (cvx[id] + (tx - cpx[id]) * 0.20f) * 0.62f;
    cvy[id] = (cvy[id] + (ty - cpy[id]) * 0.20f) * 0.62f;
    cpx[id] += cvx[id];
    cpy[id] += cvy[id];
    /* la carta se inclina hacia donde se mueve */
    crot[id] += (trot + clampf(cvx[id] * 0.012f, -0.3f, 0.3f) - crot[id]) * 0.22f;
    cpop[id] *= 0.80f;
}

static void anim_cards(void)
{
    int i, n = g.handCount;
    for (i = 0; i < n; i++) {
        int id = g.hand[i].id;
        float off = (float)i - (float)(n - 1) * 0.5f;
        float ty, tr;
        if (cdelay[id] > 0) {
            if (--cdelay[id] > 0) continue;
            snd_deal(i);
        }
        /* abanico: los extremos caen un poco y se inclinan; todas respiran */
        ty = (float)HAND_Y + off * off * 0.6f + sinf((float)frame * 0.07f + (float)id * 1.7f);
        tr = off * 0.028f + sinf((float)frame * 0.045f + (float)id * 0.9f) * 0.010f;
        if (g.sel[i]) {
            ty -= 14.0f;
            tr *= 0.4f;
        }
        if (i == cursor && phase == PH_SELECT) ty -= 3.0f;
        spring_to(id, (float)hand_x(i, n), ty, tr);
    }
    for (i = 0; i < g.playedCount; i++) {
        int id = g.played[i].id;
        if (cdelay[id] > 0) {
            if (--cdelay[id] > 0) continue;
            snd_deal(i + 4);
        }
        spring_to(id, (float)play_x(i, g.playedCount), (float)(PLAY_Y + (res.eval.scores[i] ? 0 : 10)),
                  sinf((float)frame * 0.05f + (float)i) * 0.012f);
    }
}

/* ----------------------------------------------------------------- titulo */

enum { TI_CONTINUE, TI_NEW, TI_COLLECTION, TI_OPTIONS };

static int title_items(int *ids)
{
    int n = 0;
    if (save_has_run()) ids[n++] = TI_CONTINUE;
    ids[n++] = TI_NEW;
    ids[n++] = TI_COLLECTION;
    ids[n++] = TI_OPTIONS;
    return n;
}

static void update_title(void)
{
    int ids[4], n = title_items(ids), resume = RESUME_INTRO;

    if (titleSel >= n) titleSel = 0;
    if (input_repeat(BTN_UP))   { titleSel = (titleSel + n - 1) % n; snd_move(); selPulse = 1.0f; }
    if (input_repeat(BTN_DOWN)) { titleSel = (titleSel + 1) % n; snd_move(); selPulse = 1.0f; }
    if (!input_pressed(BTN_CROSS) && !input_pressed(BTN_START)) return;

    switch (ids[titleSel]) {
    case TI_CONTINUE:
        if (save_load_run(&g, &resume)) {
            dispMoney = (float)g.money;
            moneyHidden = 0;
            snd_start();
            if (resume == RESUME_SHOP) enter_shop();
            else enter_intro();
        }
        break;
    case TI_NEW:
        deckSel = 0;
        seedEdit = 0;
        seedCustom = 0;
        set_state(ST_DECK);
        snd_select(1);
        break;
    case TI_COLLECTION:
        colSel = 0;
        colPage = 0;
        set_state(ST_COLLECTION);
        snd_select(1);
        break;
    default:
        optSel = 0;
        set_state(ST_OPTIONS);
        snd_select(1);
        break;
    }
}

static void draw_title(void)
{
    static const Card deco[5] = {
        { 14, SUIT_SPADES, 0, 0, 0 }, { 13, SUIT_HEARTS, 0, 0, 0 }, { 12, SUIT_CLUBS, 0, 0, 0 },
        { 11, SUIT_DIAMONDS, 0, 0, 0 }, { 10, SUIT_SPADES, 0, 0, 0 }
    };
    static const char *const letters[5] = { "F", "A", "R", "O", "L" };
    int ids[4], n = title_items(ids), i;

    draw_bg();
    gfx_rect(0, 104, SCR_W, 84, RGBA(0, 0, 0, 60));
    for (i = 0; i < 5; i++) {
        float t = (float)frame * 0.05f + (float)i * 0.9f;
        draw_card_x(132.0f + (float)i * 46.0f, 114.0f + sinf(t) * 4.0f + (float)((i - 2) * (i - 2)) * 2.0f,
                    (float)(i - 2) * 0.07f + sinf(t * 0.8f) * 0.04f, 1.0f, deco[i], 0, 0);
    }
    for (i = 0; i < 5; i++) {
        int x = SCR_W / 2 - 105 + i * 42;
        int y = 14 + (int)(sinf((float)frame * 0.07f + (float)i * 0.8f) * 5.0f);
        gfx_text(x + 2, y + 3, 7, RGBA(0, 0, 0, 120), letters[i]);
        gfx_text(x, y, 7, COL_BRASS, letters[i]);
    }
    gfx_text_center(SCR_W / 2, 92, 1, COL_TEXT, T("Un roguelike de póker para PSP", "A poker roguelike for PSP"));

    for (i = 0; i < n; i++) {
        const char *label;
        int y = 196 + i * 15;
        switch (ids[i]) {
        case TI_CONTINUE:   label = T("Continuar partida", "Continue run"); break;
        case TI_NEW:        label = T("Nueva partida", "New run"); break;
        case TI_COLLECTION: label = T("Colección y récords", "Collection & records"); break;
        default:            label = T("Opciones", "Options"); break;
        }
        if (i == titleSel) {
            int grow = (int)(selPulse * 3.0f);
            gfx_rect(160 - grow, y - 3, 160 + grow * 2, 14, COL_BOX);
            gfx_rect(160 - grow, y - 3, 3, 14, COL_BRASS);
            gfx_rect(317 + grow, y - 3, 3, 14, COL_BRASS);
        }
        gfx_text_center(SCR_W / 2, y, 1, i == titleSel ? COL_TEXT : COL_DIM, label);
    }
    if (save_has_run() && ids[titleSel < n ? titleSel : 0] == TI_NEW)
        gfx_text_center(SCR_W / 2, 258, 1, COL_MULT,
                        T("Empezar otra borra la partida guardada.", "Starting a new one erases your saved run."));
    gfx_text(6, 258, 1, COL_DIM, "v0.5");
}

/* ---------------------------------------------------------------- opciones */

enum { OPT_MUSIC, OPT_SFX, OPT_LANG, OPT_BACK, OPT_COUNT };

static int optMusic = 7, optSfx = 10, optLang = LANG_AUTO, optDirty;

static void apply_lang(void)
{
    if (optLang == LANG_SET_ES) game_lang = LANG_ES;
    else if (optLang == LANG_SET_EN) game_lang = LANG_EN;
    else game_lang = plat_system_lang();
}

static void options_load(void)
{
    const Profile *p = save_profile();
    optMusic = p->musicVol;
    optSfx = p->sfxVol;
    optLang = p->lang;
    optDirty = 0;
    synth_set_volume(optMusic, optSfx);
    apply_lang();
}

/* Cambia una opcion: se nota al momento y se guarda al salir del menu */
static void options_change(int which, int d)
{
    switch (which) {
    case OPT_MUSIC:
        optMusic += d;
        if (optMusic < 0) optMusic = 0;
        if (optMusic > 10) optMusic = 10;
        break;
    case OPT_SFX:
        optSfx += d;
        if (optSfx < 0) optSfx = 0;
        if (optSfx > 10) optSfx = 10;
        break;
    case OPT_LANG:
        optLang = (optLang + 3 + d) % 3;
        apply_lang();
        break;
    default:
        return;
    }
    synth_set_volume(optMusic, optSfx);
    snd_select(d > 0);          /* asi se oye el volumen nuevo de los efectos */
    selPulse = 1.0f;
    optDirty = 1;
}

static void options_commit(void)
{
    if (!optDirty) return;
    save_set_options(optMusic, optSfx, optLang);
    optDirty = 0;
}

static void update_options(void)
{
    if (input_repeat(BTN_UP))   { optSel = (optSel + OPT_COUNT - 1) % OPT_COUNT; snd_move(); selPulse = 1.0f; }
    if (input_repeat(BTN_DOWN)) { optSel = (optSel + 1) % OPT_COUNT; snd_move(); selPulse = 1.0f; }
    if (input_repeat(BTN_LEFT))  options_change(optSel, -1);
    if (input_repeat(BTN_RIGHT)) options_change(optSel, 1);
    if (input_pressed(BTN_CROSS) && optSel == OPT_LANG) options_change(OPT_LANG, 1);
    if (input_pressed(BTN_CIRCLE) || (input_pressed(BTN_CROSS) && optSel == OPT_BACK)) {
        options_commit();
        set_state(ST_TITLE);
        snd_select(0);
    }
}

/* Barra de volumen: diez casillas */
static void draw_volume(int x, int y, int v, int on)
{
    int i;
    for (i = 0; i < 10; i++)
        gfx_rect(x + i * 9, y, 7, 8, i < v ? (on ? COL_MONEY : COL_DIM) : RGB(70, 54, 62));
}

static const char *lang_label(void)
{
    if (optLang == LANG_SET_ES) return "Español";
    if (optLang == LANG_SET_EN) return "English";
    return plat_system_lang() == LANG_EN ? T("Automático (inglés)", "Automatic (English)")
                                         : T("Automático (español)", "Automatic (Spanish)");
}

static void draw_options(void)
{
    char b[64];
    int i;

    draw_bg();
    gfx_shake(shakeX, shakeY);
    popin_begin();
    draw_panel(100, 46, 280, 180);
    gfx_text_center(SCR_W / 2, 54, 2, COL_BRASS, T("OPCIONES", "OPTIONS"));
    for (i = 0; i < OPT_COUNT; i++) {
        int y = 92 + i * 24, on = (i == optSel);
        unsigned int col = on ? COL_TEXT : COL_DIM;
        if (on) {
            int grow = (int)(selPulse * 2.0f);
            gfx_rect(110, y - 5 - grow, 260, 18 + grow * 2, COL_BOX);
            gfx_rect(110, y - 5 - grow, 3, 18 + grow * 2, COL_BRASS);
        }
        switch (i) {
        case OPT_MUSIC:
            gfx_text(122, y, 1, col, T("Música", "Music"));
            draw_volume(268, y, optMusic, on);
            break;
        case OPT_SFX:
            gfx_text(122, y, 1, col, T("Efectos", "Effects"));
            draw_volume(268, y, optSfx, on);
            break;
        case OPT_LANG:
            gfx_text(122, y, 1, col, T("Idioma", "Language"));
            snprintf(b, sizeof(b), "< %s >", lang_label());
            gfx_text_right(360, y, 1, on ? COL_MONEY : COL_DIM, b);
            break;
        default:
            gfx_text(122, y, 1, col, T("Volver", "Back"));
            break;
        }
    }
    gfx_text_center(SCR_W / 2, 204, 1, COL_DIM, T("< > Cambiar   " G_CIRCLE " Volver", "< > Change   " G_CIRCLE " Back"));
    gfx_xform_off();
    gfx_shake(0, 0);
}

/* ---------------------------------------------------- eleccion de baraja */

#define DECK_SEED_ROW DECK_COUNT     /* la fila de debajo de las barajas es la semilla */

static void start_new_run(int deck, int difficulty)
{
    unsigned int seed = plat_ticks() ^ ((unsigned int)frame * 2654435761u);
#ifdef AUTOTEST
    seed = AUTOTEST_SEED;
#endif
    if (seedCustom) seed = seedValue;
    game_new_run(&g, seed, deck, difficulty);
#if defined(AUTOTEST) && AUTOTEST == 2
    /* esta variante empieza con un amuleto de "cartas en mano" para poder capturar ese efecto */
    g.relics[0] = R_LASTRE;
    g.relicCount = 1;
#endif
    dispMoney = (float)g.money;
    moneyHidden = 0;
    snd_start();
    start_intro();
}

/* Editor de la semilla: ocho cifras hexadecimales */
static void update_seed_edit(void)
{
    int sh = (7 - seedDigit) * 4, d = 0;
    unsigned int dig = (seedValue >> sh) & 15u;

    if (input_repeat(BTN_LEFT))  { seedDigit = (seedDigit + 7) % 8; snd_move(); }
    if (input_repeat(BTN_RIGHT)) { seedDigit = (seedDigit + 1) % 8; snd_move(); }
    if (input_repeat(BTN_UP)) d = 1;
    if (input_repeat(BTN_DOWN)) d = 15;
    if (d) {
        dig = (dig + (unsigned int)d) & 15u;
        seedValue = (seedValue & ~(15u << sh)) | (dig << sh);
        snd_tick((float)dig / 15.0f);
    }
    if (input_pressed(BTN_CROSS)) {
        seedEdit = 0;
        seedCustom = 1;
        snd_select(1);
    }
    if (input_pressed(BTN_CIRCLE)) {
        seedEdit = 0;
        snd_select(0);
    }
}

static void update_deck(void)
{
    int maxDiff = save_profile()->maxDifficulty;
    if (diffSel > maxDiff) diffSel = maxDiff;

    if (seedEdit) {
        update_seed_edit();
        return;
    }
    if (input_repeat(BTN_UP))   { deckSel = (deckSel + DECK_COUNT) % (DECK_COUNT + 1); snd_move(); selPulse = 1.0f; }
    if (input_repeat(BTN_DOWN)) { deckSel = (deckSel + 1) % (DECK_COUNT + 1); snd_move(); selPulse = 1.0f; }
    if (maxDiff > 0) {
        if (input_repeat(BTN_LEFT))  { diffSel = (diffSel + maxDiff) % (maxDiff + 1); snd_move(); }
        if (input_repeat(BTN_RIGHT)) { diffSel = (diffSel + 1) % (maxDiff + 1); snd_move(); }
    }
    if (input_pressed(BTN_CIRCLE)) {
        set_state(ST_TITLE);
        snd_select(0);
        return;
    }
    if (deckSel == DECK_SEED_ROW) {
        if (input_pressed(BTN_SQUARE) && seedCustom) {      /* vuelve a una semilla al azar */
            seedCustom = 0;
            snd_select(0);
        }
        if (stateAge > 6 && input_pressed(BTN_CROSS)) {
            if (!seedCustom) {
#ifdef AUTOTEST
                seedValue = AUTOTEST_SEED;
#else
                seedValue = plat_ticks() * 2654435761u;
#endif
            }
            seedEdit = 1;
            seedDigit = 0;
            snd_select(1);
        }
        return;
    }
    if (stateAge > 6 && input_pressed(BTN_CROSS)) {
        if (save_deck_unlocked(deckSel)) {
            start_new_run(deckSel, diffSel);
        } else {
            snd_error();
            shake += 2.5f;
        }
    }
}

static void draw_deck(void)
{
    char b[220];
    int d, maxDiff = save_profile()->maxDifficulty, onSeed = (deckSel == DECK_SEED_ROW);
    const char *help;

    draw_bg();
    gfx_shake(shakeX, shakeY);
    popin_begin();
    draw_panel(70, 12, 340, 248);
    gfx_text_center(SCR_W / 2, 18, 2, COL_BRASS, T("ELIGE UNA BARAJA", "CHOOSE A DECK"));
    for (d = 0; d < DECK_COUNT; d++) {
        int y = 44 + d * 15, open = save_deck_unlocked(d);
        if (d == deckSel) {
            int grow = (int)(selPulse * 2.0f);
            gfx_rect(80, y - 3 - grow, 320, 13 + grow * 2, COL_BOX);
            gfx_rect(80, y - 3 - grow, 3, 13 + grow * 2, COL_BRASS);
        }
        gfx_text(92, y, 1, open ? COL_TEXT : COL_DIM, DECK_INFO[d].name[LANG]);
        if (!open) gfx_text_right(390, y, 1, COL_MULT, T("bloqueada", "locked"));
    }

    /* dificultad: se cambia con izquierda/derecha desde cualquier fila */
    gfx_text(92, 124, 1, COL_DIM, T("Dificultad", "Difficulty"));
    if (maxDiff > 0) snprintf(b, sizeof(b), "<  %s  >", DIFF_INFO[diffSel].name[LANG]);
    else snprintf(b, sizeof(b), "%s", DIFF_INFO[0].name[LANG]);
    gfx_text_right(390, 124, 1, diffSel == 0 ? COL_TEXT : (diffSel == 1 ? COL_MONEY : COL_MULT), b);

    /* semilla */
    if (onSeed) {
        int grow = (int)(selPulse * 2.0f);
        gfx_rect(80, 137 - grow, 320, 13 + grow * 2, COL_BOX);
        gfx_rect(80, 137 - grow, 3, 13 + grow * 2, COL_BRASS);
    }
    gfx_text(92, 140, 1, onSeed ? COL_TEXT : COL_DIM, T("Semilla", "Seed"));
    if (seedEdit) {
        int k;
        for (k = 0; k < 8; k++) {
            unsigned int dig = (seedValue >> ((7 - k) * 4)) & 15u;
            char s[2];
            int x = 342 + k * 6;
            s[0] = (char)(dig < 10 ? '0' + dig : 'A' + dig - 10);
            s[1] = 0;
            if (k == seedDigit) gfx_rect(x - 1, 138, 7, 11, COL_MONEY);
            gfx_text(x, 140, 1, k == seedDigit ? COL_PANEL : COL_TEXT, s);
        }
    } else if (seedCustom) {
        snprintf(b, sizeof(b), "%08X", seedValue);
        gfx_text_right(390, 140, 1, COL_MONEY, b);
    } else {
        gfx_text_right(390, 140, 1, COL_DIM, T("al azar", "random"));
    }

    /* descripcion de lo elegido */
    gfx_rect(80, 156, 320, 66, COL_BOX);
    if (onSeed) {
        gfx_text_wrap(88, 160, 304, 1, COL_TEXT,
                      T("La misma semilla da el mismo mazo y las mismas tiendas si juegas igual. "
                        "Sirve para repetir una partida o compartirla.",
                        "The same seed gives the same deck and the same shops if you play the same way. "
                        "Use it to replay a run or to share it."));
    } else {
        if (save_deck_unlocked(deckSel)) {
            gfx_text_wrap(88, 160, 304, 1, COL_TEXT, DECK_INFO[deckSel].desc[LANG]);
        } else {
            snprintf(b, sizeof(b), T("%s Para abrirla: %s", "%s To unlock: %s"),
                     DECK_INFO[deckSel].desc[LANG], DECK_INFO[deckSel].unlock[LANG]);
            gfx_text_wrap(88, 160, 304, 1, COL_DIM, b);
        }
        if (maxDiff > 0) snprintf(b, sizeof(b), "%s: %s", DIFF_INFO[diffSel].name[LANG], DIFF_INFO[diffSel].desc[LANG]);
        else snprintf(b, sizeof(b), "%s", T("Gana una partida para abrir dificultades más altas.",
                                           "Win a run to unlock higher difficulties."));
        gfx_text_wrap(88, 190, 304, 1, COL_DIM, b);
    }

    if (seedEdit)
        help = T("Arriba/abajo cifra  < > mover  " G_CROSS " aceptar  " G_CIRCLE " cancelar",
                 "Up/down digit  < > move  " G_CROSS " OK  " G_CIRCLE " cancel");
    else if (onSeed)
        help = T(G_CROSS " Editar   " G_SQUARE " Al azar   " G_CIRCLE " Volver",
                 G_CROSS " Edit   " G_SQUARE " Random   " G_CIRCLE " Back");
    else if (maxDiff > 0)
        help = T(G_CROSS " Elegir   < > Dificultad   " G_CIRCLE " Volver",
                 G_CROSS " Choose   < > Difficulty   " G_CIRCLE " Back");
    else
        help = T(G_CROSS " Elegir    " G_CIRCLE " Volver", G_CROSS " Choose    " G_CIRCLE " Back");
    gfx_text_center(SCR_W / 2, 234, 1, COL_TEXT, help);
    gfx_xform_off();
    gfx_shake(0, 0);
}

/* ------------------------------------------------------------- coleccion */

#define COL_ROWS        11
#define COL_PER_PAGE    (COL_ROWS * 2)
#define COL_RELIC_PAGES ((R_COUNT + COL_PER_PAGE - 1) / COL_PER_PAGE)
#define COL_TRICK_PAGE  COL_RELIC_PAGES
#define COL_RECORD_PAGE (COL_RELIC_PAGES + 1)
#define COL_STATS_PAGE  (COL_RELIC_PAGES + 2)
#define COL_PAGES       (COL_RELIC_PAGES + 3)     /* amuletos, trucos, records y estadisticas */

/* Cuantas fichas tiene la pagina y cual es la primera */
static int col_count(int page, int *first)
{
    int n;
    if (page == COL_TRICK_PAGE) {
        *first = 0;
        return T_COUNT > COL_PER_PAGE ? COL_PER_PAGE : T_COUNT;
    }
    *first = page * COL_PER_PAGE;
    n = R_COUNT - *first;
    return n > COL_PER_PAGE ? COL_PER_PAGE : (n < 0 ? 0 : n);
}

static void update_collection(void)
{
    int count, first;
    if (input_pressed(BTN_CIRCLE)) {
        set_state(ST_TITLE);
        snd_select(0);
        return;
    }
    if (input_pressed(BTN_L)) { colPage = (colPage + COL_PAGES - 1) % COL_PAGES; colSel = 0; snd_whoosh(); }
    if (input_pressed(BTN_R)) { colPage = (colPage + 1) % COL_PAGES; colSel = 0; snd_whoosh(); }
    if (colPage > COL_TRICK_PAGE) return;

    count = col_count(colPage, &first);
    if (count <= 0) return;
    if (input_repeat(BTN_UP))   { colSel = (colSel + count - 1) % count; snd_move(); }
    if (input_repeat(BTN_DOWN)) { colSel = (colSel + 1) % count; snd_move(); }
    if (input_repeat(BTN_LEFT) && colSel >= COL_ROWS) { colSel -= COL_ROWS; snd_move(); }
    if (input_repeat(BTN_RIGHT) && colSel + COL_ROWS < count) { colSel += COL_ROWS; snd_move(); }
}

static void record_line(int *y, const char *label, const char *value)
{
    gfx_text(76, *y, 1, COL_TEXT, label);
    gfx_text_right(404, *y, 1, COL_MONEY, value);
    *y += 13;
}

static void stat_line(int *y, const char *label, const char *value)
{
    gfx_text(20, *y, 1, COL_TEXT, label);
    gfx_text_right(232, *y, 1, COL_MONEY, value);
    *y += 14;
}

/* Estadisticas acumuladas de todas las partidas terminadas */
static void draw_stats_page(void)
{
    const Profile *pr = save_profile();
    char b[96];
    unsigned int maxPlays = 0;
    int i, y = 40, fav = -1, favCount = 0;

    draw_panel(8, 30, 464, 218);

    snprintf(b, sizeof(b), "%u", pr->totalHands);
    stat_line(&y, T("Manos jugadas", "Hands played"), b);
    snprintf(b, sizeof(b), "%u", pr->totalDiscards);
    stat_line(&y, T("Descartes", "Discards"), b);
    snprintf(b, sizeof(b), "%u", pr->totalRounds);
    stat_line(&y, T("Rondas superadas", "Rounds won"), b);
    snprintf(b, sizeof(b), "%u", pr->totalTricks);
    stat_line(&y, T("Trucos usados", "Tricks used"), b);
    snprintf(b, sizeof(b), "$%u", pr->totalEarned);
    stat_line(&y, T("Dinero ganado", "Money earned"), b);

    /* el amuleto que mas veces has comprado */
    for (i = 0; i < R_COUNT; i++)
        if (pr->relicBought[i] > favCount) { favCount = pr->relicBought[i]; fav = i; }
    y += 8;
    gfx_text(20, y, 1, COL_BRASS, T("AMULETO FAVORITO", "FAVORITE CHARM"));
    y += 14;
    if (fav >= 0) {
        gfx_rect(18, y, 36, 36, RGB(44, 30, 40));
        gfx_art(fav, 20, y + 2, 2, COL_WHITE);
        gfx_text(62, y + 6, 1, RARITY_COL[RELIC_INFO[fav].rarity], RELIC_INFO[fav].name[LANG]);
        snprintf(b, sizeof(b), T("Comprado %d %s", "Bought %d %s"), favCount,
                 favCount == 1 ? T("vez", "time") : T("veces", "times"));
        gfx_text(62, y + 20, 1, COL_DIM, b);
    } else {
        gfx_text(20, y + 4, 1, COL_DIM, T("Todavía ninguno.", "None yet."));
    }

    /* derecha: cuantas veces se ha jugado cada mano */
    gfx_rect(240, 36, 1, 206, COL_BRASS);
    gfx_text(250, 40, 1, COL_BRASS, T("MANOS JUGADAS", "HANDS PLAYED"));
    for (i = 0; i < HT_COUNT; i++)
        if (pr->handPlays[i] > maxPlays) maxPlays = pr->handPlays[i];
    for (i = 0; i < HT_COUNT; i++) {
        int yy = 56 + i * 17, w = maxPlays ? (int)(80u * pr->handPlays[i] / maxPlays) : 0;
        gfx_text(250, yy, 1, pr->handPlays[i] ? COL_TEXT : COL_DIM, HAND_INFO[i].name[LANG]);
        gfx_rect(356, yy + 1, 80, 7, RGB(58, 44, 52));
        if (w > 0) gfx_rect(356, yy + 1, w, 7, COL_CHIPS);
        snprintf(b, sizeof(b), "%u", pr->handPlays[i]);
        gfx_text_right(466, yy, 1, COL_DIM, b);
    }
    gfx_text(20, 236, 1, COL_DIM, T("Cuenta las partidas terminadas.", "Counts finished runs."));
}

static void draw_collection(void)
{
    const Profile *pr = save_profile();
    char b[200], n[40];
    const char *title;
    int k, d;

    draw_bg();
    if (colPage < COL_TRICK_PAGE) title = T("AMULETOS", "CHARMS");
    else if (colPage == COL_TRICK_PAGE) title = T("TRUCOS", "TRICKS");
    else if (colPage == COL_RECORD_PAGE) title = T("RÉCORDS", "RECORDS");
    else title = T("ESTADÍSTICAS", "STATISTICS");
    gfx_text(14, 6, 2, COL_BRASS, title);
    snprintf(b, sizeof(b), T("L <  página %d/%d  > R", "L <  page %d/%d  > R"), colPage + 1, COL_PAGES);
    gfx_text_right(466, 12, 1, COL_DIM, b);

    if (colPage <= COL_TRICK_PAGE) {
        int tricks = colPage == COL_TRICK_PAGE;
        int first, count = col_count(colPage, &first);
        int cur = first + colSel;
        int curFound = tricks ? save_is_trick_found(cur) : save_is_found(cur);

        draw_panel(8, 30, 464, 180);
        for (k = 0; k < count; k++) {
            int id = first + k;
            int x = 16 + (k / COL_ROWS) * 230, y = 33 + (k % COL_ROWS) * 16;
            int found = tricks ? save_is_trick_found(id) : save_is_found(id);
            unsigned int col = tricks ? COL_TRICK : RARITY_COL[RELIC_INFO[id].rarity];
            if (k == colSel) {
                gfx_rect(x - 5, y, 224, 16, COL_BOX);
                gfx_rect(x - 5, y, 3, 16, COL_BRASS);
            }
            /* lo que aun no se ha descubierto se ve como una silueta */
            gfx_art(tricks ? TRICK_ART(id) : id, x, y, 1, found ? COL_WHITE : RGB(58, 48, 58));
            gfx_text(x + 22, y + 3, 1, found ? col : COL_DIM,
                     found ? (tricks ? TRICK_INFO[id].name[LANG] : RELIC_INFO[id].name[LANG]) : "???");
        }

        draw_panel(8, 214, 464, 38);
        gfx_art(tricks ? TRICK_ART(cur) : cur, 12, 217, 2, curFound ? COL_WHITE : RGB(58, 48, 58));
        if (curFound) {
            if (tricks) snprintf(b, sizeof(b), "%s", TRICK_INFO[cur].desc[LANG]);
            else snprintf(b, sizeof(b), "[%s] %s", RARITY_NAME[LANG][RELIC_INFO[cur].rarity], RELIC_INFO[cur].desc[LANG]);
            gfx_text_wrap(52, 219, 412, 1, COL_TEXT, b);
        } else {
            gfx_text_wrap(52, 219, 412, 1, COL_DIM, T("Sin descubrir. Cómpralo en una tienda para verlo aquí.",
                                                       "Not discovered yet. Buy it in a shop to see it here."));
        }
        if (tricks) snprintf(b, sizeof(b), T("Descubiertos: %d/%d", "Discovered: %d/%d"), save_trick_found_count(), T_COUNT);
        else snprintf(b, sizeof(b), T("Descubiertos: %d/%d", "Discovered: %d/%d"), save_found_count(), R_COUNT);
        gfx_text_right(472, 258, 1, COL_TEXT, b);
    } else if (colPage == COL_RECORD_PAGE) {
        int y = 42;
        draw_panel(60, 34, 360, 216);

        snprintf(b, sizeof(b), "%u", pr->runs);
        record_line(&y, T("Partidas jugadas", "Runs played"), b);
        snprintf(b, sizeof(b), "%u", pr->wins);
        record_line(&y, T("Victorias", "Wins"), b);
        if (LANG) snprintf(b, sizeof(b), "%d table%s cleared", pr->bestMesa, pr->bestMesa == 1 ? "" : "s");
        else snprintf(b, sizeof(b), "%d mesa%s superada%s", pr->bestMesa, pr->bestMesa == 1 ? "" : "s",
                      pr->bestMesa == 1 ? "" : "s");
        record_line(&y, T("Mejor partida", "Best run"), b);
        fmt_ll(n, pr->bestHand);
        record_line(&y, T("Mejor jugada", "Best hand"), n);
        snprintf(b, sizeof(b), "%d/%d", save_found_count(), R_COUNT);
        record_line(&y, T("Amuletos descubiertos", "Charms discovered"), b);
        snprintf(b, sizeof(b), "%d/%d", save_trick_found_count(), T_COUNT);
        record_line(&y, T("Trucos descubiertos", "Tricks discovered"), b);
        record_line(&y, T("Dificultad más alta abierta", "Highest difficulty unlocked"), DIFF_INFO[pr->maxDifficulty].name[LANG]);

        y += 6;
        gfx_text(76, y, 1, COL_BRASS, T("BARAJAS", "DECKS"));
        for (d = 0; d < DECK_COUNT; d++) {
            int open = save_deck_unlocked(d);
            y += 13;
            gfx_text(76, y, 1, open ? COL_TEXT : COL_DIM, DECK_INFO[d].name[LANG]);
            gfx_text_right(404, y, 1, open ? COL_GOOD : COL_DIM, open ? T("abierta", "unlocked") : DECK_INFO[d].unlock[LANG]);
        }
    } else {
        draw_stats_page();
    }
    gfx_text(8, 258, 1, COL_DIM, T("L/R Página   " G_CIRCLE " Volver", "L/R Page   " G_CIRCLE " Back"));
}

/* ------------------------------------------------------------ intro ronda */

#define INTRO_COUNT_START 8
#define INTRO_COUNT_LEN   22

static void update_intro(void)
{
    int t = stateAge - INTRO_COUNT_START;
    if (t >= 0 && t < INTRO_COUNT_LEN && (t & 1) == 0) snd_tick((float)t / INTRO_COUNT_LEN);
    if (t == INTRO_COUNT_LEN) synth_note(W_TRI, 660.0f, 0.12f, 0.25f, 0.0f);
    if (stateAge > 10 && input_pressed(BTN_CROSS)) start_play();
}

static void draw_intro(void)
{
    char b[96];
    int cx = SCR_W / 2;
    float k = clampf((float)(stateAge - INTRO_COUNT_START) / INTRO_COUNT_LEN, 0.0f, 1.0f);
    int boss = g.bossRule != B_NONE;

    draw_bg();
    gfx_shake(shakeX, shakeY);
    popin_begin();
    draw_panel(90, 30, 300, 212);
    if (g.endless) snprintf(b, sizeof(b), T("MESA %d - INFINITO", "TABLE %d - ENDLESS"), g.mesa + 1);
    else snprintf(b, sizeof(b), T("MESA %d DE %d", "TABLE %d OF %d"), g.mesa + 1, NUM_MESAS);
    gfx_text_center(cx, 40, 2, COL_BRASS, b);
    snprintf(b, sizeof(b), T("Ronda %d de 3", "Round %d of 3"), g.round + 1);
    gfx_text_center(cx, 64, 1, COL_DIM, b);
    text_fit_center(cx + (boss && stateAge < 30 ? vrand() % 3 - 1 : 0), 78, 280,
                    boss ? COL_MULT : COL_TEXT, round_name());

    gfx_rect(110, 106, 260, 50, COL_BOX);
    gfx_text_center(cx, 108, 1, COL_DIM, T("Objetivo", "Target"));
    fmt_ll(b, (long long)((double)g.target * (double)k + 0.5));
    gfx_text_center(cx, 120, 3, k >= 1.0f ? COL_TEXT : COL_MONEY, b);

    snprintf(b, sizeof(b), T("Premio: $%d", "Prize: $%d"), game_round_prize(&g));
    gfx_text_center(cx, 162, 1, COL_MONEY, b);

    if (boss) {
        boss_desc(b, sizeof(b));
        gfx_rect(100, 168, 52, 52, RGB(44, 30, 40));
        gfx_frame(100, 168, 52, 52, 1, COL_MULT);
        gfx_art(BOSS_ART(g.bossRule), 102, 170, 3, COL_WHITE);
        gfx_text_wrap(160, 176, 216, 1, COL_MULT, b);
    } else {
        gfx_text_center(cx, 182, 1, COL_DIM, T("Sin reglas especiales.", "No special rules."));
    }
    gfx_text_center(cx, 224, 1, COL_TEXT, T(G_CROSS " Empezar", G_CROSS " Start"));
    gfx_xform_off();
    gfx_shake(0, 0);
}

/* ----------------------------------------------------------------- juego */

enum { PA_CONTINUE, PA_MUSIC, PA_SFX, PA_QUIT, PA_COUNT };

static void update_pause(void)
{
    if (input_pressed(BTN_START) || input_pressed(BTN_CIRCLE)) {
        paused = 0;
        options_commit();
        return;
    }
    if (input_repeat(BTN_UP))   { pauseSel = (pauseSel + PA_COUNT - 1) % PA_COUNT; snd_move(); }
    if (input_repeat(BTN_DOWN)) { pauseSel = (pauseSel + 1) % PA_COUNT; snd_move(); }
    if (pauseSel == PA_MUSIC || pauseSel == PA_SFX) {
        int which = pauseSel == PA_MUSIC ? OPT_MUSIC : OPT_SFX;
        if (input_repeat(BTN_LEFT))  options_change(which, -1);
        if (input_repeat(BTN_RIGHT)) options_change(which, 1);
    }
    if (input_pressed(BTN_CROSS)) {
        if (pauseSel == PA_CONTINUE) {
            paused = 0;
            options_commit();
        } else if (pauseSel == PA_QUIT) {
            paused = 0;
            options_commit();
            set_state(ST_TITLE);
        }
    }
}

/* Menu de trucos: se eligen antes las cartas y luego el truco */
static void update_tricks(void)
{
    static char msg[40];
    const TrickInfo *info;
    int ids[MAX_HAND], n = 0, i, code, id;

    if (g.trickCount == 0 || input_pressed(BTN_CIRCLE) || input_pressed(BTN_R)) {
        trickMode = 0;
        return;
    }
    if (trickSel >= g.trickCount) trickSel = g.trickCount - 1;
    if (input_repeat(BTN_LEFT))  { trickSel = (trickSel + g.trickCount - 1) % g.trickCount; snd_move(); }
    if (input_repeat(BTN_RIGHT)) { trickSel = (trickSel + 1) % g.trickCount; snd_move(); }
    if (!input_pressed(BTN_CROSS)) return;

    id = g.tricks[trickSel];
    info = &TRICK_INFO[id];
    for (i = 0; i < g.handCount; i++)
        if (g.sel[i]) ids[n++] = g.hand[i].id;
    if (id == T_CIZALLA && n >= info->minSel && n <= info->maxSel)
        for (i = 0; i < g.handCount; i++)
            if (g.sel[i]) ghost_add(g.hand[i], vrs() * 2.0f, -5.0f, vrs() * 0.2f);

    code = game_use_trick(&g, trickSel);
    if (code == TRICK_OK) {
        for (i = 0; i < n; i++) {
            if (id != T_CIZALLA) {
                cpop[ids[i]] = 0.45f;
                sparks(cpx[ids[i]] + CARD_W * 0.5f, cpy[ids[i]] + 20.0f, 12, COL_TRICK, 3.0f);
            }
        }
        if (n == 0) sparks(65.0f, 187.0f, 16, COL_TRICK, 3.0f);
        mark_new_cards();
        snd_buy();
        flash = 0.18f;
        moneyPulse = 1.0f;
        trickMode = 0;
        if (cursor >= g.handCount) cursor = g.handCount > 0 ? g.handCount - 1 : 0;
    } else {
        if (code == TRICK_NEED_CARDS) {
            if (info->minSel == info->maxSel)
                snprintf(msg, sizeof(msg), T("Elige %d carta%s antes", "Pick %d card%s first"), info->minSel, info->minSel == 1 ? "" : "s");
            else
                snprintf(msg, sizeof(msg), T("Elige de %d a %d cartas antes", "Pick %d to %d cards first"), info->minSel, info->maxSel);
            toast(msg);
        } else {
            toast(T("Ahora no tiene efecto", "No effect right now"));
        }
        snd_error();
        shake += 2.0f;
    }
}

static void update_select(void)
{
    int i;

    if (input_pressed(BTN_START)) {
        paused = 1;
        pauseSel = 0;
        showInfo = 0;
        return;
    }
    if (update_info()) return;

    if (trickMode) {
        update_tricks();
        return;
    }
    if (input_pressed(BTN_R)) {
        if (g.trickCount > 0) {
            trickMode = 1;
            if (trickSel >= g.trickCount) trickSel = 0;
            snd_select(1);
        } else {
            toast(T("No tienes trucos", "You have no tricks"));
            snd_error();
        }
        return;
    }

    if (g.handCount > 0) {
        if (cursor >= g.handCount) cursor = g.handCount - 1;
        if (input_repeat(BTN_LEFT))  { cursor = (cursor + g.handCount - 1) % g.handCount; snd_move(); }
        if (input_repeat(BTN_RIGHT)) { cursor = (cursor + 1) % g.handCount; snd_move(); }
        if (input_pressed(BTN_CROSS) || (input_pressed(BTN_UP) && !g.sel[cursor])
            || (input_pressed(BTN_DOWN) && g.sel[cursor])) {
            int id = g.hand[cursor].id;
            if (!g.sel[cursor] && game_selected_count(&g) >= MAX_PLAY) {
                toast(T("Máximo 5 cartas", "5 cards at most"));
                snd_error();
            } else {
                game_toggle_select(&g, cursor);
                snd_select(g.sel[cursor]);
                cpop[id] = 0.16f;
            }
        }
    }
    if (input_pressed(BTN_CIRCLE)) {
        if (game_selected_count(&g) > 0) snd_select(0);
        game_clear_selection(&g);
    }
    if (input_pressed(BTN_L)) { g.sortMode ^= 1; game_sort_hand(&g); snd_whoosh(); }

    if (input_pressed(BTN_SQUARE)) {
        int why = game_play_block(&g);
        if (why == PLAY_OK) {
            game_play(&g, &res);
#if defined(AUTOTEST) && AUTOTEST == 4
            if (res.total < g.target) res.total = g.target;     /* esta variante gana siempre */
#endif
            for (i = 0; i < g.playedCount; i++) cdelay[g.played[i].id] = (short)(1 + i * 3);
            phase = PH_TOCENTER;
            timer = 0;
            shownChips = res.baseChips;
            shownMult = res.baseMult;
            chipsPulse = multPulse = 1.0f;
            snd_whoosh();
        } else {
            toast(why == PLAY_NEED_FIVE ? T("El Estirado exige 5 cartas", "The Snob demands 5 cards") : T("Elige de 1 a 5 cartas", "Pick 1 to 5 cards"));
            snd_error();
            shake += 2.0f;
        }
    }
    if (input_pressed(BTN_TRIANGLE)) {
        if (g.discardsLeft <= 0) {
            toast(T("No te quedan descartes", "No discards left"));
            snd_error();
        } else if (game_selected_count(&g) == 0) {
            toast(T("Elige cartas para descartar", "Pick cards to discard"));
            snd_error();
        } else {
            for (i = 0; i < g.handCount; i++)
                if (g.sel[i]) ghost_add(g.hand[i], 5.0f + vrf() * 4.0f, -6.0f - vrf() * 3.0f, 0.10f + vrf() * 0.12f);
            game_discard(&g);
            mark_new_cards();
            snd_discard();
        }
        if (cursor >= g.handCount) cursor = g.handCount > 0 ? g.handCount - 1 : 0;
    }
}

static unsigned int ev_text(const ScoreEvent *e, char *b, int n)
{
    int t;
    switch (e->kind) {
    case EV_CHIPS:
        snprintf(b, n, "+%d", (int)e->value);
        return COL_CHIPS;
    case EV_MULT:
        snprintf(b, n, T("+%d Multi", "+%d Mult"), (int)e->value);
        return COL_MULT;
    case EV_XMULT:
        t = (int)(e->value * 10.0f + 0.5f);
        if (t % 10) snprintf(b, n, T("x%d,%d Multi", "x%d.%d Mult"), t / 10, t % 10);
        else snprintf(b, n, T("x%d Multi", "x%d Mult"), t / 10);
        return COL_MULT;
    case EV_MONEY:
        snprintf(b, n, "+$%d", (int)e->value);
        return COL_MONEY;
    default:
        snprintf(b, n, T("Anulada", "Void"));
        return COL_DIM;
    }
}

/* Un evento de puntuacion: la carta salta, suena una nota mas aguda que la anterior... */
static void fire_event(int idx)
{
    const ScoreEvent *e = &res.ev[idx];
    char b[24];
    unsigned int col = ev_text(e, b, sizeof(b));
    float x = (float)(AREA_X + AREA_W / 2), y = (float)(PLAY_Y - 16);
    float sx = x, sy = (float)(PLAY_Y + 20);

    if (e->card >= 0) {
        int id = g.played[e->card].id;
        cpop[id] = 0.32f;
        cvy[id] -= 2.5f;
        x = sx = cpx[id] + CARD_W * 0.5f;
        sy = cpy[id] + 10.0f;
    }
    if (e->relic >= 0) {
        relicPop[e->relic] = 1.0f;
        x = (float)(RELIC_X(e->relic) + SLOT_W / 2);
        y = 46.0f;
        if (e->card < 0) { sx = x; sy = 30.0f; }
    }
    if (e->held >= 0 && e->held < g.handCount) {
        /* carta que se quedo en la mano: salta ella y el numero sale encima */
        int id = g.hand[e->held].id;
        cpop[id] = 0.30f;
        cvy[id] -= 2.5f;
        x = sx = cpx[id] + CARD_W * 0.5f;
        y = cpy[id] - 16.0f;
        sy = cpy[id] + 10.0f;
    }
    if (e->relic >= 0 && e->relic < g.relicCount && e->card < 0 && e->held < 0) {
        char full[48];
        snprintf(full, sizeof(full), "%s %s", RELIC_INFO[g.relics[e->relic]].name[LANG], b);
        popup_add(x, y, col, full);
    } else {
        popup_add(x, y, col, b);
    }

    switch (e->kind) {
    case EV_CHIPS:
        chipsPulse = 1.0f;
        snd_chips(idx);
        sparks(sx, sy, 5, COL_CHIPS, 2.2f);
        break;
    case EV_MULT:
        multPulse = 1.0f;
        snd_mult(idx);
        sparks(sx, sy, 7, COL_MULT, 2.6f);
        break;
    case EV_XMULT:
        multPulse = 1.6f;
        shake += 4.5f;
        heat += 0.2f;
        snd_xmult(idx);
        sparks(sx, sy, 18, COL_HOT, 3.6f);
        break;
    case EV_MONEY:
        moneyPulse = 1.0f;
        snd_money();
        sparks(sx, sy, 8, COL_MONEY, 2.4f);
        break;
    default:
        snd_null();
        break;
    }
    heat = clampf(heat + 0.07f, 0.0f, 1.0f);
}

static void enter_total(void)
{
    long long t = res.total;
    int digits = 1;
    while (t >= 10) { t /= 10; digits++; }

    phase = PH_TOTAL;
    timer = 0;
    shownChips = res.chips;
    shownMult = res.mult;
    totalPop = 1.0f;
    hotHand = (g.score + res.total >= g.target);
    shake += clampf(1.0f + (float)digits * 1.1f, 0.0f, 8.0f) + (hotHand ? 3.0f : 0.0f);
    snd_total(hotHand);
    sparks((float)(AREA_X + AREA_W / 2), (float)(PLAY_Y + CARD_H + 24), hotHand ? 30 : 12,
           hotHand ? COL_HOT : COL_MONEY, hotHand ? 4.5f : 3.0f);
    if (hotHand) heat = 1.0f;
}

static void enter_end(void)
{
    int i;
    phase = PH_END;
    timer = 0;
    bannerY = -44.0f;
    bannerVy = 0.0f;
    if (game_round_won(&g)) {
        flash = 0.6f;
        confetti(46);
        snd_win();
    } else {
        snd_lose();
        shake += 7.0f;
        handGone = 1;
        for (i = 0; i < g.handCount; i++)
            ghost_add(g.hand[i], vrs() * 2.5f, -4.0f * vrf(), vrs() * 0.12f);
    }
}

static void update_play(void)
{
    int fast = input_held(BTN_CROSS);
    int i, dur;

    if (toastTimer > 0) toastTimer--;
    if (paused) {
        update_pause();
        return;
    }
    anim_cards();

    switch (phase) {
    case PH_SELECT:
        update_select();
        break;

    case PH_TOCENTER:
        if (++timer >= (fast ? 8 : 22)) {
            phase = PH_EVENTS;
            evIdx = 0;
            evTimer = 0;
        }
        break;

    case PH_EVENTS:
        if (evIdx >= res.evCount) {
            enter_total();
            break;
        }
        if (evTimer == 0) {
            shownChips = res.ev[evIdx].chips;
            shownMult = res.ev[evIdx].mult;
            fire_event(evIdx);
        }
        /* cada evento dura un poco menos que el anterior: el recuento se acelera */
        dur = 16 - evIdx / 2;
        if (dur < 9) dur = 9;
        if (fast) dur = 4;
        if (++evTimer >= dur) {
            evIdx++;
            evTimer = 0;
        }
        break;

    case PH_TOTAL:
        if (++timer >= (fast ? 10 : 34)) {
            for (i = 0; i < g.playedCount; i++)
                ghost_add(g.played[i], 7.0f + vrf() * 4.0f, -3.0f - vrf() * 2.0f, 0.06f + vrf() * 0.08f);
            game_finish_play(&g, &res);
            mark_new_cards();
            if (cursor >= g.handCount) cursor = g.handCount > 0 ? g.handCount - 1 : 0;
            scorePulse = 1.0f;
            snd_whoosh();
            phase = PH_COUNT;
            timer = 0;
        }
        break;

    case PH_COUNT: {
        double diff = (double)g.score - dispScore;
        if (diff > 0.5) {
            double step = fast ? diff : diff * 0.16;
            if (step < 1.0) step = 1.0;
            dispScore += step;
            if (dispScore > (double)g.score) dispScore = (double)g.score;
            if ((frame & 1) == 0) snd_tick((float)timer / 20.0f);
            scorePulse = 0.6f;
            if (game_round_won(&g)) embers(8.0f, 122.0f, 92.0f, 2);
        } else {
            dispScore = (double)g.score;
        }
        timer++;
        if (dispScore >= (double)g.score && timer >= (fast ? 4 : 14)) {
            if (game_round_won(&g) || game_round_lost(&g)) enter_end();
            else phase = PH_SELECT;
        }
        break;
    }

    default: /* PH_END */
        bannerVy += 1.3f;
        bannerY += bannerVy;
        if (bannerY > 96.0f) {
            bannerY = 96.0f;
            bannerVy = -bannerVy * 0.42f;
        }
        if (game_round_won(&g) && (timer % 6) == 0) confetti(3);
        timer++;
        if (timer >= 85 || (timer > 30 && input_pressed(BTN_CROSS))) {
            if (game_round_won(&g)) {
                game_collect_reward(&g, &reward);
                moneyHidden = reward.total;
                set_state(ST_WIN);
            } else {
                end_run(0);
                set_state(ST_GAMEOVER);
                shake += 5.0f;
            }
        }
        break;
    }
}

static void draw_left(void)
{
    char b[128];
    int type = -1;
    int won = dispScore >= (double)g.target;
    double chips = 0, mult = 0;
    float p;

    draw_panel(2, 2, 126, 268);
    if (g.endless) snprintf(b, sizeof(b), T("MESA %d inf. R%d", "TABLE %d inf. R%d"), g.mesa + 1, g.round + 1);
    else snprintf(b, sizeof(b), T("MESA %d/%d  R%d", "TABLE %d/%d  R%d"), g.mesa + 1, NUM_MESAS, g.round + 1);
    gfx_text(8, 5, 1, COL_BRASS, b);
    gfx_text(8, 16, 1, g.round == 2 ? COL_MULT : COL_TEXT, round_name());

    gfx_rect(6, 29, 118, 31, COL_BOX);
    gfx_text(10, 29, 1, COL_DIM, T("Objetivo", "Target"));
    fmt_ll(b, g.target);
    text_fit_center(65, 39, 112, COL_TEXT, b);

    p = clampf(scorePulse, 0.0f, 1.0f);
    if (p > 0.02f) gfx_xform(65.0f, 78.0f, 0.0f, 1.0f + 0.10f * p);
    gfx_rect(6, 63, 118, 31, col_lerp(COL_BOX, won ? RGB(70, 110, 60) : RGB(110, 90, 50), p));
    if (won) gfx_frame(6, 63, 118, 31, 1, COL_GOOD);
    gfx_text(10, 63, 1, COL_DIM, T("Puntos", "Score"));
    fmt_ll(b, (long long)(dispScore + 0.5));
    text_fit_center(65, 73, 112, won ? COL_GOOD : COL_MONEY, b);
    gfx_xform_off();

    /* jugada actual */
    if (phase == PH_SELECT) {
        Card c[MAX_PLAY];
        int n = game_selected_cards(&g, c);
        if (n > 0) {
            HandEval e;
            int bc, bm;
            game_eval(c, n, &e);
            type = e.type;
            game_hand_base(&g, type, &bc, &bm);
            chips = bc;
            mult = bm;
        }
    } else if (phase == PH_TOCENTER || phase == PH_EVENTS || phase == PH_TOTAL) {
        type = res.eval.type;
        chips = shownChips;
        mult = shownMult;
    }
    if (type >= 0) {
        gfx_text(8, 98, 1, COL_TEXT, HAND_INFO[type].name[LANG]);
        snprintf(b, sizeof(b), T("nv.%d", "lv.%d"), g.handLevel[type]);
        gfx_text_right(122, 98, 1, COL_DIM, b);
    } else if (phase == PH_SELECT) {
        gfx_text(8, 98, 1, COL_DIM, T("Elige cartas", "Pick cards"));
    }
    gfx_text_center(65, 116, 1, COL_TEXT, "x");

    p = clampf(chipsPulse, 0.0f, 1.6f);
    if (p > 0.02f) gfx_xform(32.0f, 122.0f, 0.0f, 1.0f + 0.18f * p);
    gfx_rect(6, 110, 52, 24, col_lerp(COL_CHIPS, COL_WHITE, p * 0.5f));
    fmt_ll(b, (long long)chips);
    text_fit_center(32, 112, 50, COL_TEXT, b);
    gfx_xform_off();

    p = clampf(multPulse, 0.0f, 1.6f);
    if (p > 0.02f) gfx_xform(98.0f, 122.0f, 0.0f, 1.0f + 0.18f * p);
    gfx_rect(72, 110, 52, 24, col_lerp(COL_MULT, COL_WHITE, p * 0.5f));
    fmt_mult(b, mult);
    text_fit_center(98, 112, 50, COL_TEXT, b);
    gfx_xform_off();

    gfx_rect(6, 139, 38, 32, COL_BOX);
    gfx_rect(46, 139, 38, 32, COL_BOX);
    gfx_text_center(25, 139, 1, COL_CHIPS, T("Jug.", "Hands"));
    gfx_text_center(65, 139, 1, COL_MULT, T("Desc.", "Disc."));
    snprintf(b, sizeof(b), "%d", g.handsLeft);
    gfx_text_center(25, 150, 2, g.handsLeft == 1 ? COL_HOT : COL_TEXT, b);
    snprintf(b, sizeof(b), "%d", g.discardsLeft);
    gfx_text_center(65, 150, 2, COL_TEXT, b);

    p = clampf(moneyPulse, 0.0f, 1.0f);
    if (p > 0.02f) gfx_xform(105.0f, 155.0f, 0.0f, 1.0f + 0.14f * p);
    gfx_rect(86, 139, 38, 32, col_lerp(COL_BOX, RGB(120, 100, 40), p));
    snprintf(b, sizeof(b), "$%d", (int)(dispMoney + 0.5f));
    text_fit_center(105, 146, 36, COL_MONEY, b);
    gfx_xform_off();

    if (g.bossRule != B_NONE) {
        boss_desc(b, sizeof(b));
        gfx_text_wrap(8, 180, 116, 1, COL_MULT, b);
        gfx_rect(88, 222, 36, 36, RGB(44, 30, 40));
        gfx_frame(88, 222, 36, 36, 1, COL_MULT);
        gfx_art(BOSS_ART(g.bossRule), 90, 224, 2, COL_WHITE);
    }
    snprintf(b, sizeof(b), T("Mazo %d/%d", "Deck %d/%d"), g.drawCount, game_deck_alive(&g));
    gfx_text(8, 256, 1, COL_DIM, b);
}

/* Fila superior: cinco amuletos y, a la derecha, los huecos de trucos. Solo ilustraciones. */
static void draw_relics(void)
{
    int s, k, pockets = game_pockets(&g);

    for (s = 0; s < MAX_RELICS; s++) {
        int x = RELIC_X(s), y = 4;
        if (s < g.relicCount) {
            int id = g.relics[s];
            int off = (state == ST_PLAY && g.bossRule == B_THIEF && s == 0);   /* anulado por el rival */
            float p = relicPop[s];
            float rot = sinf((float)frame * 0.03f + (float)s * 1.4f) * 0.012f;
            if (p > 0.03f) rot += sinf((float)frame * 0.9f) * 0.10f * p;
            gfx_xform((float)x + SLOT_W * 0.5f, (float)y + 18.0f, rot, 1.0f + 0.22f * p);
            gfx_rect(x, y, SLOT_W, 36, col_lerp(RGB(44, 30, 40), COL_WHITE, p * 0.45f));
            gfx_frame(x, y, SLOT_W, 36, 1, RELIC_INFO[id].rarity ? RARITY_COL[RELIC_INFO[id].rarity] : COL_BRASS);
            gfx_art(id, x + 4, y + 2, 2, off ? RGB(76, 70, 78) : COL_WHITE);
            if (p > 0.25f) gfx_frame(x - 1, y - 1, SLOT_W + 2, 38, 2, COL_TEXT);
            gfx_xform_off();
        } else {
            gfx_rect(x, y, SLOT_W, 36, RGBA(0, 0, 0, 70));
            gfx_frame(x, y, SLOT_W, 36, 1, RGBA(204, 160, 78, 70));
        }
    }

    for (k = 0; k < pockets; k++) {
        int x = TRICK_X(k), y = 4;
        if (k < g.trickCount) {
            int on = trickMode && k == trickSel;
            gfx_rect(x, y, SLOT_W, 36, on ? RGB(92, 70, 136) : RGB(52, 40, 80));
            gfx_frame(x, y, SLOT_W, 36, on ? 2 : 1, on ? COL_MONEY : COL_TRICK);
            gfx_art(TRICK_ART(g.tricks[k]), x + 4, y + 2, 2, COL_WHITE);
        } else {
            gfx_rect(x, y, SLOT_W, 36, RGBA(0, 0, 0, 70));
            gfx_frame(x, y, SLOT_W, 36, 1, RGBA(170, 130, 230, 70));
        }
    }
}

/* ------------------------------------------------- informacion (SELECT) */

#define INFO_PAGES 3

/* Abre, cierra y pasa de pagina. Devuelve 1 mientras la informacion esta en pantalla.
   En la pagina de amuletos, X coge uno y arriba/abajo lo cambia de sitio. */
static int update_info(void)
{
    if (input_pressed(BTN_SELECT)) {
        showInfo = !showInfo;
        infoHeld = 0;
        snd_select(showInfo);
        return 1;
    }
    if (!showInfo) return 0;
    if (input_pressed(BTN_CIRCLE)) {
        if (infoHeld) infoHeld = 0;
        else showInfo = 0;
        snd_select(0);
        return 1;
    }
    if (input_pressed(BTN_L) || input_repeat(BTN_LEFT)) {
        infoPage = (infoPage + INFO_PAGES - 1) % INFO_PAGES;
        infoHeld = 0;
        snd_move();
    }
    if (input_pressed(BTN_R) || input_repeat(BTN_RIGHT)) {
        infoPage = (infoPage + 1) % INFO_PAGES;
        infoHeld = 0;
        snd_move();
    }
    if (infoPage == 1 && g.relicCount > 0) {
        int d = 0;
        if (infoSel >= g.relicCount) infoSel = g.relicCount - 1;
        if (input_pressed(BTN_CROSS) && g.relicCount > 1) {
            infoHeld = !infoHeld;
            snd_select(infoHeld);
        }
        if (input_repeat(BTN_UP)) d = -1;
        if (input_repeat(BTN_DOWN)) d = 1;
        if (d && infoHeld) {
            int to = game_move_relic(&g, infoSel, d);
            if (to != infoSel) {
                infoSel = to;
                relicPop[to] = 0.6f;
                snd_whoosh();
            }
        } else if (d && infoSel + d >= 0 && infoSel + d < g.relicCount) {
            infoSel += d;
            snd_move();
        }
    }
    return 1;
}

static void info_hands(void)
{
    char b[96];
    int i, y;

    gfx_text(16, 30, 1, COL_DIM, T("Mano", "Hand"));
    gfx_text(150, 30, 1, COL_DIM, T("Nv", "Lv"));
    gfx_text(176, 30, 1, COL_DIM, T("Fichas x Multi", "Chips x Mult"));
    gfx_text(280, 30, 1, COL_DIM, T("Jugada", "Played"));
    for (i = 0; i < HT_COUNT; i++) {
        int bc, bm;
        y = 44 + i * 13;
        game_hand_base(&g, i, &bc, &bm);
        gfx_text(16, y, 1, g.handPlays[i] ? COL_TEXT : COL_DIM, HAND_INFO[i].name[LANG]);
        snprintf(b, sizeof(b), "%d", g.handLevel[i]);
        gfx_text(150, y, 1, COL_TEXT, b);
        snprintf(b, sizeof(b), "%d", bc);
        gfx_text_right(210, y, 1, COL_CHIPS, b);
        gfx_text(216, y, 1, COL_DIM, "x");
        snprintf(b, sizeof(b), "%d", bm);
        gfx_text(228, y, 1, COL_MULT, b);
        snprintf(b, sizeof(b), "%d", g.handPlays[i]);
        gfx_text_right(316, y, 1, COL_DIM, b);
    }

    gfx_text(16, 182, 1, COL_BRASS, T("MARCAS", "MARKS"));
    for (i = 1; i < MARK_COUNT; i++) {
        y = 182 + i * 12;
        gfx_rect(16, y + 2, 8, 8, MARK_BAND[i]);
        snprintf(b, sizeof(b), "%s: %s", MARK_NAME[LANG][i], MARK_DESC[LANG][i]);
        gfx_text(28, y, 1, COL_TEXT, b);
    }
    gfx_text(250, 182, 1, COL_BRASS, T("MUESCAS", "NOTCHES"));
    for (i = 1; i < NOTCH_COUNT; i++) {
        y = 182 + i * 12;
        gfx_rect(250, y + 2, 8, 8, NOTCH_COL[i]);
        snprintf(b, sizeof(b), "%s: %s", NOTCH_NAME[LANG][i], NOTCH_DESC[LANG][i]);
        gfx_text(262, y, 1, COL_TEXT, b);
    }
    gfx_text(340, 44, 1, COL_DIM, T("Las cartas valen su", "Cards are worth"));
    gfx_text(340, 55, 1, COL_DIM, T("número; J, Q y K", "their number; J, Q"));
    gfx_text(340, 66, 1, COL_DIM, T("valen 10 y el As 11.", "and K are 10, Ace 11."));
}

static void info_items(void)
{
    int i, y;

    gfx_text(16, 30, 1, COL_BRASS, T("AMULETOS", "CHARMS"));
    if (g.relicCount > 1) gfx_text_right(250, 30, 1, COL_DIM, T("el orden importa", "order matters"));
    if (g.relicCount == 0) gfx_text(16, 46, 1, COL_DIM, T("Todavía no tienes ninguno.", "You don't have any yet."));
    for (i = 0; i < g.relicCount; i++) {
        int id = g.relics[i];
        y = 42 + i * 40;
        if (i == infoSel && g.relicCount > 1)
            gfx_frame(10, y - 3, 242, 41, infoHeld ? 2 : 1, infoHeld ? COL_MONEY : COL_BRASS);
        gfx_rect(14, y, 36, 36, RGB(44, 30, 40));
        gfx_art(id, 16, y + 2, 2, COL_WHITE);
        gfx_text(56, y + 2, 1, RARITY_COL[RELIC_INFO[id].rarity], RELIC_INFO[id].name[LANG]);
        gfx_text_wrap(56, y + 13, 194, 1, COL_DIM, RELIC_INFO[id].desc[LANG]);
    }
    if (g.relicCount > 1)
        gfx_text(16, 246, 1, infoHeld ? COL_MONEY : COL_DIM,
                 infoHeld ? T("Arriba/abajo: moverlo.  " G_CROSS " soltarlo.", "Up/down: move it.  " G_CROSS " drop it.")
                          : T(G_CROSS " coge uno para cambiarlo de sitio.", G_CROSS " pick one up to move it."));

    gfx_rect(256, 30, 1, 224, COL_BRASS);
    gfx_text(266, 30, 1, COL_TRICK, T("TRUCOS", "TRICKS"));
    if (g.trickCount == 0) gfx_text(266, 46, 1, COL_DIM, T("No llevas ninguno.", "You carry none."));
    for (i = 0; i < g.trickCount; i++) {
        int id = g.tricks[i];
        y = 42 + i * 52;
        gfx_rect(264, y, 36, 36, RGB(52, 40, 80));
        gfx_art(TRICK_ART(id), 266, y + 2, 2, COL_WHITE);
        gfx_text(308, y + 2, 1, COL_TEXT, TRICK_INFO[id].name[LANG]);
        gfx_text_wrap(308, y + 13, 158, 1, COL_DIM, TRICK_INFO[id].desc[LANG]);
    }
    gfx_text_wrap(266, 206, 200, 1, COL_DIM, T("Los trucos se usan en la ronda con R, tras elegir las cartas.",
                                               "Use tricks during a round with R, after picking the cards."));
}

/* El mazo entero: una casilla por carta, con las que quedan por robar */
static void info_deck(void)
{
    static const int ORDER[13] = { 14, 13, 12, 11, 10, 9, 8, 7, 6, 5, 4, 3, 2 };
    unsigned char total[4][15], left[4][15];
    char b[64];
    int inRound = (state == ST_PLAY);
    int i, s, col, alive = 0;

    memset(total, 0, sizeof(total));
    memset(left, 0, sizeof(left));
    for (i = 0; i < g.deckCount; i++) {
        Card c = g.deck[i];
        if (!c.rank || c.rank > 14 || c.suit > 3) continue;
        total[c.suit][c.rank]++;
        alive++;
    }
    if (inRound) {
        for (i = 0; i < g.drawCount; i++) left[g.draw[i].suit & 3][g.draw[i].rank < 15 ? g.draw[i].rank : 0]++;
    } else {
        memcpy(left, total, sizeof(left));
    }

    for (s = 0; s < 4; s++) {
        int y = 30 + s * 48;
        gfx_icon(12, y + 14, 2, s, SUIT_BRIGHT[s]);
        for (col = 0; col < 13; col++) {
            int r = ORDER[col], x = 34 + col * 34, dots = 0;
            int t = total[s][r], l = left[s][r];
            gfx_rect(x, y, 32, 44, t ? (l ? COL_BOX : RGB(34, 24, 30)) : RGBA(0, 0, 0, 60));
            if (!t) continue;
            gfx_text(x + (r == 10 ? 3 : 9), y + 2, 2, l ? SUIT_BRIGHT[s] : RGB(96, 86, 92), RANK_STR[r]);
            if (inRound) snprintf(b, sizeof(b), "%d/%d", l, t);
            else snprintf(b, sizeof(b), "x%d", t);
            if (inRound || t > 1) gfx_text_center(x + 16, y + 24, 1, l ? COL_TEXT : COL_DIM, b);
            /* cuadraditos: marcas y muescas de las cartas de esa casilla */
            for (i = 0; i < g.deckCount && dots < 6; i++) {
                Card c = g.deck[i];
                if (c.rank != r || c.suit != s) continue;
                if (c.mark) { gfx_rect(x + 2 + dots * 5, y + 37, 4, 4, MARK_BAND[c.mark]); dots++; }
                if (c.notch && dots < 6) { gfx_rect(x + 2 + dots * 5, y + 37, 4, 4, NOTCH_COL[c.notch]); dots++; }
            }
        }
    }
    if (inRound) snprintf(b, sizeof(b), T("Quedan %d de %d cartas por robar.", "%d of %d cards left to draw."), g.drawCount, alive);
    else snprintf(b, sizeof(b), T("Tu mazo tiene %d cartas.", "Your deck has %d cards."), alive);
    gfx_text(12, 226, 1, COL_TEXT, b);
    gfx_text(12, 238, 1, COL_DIM, T("Cuadraditos de color: cartas con marca o muesca.", "Colored squares: cards with a mark or notch."));
}

static void draw_info(void)
{
    const char *title[INFO_PAGES];
    int i, x = 14;

    title[0] = T("MANOS", "HANDS");
    title[1] = T("AMULETOS Y TRUCOS", "CHARMS & TRICKS");
    title[2] = T("MAZO", "DECK");
    gfx_rect(0, 0, SCR_W, SCR_H, COL_SHADE);
    draw_panel(4, 4, 472, 264);
    for (i = 0; i < INFO_PAGES; i++) {
        int w = gfx_text_width(title[i], 1);
        if (i == infoPage) gfx_rect(x - 4, 8, w + 8, 14, COL_BOX);
        gfx_text(x, 10, 1, i == infoPage ? COL_BRASS : COL_DIM, title[i]);
        x += w + 16;
    }
    gfx_text_right(468, 10, 1, COL_DIM, T("L/R página  " G_CIRCLE " cerrar", "L/R page  " G_CIRCLE " close"));
    gfx_rect(8, 24, 464, 1, COL_BRASS);

    if (infoPage == 0) info_hands();
    else if (infoPage == 1) info_items();
    else info_deck();
}

static void draw_pause(void)
{
    char b[48];
    int i;

    gfx_rect(0, 0, SCR_W, SCR_H, COL_SHADE);
    draw_panel(130, 56, 220, 160);
    gfx_text_center(SCR_W / 2, 64, 2, COL_BRASS, T("PAUSA", "PAUSE"));
    for (i = 0; i < PA_COUNT; i++) {
        int y = 98 + i * 20, on = (i == pauseSel);
        unsigned int col = on ? COL_TEXT : COL_DIM;
        if (on) {
            gfx_rect(140, y - 4, 200, 16, COL_BOX);
            gfx_rect(140, y - 4, 3, 16, COL_BRASS);
        }
        switch (i) {
        case PA_CONTINUE:
            gfx_text(152, y, 1, col, T("Continuar", "Resume"));
            break;
        case PA_MUSIC:
            gfx_text(152, y, 1, col, T("Música", "Music"));
            draw_volume(240, y, optMusic, on);
            break;
        case PA_SFX:
            gfx_text(152, y, 1, col, T("Efectos", "Effects"));
            draw_volume(240, y, optSfx, on);
            break;
        default:
            gfx_text(152, y, 1, col, T("Salir al título", "Quit to title"));
            break;
        }
    }
    snprintf(b, sizeof(b), T("Semilla: %08X", "Seed: %08X"), g.seed);
    gfx_text_center(SCR_W / 2, 184, 1, COL_DIM, b);
    gfx_text_center(SCR_W / 2, 198, 1, COL_DIM, T("La partida se guarda sola.", "The game saves by itself."));
}

static void draw_play(void)
{
    char b[64];
    int i, pass;

    draw_bg();
    gfx_shake(shakeX, shakeY);
    gfx_grad(AREA_X + shakeX, FELT_Y + shakeY, AREA_W, FELT_H,
             col_lerp(COL_FELT_TOP, RGB(52, 128, 92), heat * 0.6f), COL_FELT_BOT);
    gfx_frame(AREA_X, FELT_Y, AREA_W, FELT_H, 1, COL_BRASS);
    draw_left();
    draw_relics();

    /* cartas que salen volando (descartes, jugadas ya contadas) */
    for (i = 0; i < MAX_GHOST; i++) {
        const Ghost *gh = &ghosts[i];
        if (gh->life > 0) draw_card_x(gh->x, gh->y, gh->rot, 1.0f, gh->c, 0, 0);
    }

    /* cartas jugadas */
    for (i = 0; i < g.playedCount; i++) {
        Card c = g.played[i];
        draw_card_x(cpx[c.id], cpy[c.id], crot[c.id], 1.0f + cpop[c.id], c, game_card_debuffed(&g, c), 0);
    }

    /* mano: la carta del cursor se pinta la ultima para que quede encima */
    if (!handGone) {
        for (pass = 0; pass < 2; pass++) {
            for (i = 0; i < g.handCount; i++) {
                Card c = g.hand[i];
                int cur = (phase == PH_SELECT && i == cursor && !paused);
                if (cur != pass) continue;
                draw_card_x(cpx[c.id], cpy[c.id], crot[c.id], 1.0f + cpop[c.id] + (cur ? 0.08f : 0.0f),
                            c, game_card_debuffed(&g, c), cur);
            }
        }
    }

    draw_particles();
    draw_popups();

    /* total de la jugada: entra con un golpe y tiembla si con ella se gana la ronda */
    if (phase == PH_TOTAL || (phase == PH_COUNT && timer < 12)) {
        char n[40];
        int cx = AREA_X + AREA_W / 2, y = PLAY_Y + CARD_H + 3;
        int sc;
        unsigned int col = COL_MONEY;
        fmt_ll(n, res.total);
        snprintf(b, sizeof(b), "+%s", n);
        sc = gfx_text_width(b, 3) <= AREA_W - 20 ? 3 : 2;
        if (hotHand) col = col_lerp(COL_MONEY, COL_HOT, 0.5f + 0.5f * sinf((float)frame * 0.6f));
        gfx_xform((float)cx, (float)(y + 5 * sc), hotHand ? sinf((float)frame * 0.9f) * 0.05f : 0.0f,
                  1.0f + totalPop * 0.6f);
        gfx_text_center(cx + 2, y + 2, sc, RGBA(0, 0, 0, 170), b);
        gfx_text_center(cx, y, sc, col_lerp(col, COL_WHITE, totalPop), b);
        gfx_xform_off();
    }

    if (phase == PH_END) {
        int won = game_round_won(&g);
        int by = (int)bannerY;
        gfx_rect(AREA_X + 1, by, AREA_W - 2, 36, RGBA(0, 0, 0, 190));
        gfx_rect(AREA_X + 1, by, AREA_W - 2, 1, won ? COL_GOOD : COL_MULT);
        gfx_rect(AREA_X + 1, by + 35, AREA_W - 2, 1, won ? COL_GOOD : COL_MULT);
        gfx_text_center(AREA_X + AREA_W / 2, by + 8, 2, won ? COL_GOOD : COL_MULT,
                        won ? T("¡Ronda superada!", "Round cleared!") : T("Sin jugadas...", "Out of hands..."));
    }

    if (toastTimer > 0 && toastMsg) {
        int w = gfx_text_width(toastMsg, 1);
        int x = AREA_X + AREA_W / 2 - w / 2;
        gfx_rect(x - 4, 147, w + 7, 12, RGBA(0, 0, 0, 205));
        gfx_text(x, 148, 1, COL_TEXT, toastMsg);
    }

    if (trickMode && g.trickCount > 0) {
        const TrickInfo *info = &TRICK_INFO[g.tricks[trickSel < g.trickCount ? trickSel : 0]];
        gfx_rect(AREA_X + 20, 84, AREA_W - 40, 58, RGBA(20, 12, 34, 235));
        gfx_frame(AREA_X + 20, 84, AREA_W - 40, 58, 1, COL_TRICK);
        gfx_text(AREA_X + 28, 88, 1, COL_TRICK, T("TRUCO", "TRICK"));
        gfx_text(AREA_X + 70, 88, 1, COL_TEXT, info->name[LANG]);
        gfx_text_wrap(AREA_X + 28, 101, AREA_W - 56, 1, COL_TEXT, info->desc[LANG]);
        gfx_text(AREA_X + 28, 128, 1, COL_DIM, T(G_CROSS " Usar   " G_CIRCLE " Cancelar   < > Cambiar", G_CROSS " Use   " G_CIRCLE " Cancel   < > Switch"));
    }

    gfx_text(AREA_X + 2, 258, 1, COL_DIM,
             T(G_CROSS " Elegir " G_SQUARE " Jugar " G_TRIANGLE " Descartar L Orden R Trucos SEL Info", G_CROSS " Pick  " G_SQUARE " Play  " G_TRIANGLE " Discard  L Sort  R Tricks  SEL Info"));

    gfx_shake(0, 0);
    if (showInfo) draw_info();
    if (paused) draw_pause();
    draw_flash();
}

/* ------------------------------------------------------------ fin de ronda */

#define WIN_FIRST 10
#define WIN_STEP  12

static int win_lines(const char **label, int *amount)
{
    int n = 0;
    if (reward.base > 0)     { label[n] = T("Premio de la ronda", "Round prize"); amount[n++] = reward.base; }
    if (reward.hands > 0)    { label[n] = T("Jugadas sin usar", "Unused hands"); amount[n++] = reward.hands; }
    if (reward.interest > 0) { label[n] = T("Intereses ($1 por cada $5)", "Interest ($1 per $5)"); amount[n++] = reward.interest; }
    if (reward.relic > 0)    { label[n] = T("Amuletos", "Charms"); amount[n++] = reward.relic; }
    return n;
}

static void update_win(void)
{
    const char *label[4];
    int amount[4];
    int n = win_lines(label, amount);
    int totalAt = WIN_FIRST + n * WIN_STEP + 6;
    int t = stateAge - WIN_FIRST;

    /* las lineas van cayendo de una en una, como monedas */
    if (stateAge < totalAt && t >= 0 && t % WIN_STEP == 0 && t / WIN_STEP < n)
        synth_note(W_SINE, 988.0f * scale_note(t / WIN_STEP), 0.11f, 0.12f, 0.0f);
    if (stateAge == totalAt) {
        moneyHidden = 0;
        totalPop = 1.0f;
        moneyPulse = 1.0f;
        snd_buy();
        sparks(SCR_W * 0.5f, 140.0f, 22, COL_MONEY, 3.4f);
    }
    if (input_pressed(BTN_CROSS)) {
        if (stateAge < totalAt) {
            stateAge = totalAt - 1;      /* salta la animacion */
        } else if (stateAge > totalAt + 8) {
            if (game_next_round(&g)) {
                end_run(1);
                endSel = 0;
                set_state(ST_VICTORY);
                snd_win();
                confetti(60);
                flash = 0.7f;
            } else {
                game_shop_enter(&g);
                save_run(&g, RESUME_SHOP);
                enter_shop();
            }
        }
    }
}

static void draw_win(void)
{
    const char *label[4];
    int amount[4];
    char b[48];
    int n = win_lines(label, amount);
    int totalAt = WIN_FIRST + n * WIN_STEP + 6;
    int i, y = 84;

    draw_bg();
    popin_begin();
    draw_panel(110, 36, 260, 200);
    gfx_text_center(SCR_W / 2, 46, 2, COL_GOOD, T("¡Ronda superada!", "Round cleared!"));
    for (i = 0; i < n; i++) {
        int age = stateAge - (WIN_FIRST + i * WIN_STEP);
        if (age >= 0) {
            int ox = age < 6 ? (6 - age) * 5 : 0;
            gfx_text(130 - ox, y, 1, col_alpha(COL_TEXT, (float)(age + 1) / 6.0f), label[i]);
            snprintf(b, sizeof(b), "$%d", amount[i]);
            gfx_text_right(350 + ox, y, 1, col_alpha(COL_MONEY, (float)(age + 1) / 6.0f), b);
        }
        y += 14;
    }
    gfx_rect(130, y + 2, 220, 1, COL_BRASS);
    if (stateAge >= totalAt) {
        snprintf(b, sizeof(b), "+$%d", reward.total);
        gfx_xform(SCR_W * 0.5f, (float)(y + 25), 0.0f, 1.0f + totalPop * 0.7f);
        gfx_text_center(SCR_W / 2, y + 10, 3, col_lerp(COL_MONEY, COL_WHITE, totalPop), b);
        gfx_xform_off();
    }
    snprintf(b, sizeof(b), T("Tienes $%d", "You have $%d"), (int)(dispMoney + 0.5f));
    gfx_text_center(SCR_W / 2, 198, 1, COL_DIM, b);
    if (stateAge > totalAt + 8) gfx_text_center(SCR_W / 2, 218, 1, COL_TEXT, T(G_CROSS " Continuar", G_CROSS " Continue"));
    gfx_xform_off();
    draw_particles();
}

/* ----------------------------------------------------------------- tienda */

#define SHOP_ROW_H 12
#define SHOP_MAX_ROWS (SHOP_OFFERS + 1 + MAX_RELICS + MAX_TRICKS + 1)

static int build_rows(ShopRow *rows)
{
    int n = 0, i;
    for (i = 0; i < SHOP_OFFERS; i++) { rows[n].kind = ROW_OFFER; rows[n].idx = i; n++; }
    rows[n].kind = ROW_REROLL; rows[n].idx = 0; n++;
    for (i = 0; i < g.relicCount; i++) { rows[n].kind = ROW_SELL; rows[n].idx = i; n++; }
    for (i = 0; i < g.trickCount; i++) { rows[n].kind = ROW_TRICK; rows[n].idx = i; n++; }
    rows[n].kind = ROW_NEXT; rows[n].idx = 0; n++;
    return n;
}

static void shop_fail(const char *msg)
{
    toast(msg);
    snd_error();
    shake += 2.5f;
}

static void update_shop(void)
{
    ShopRow rows[SHOP_MAX_ROWS];
    ShopRow r;
    int n = build_rows(rows);
    float rowY;

    if (toastTimer > 0) toastTimer--;
    if (update_info()) return;

    if (shopSel >= n) shopSel = n - 1;
    if (input_repeat(BTN_UP))   { shopSel = (shopSel + n - 1) % n; snd_move(); selPulse = 1.0f; }
    if (input_repeat(BTN_DOWN)) { shopSel = (shopSel + 1) % n; snd_move(); selPulse = 1.0f; }
    rowY = (float)(39 + shopSel * SHOP_ROW_H);
    r = rows[shopSel];

    /* L y R cambian de sitio un amuleto propio: el orden importa al puntuar */
    if (r.kind == ROW_SELL && (input_pressed(BTN_L) || input_pressed(BTN_R))) {
        int to = game_move_relic(&g, r.idx, input_pressed(BTN_L) ? -1 : 1);
        if (to != r.idx) {
            shopSel += to - r.idx;
            relicPop[to] = 0.6f;
            selPulse = 1.0f;
            snd_whoosh();
        } else {
            snd_error();
        }
        return;
    }

    /* un truco propio se vende con Cuadrado y, si sirve fuera de la ronda, se usa con X */
    if (input_pressed(BTN_SQUARE) && r.kind == ROW_TRICK) {
        game_sell_trick(&g, r.idx);
        toast(T("Truco vendido", "Trick sold"));
        snd_money();
        moneyPulse = 1.0f;
        sparks(440.0f, rowY, 10, COL_GOOD, 2.6f);
        return;
    }
    if (!input_pressed(BTN_CROSS)) return;

    switch (r.kind) {
    case ROW_OFFER: {
        const Offer *o = &g.shop[r.idx];
        int kind = o->kind, id = o->id;
        switch (game_shop_buy(&g, r.idx)) {
        case BUY_OK:
            if (kind == OF_RELIC) save_relic_bought(id);
            if (kind == OF_TRICK) save_mark_trick_found(id);
            toast(T("¡Comprado!", "Bought!"));
            snd_buy();
            moneyPulse = 1.0f;
            selPulse = 1.0f;
            sparks(440.0f, rowY, 16, COL_MONEY, 3.0f);
            break;
        case BUY_NO_MONEY:
            shop_fail(T("No te llega el dinero", "Not enough money"));
            break;
        case BUY_NO_SPACE:
            shop_fail(kind == OF_RELIC ? T("No hay hueco: vende un amuleto", "No room: sell a charm")
                      : kind == OF_TRICK ? T("No te caben más trucos", "No room for more tricks") : T("El mazo está lleno", "Your deck is full"));
            break;
        default:
            shop_fail(T("Ya está vendido", "Already sold"));
            break;
        }
        break;
    }
    case ROW_REROLL:
        if (game_shop_reroll(&g)) {
            snd_whoosh();
            stateAge = 0;           /* las ofertas nuevas vuelven a entrar */
            moneyPulse = 1.0f;
        } else {
            shop_fail(T("No te llega el dinero", "Not enough money"));
        }
        break;
    case ROW_SELL:
        game_shop_sell(&g, r.idx);
        toast(T("Amuleto vendido", "Charm sold"));
        snd_money();
        moneyPulse = 1.0f;
        sparks(440.0f, rowY, 10, COL_GOOD, 2.6f);
        break;
    case ROW_TRICK:
        if (!game_trick_shop_usable(g.tricks[r.idx])) {
            shop_fail(T("Ese solo sirve durante una ronda", "That one only works in a round"));
        } else if (game_use_trick(&g, r.idx) == TRICK_OK) {
            toast(T("¡Truco usado!", "Trick used!"));
            snd_buy();
            moneyPulse = 1.0f;
            sparks(440.0f, rowY, 16, COL_TRICK, 3.0f);
        } else {
            shop_fail(T("Ahora no tiene efecto", "No effect right now"));
        }
        break;
    default:
        showInfo = 0;
        start_intro();
        break;
    }
}

static void draw_shop(void)
{
    ShopRow rows[SHOP_MAX_ROWS];
    char name[64], price[16], desc[200], tmp[24];
    int n = build_rows(rows), i, descArt = -1;
    float p;

    draw_bg();
    gfx_shake(shakeX, shakeY);
    gfx_text(14, 6, 2, COL_BRASS, T("TIENDA", "SHOP"));
    p = clampf(moneyPulse, 0.0f, 1.0f);
    snprintf(name, sizeof(name), "$%d", (int)(dispMoney + 0.5f));
    if (p > 0.02f) gfx_xform(440.0f, 16.0f, 0.0f, 1.0f + 0.25f * p);
    gfx_text_right(466, 6, 2, col_lerp(COL_MONEY, COL_WHITE, p), name);
    gfx_xform_off();
    if (g.endless) snprintf(name, sizeof(name), T("Siguiente: mesa %d (infinito), ronda %d", "Next: table %d (endless), round %d"), g.mesa + 1, g.round + 1);
    else snprintf(name, sizeof(name), T("Siguiente: mesa %d, ronda %d", "Next: table %d, round %d"), g.mesa + 1, g.round + 1);
    gfx_text_center(SCR_W / 2, 12, 1, COL_DIM, name);

    draw_panel(8, 30, 464, 186);
    desc[0] = 0;
    for (i = 0; i < n; i++) {
        ShopRow r = rows[i];
        const char *tag = "";
        unsigned int tagCol = COL_DIM, nameCol = COL_TEXT, priceCol = COL_MONEY;
        int y = 34 + i * SHOP_ROW_H, art = -1;
        float in = clampf((float)(stateAge - i * 2) / 9.0f, 0.0f, 1.0f);   /* entrada escalonada */
        int ox = -(int)((1.0f - in) * (1.0f - in) * 46.0f);
        char d[200];

        name[0] = 0; price[0] = 0; d[0] = 0;
        switch (r.kind) {
        case ROW_OFFER: {
            const Offer *o = &g.shop[r.idx];
            if (o->kind == OF_RELIC) {
                tag = T("Amuleto", "Charm"); tagCol = COL_BRASS;
                if (!o->sold) {
                    snprintf(name, sizeof(name), "%s", RELIC_INFO[o->id].name[LANG]);
                    snprintf(d, sizeof(d), "[%s] %s", RARITY_NAME[LANG][RELIC_INFO[o->id].rarity], RELIC_INFO[o->id].desc[LANG]);
                    nameCol = RARITY_COL[RELIC_INFO[o->id].rarity];
                    art = o->id;
                }
            } else if (o->kind == OF_TRICK) {
                tag = T("Truco", "Trick"); tagCol = COL_TRICK;
                snprintf(name, sizeof(name), "%s", TRICK_INFO[o->id].name[LANG]);
                snprintf(d, sizeof(d), T("De un solo uso (botón R en la ronda). %s", "Single use (R button in a round). %s"), TRICK_INFO[o->id].desc[LANG]);
                art = TRICK_ART(o->id);
            } else if (o->kind == OF_LESSON) {
                tag = T("Lección", "Lesson"); tagCol = COL_CHIPS;
                snprintf(name, sizeof(name), T("%s (nivel %d)", "%s (level %d)"), HAND_INFO[o->id].name[LANG], g.handLevel[o->id] + (o->sold ? 0 : 1));
                snprintf(d, sizeof(d), T("Sube de nivel %s: +%d fichas y +%d Multi de base.", "Levels up %s: +%d chips and +%d base Mult."),
                         HAND_INFO[o->id].name[LANG], HAND_INFO[o->id].upChips, HAND_INFO[o->id].upMult);
            } else {
                tag = T("Carta", "Card"); tagCol = COL_GOOD;
                if (o->card.notch) {
                    snprintf(name, sizeof(name), "%s%s %s + %s", RANK_STR[o->card.rank], SUIT_GLYPH[o->card.suit],
                             MARK_NAME[LANG][o->card.mark], NOTCH_NAME[LANG][o->card.notch]);
                    snprintf(d, sizeof(d), T("Carta para tu mazo. Marca de %s: %s Muesca de %s: %s", "Card for your deck. %s mark: %s %s notch: %s"),
                             MARK_NAME[LANG][o->card.mark], MARK_DESC[LANG][o->card.mark],
                             NOTCH_NAME[LANG][o->card.notch], NOTCH_DESC[LANG][o->card.notch]);
                } else {
                    snprintf(name, sizeof(name), "%s%s %s", RANK_STR[o->card.rank], SUIT_GLYPH[o->card.suit], MARK_NAME[LANG][o->card.mark]);
                    snprintf(d, sizeof(d), T("Añade a tu mazo un %s de %s con marca de %s: %s", "Adds a %s of %s with a %s mark to your deck: %s"),
                             RANK_STR[o->card.rank], SUIT_NAME[LANG][o->card.suit], MARK_NAME[LANG][o->card.mark], MARK_DESC[LANG][o->card.mark]);
                }
            }
            if (o->sold) {
                snprintf(name, sizeof(name), T("- vendido -", "- sold -"));
                nameCol = COL_DIM;
                d[0] = 0;
                art = -1;
            } else {
                snprintf(price, sizeof(price), "$%d", o->price);
                if (g.money < o->price) priceCol = COL_MULT;
            }
            break;
        }
        case ROW_REROLL:
            tag = T("Tienda", "Shop");
            snprintf(name, sizeof(name), T("Cambiar las ofertas", "Reroll the offers"));
            snprintf(price, sizeof(price), "$%d", g.rerollCost);
            if (g.money < g.rerollCost) priceCol = COL_MULT;
            snprintf(d, sizeof(d), T("Trae ofertas nuevas. Cada cambio cuesta $1 más.", "Brings new offers. Each reroll costs $1 more."));
            break;
        case ROW_SELL: {
            int id = g.relics[r.idx];
            tag = T("Tuyo", "Yours"); tagCol = COL_MULT;
            snprintf(name, sizeof(name), "%s", RELIC_INFO[id].name[LANG]);
            snprintf(price, sizeof(price), "+$%d", game_relic_sell_value(id));
            priceCol = COL_GOOD;
            snprintf(d, sizeof(d), T("%s  " G_CROSS " venderlo.  L/R lo cambian de sitio.", "%s  " G_CROSS " sell it.  L/R move it."),
                     RELIC_INFO[id].desc[LANG]);
            nameCol = RARITY_COL[RELIC_INFO[id].rarity];
            art = id;
            break;
        }
        case ROW_TRICK: {
            int id = g.tricks[r.idx];
            tag = T("Tuyo", "Yours"); tagCol = COL_TRICK;
            snprintf(name, sizeof(name), "%s", TRICK_INFO[id].name[LANG]);
            snprintf(price, sizeof(price), "+$%d", game_trick_sell_value(id));
            priceCol = COL_GOOD;
            snprintf(d, sizeof(d), T("%s  " G_SQUARE " venderlo%s", "%s  " G_SQUARE " sell it%s"), TRICK_INFO[id].desc[LANG],
                     game_trick_shop_usable(id) ? T(".  " G_CROSS " usarlo ahora.", ".  " G_CROSS " use it now.") : ".");
            art = TRICK_ART(id);
            break;
        }
        default:
            snprintf(name, sizeof(name), T("Ir a la siguiente ronda >", "Go to the next round >"));
            nameCol = COL_GOOD;
            snprintf(d, sizeof(d), T("Sal de la tienda. Cada $5 ahorrados dan $1 de interés (máximo $5).", "Leave the shop. Every $5 saved earns $1 interest (up to $5)."));
            break;
        }

        if (i == shopSel) {
            int grow = (int)(selPulse * 2.0f);
            gfx_rect(11, y - 1 - grow, 458, 11 + grow * 2, col_lerp(COL_BOX, RGB(96, 74, 60), selPulse));
            gfx_rect(11, y - 1 - grow, 3, 11 + grow * 2, COL_BRASS);
            strcpy(desc, d);
            descArt = art;
        }
        gfx_text(20 + ox, y, 1, col_alpha(tagCol, in), tag);
        gfx_text(78 + ox, y, 1, col_alpha(nameCol, in), name);
        gfx_text_right(460, y, 1, col_alpha(priceCol, in), price);
    }

    draw_panel(8, 218, 464, 36);
    if (descArt >= 0) {
        gfx_art(descArt, 11, 220, 2, COL_WHITE);
        if (desc[0]) gfx_text_wrap(50, 222, 416, 1, COL_TEXT, desc);
    } else if (desc[0]) {
        gfx_text_wrap(16, 222, 448, 1, COL_TEXT, desc);
    }
    snprintf(tmp, sizeof(tmp), "%d/%d", g.relicCount, MAX_RELICS);
    gfx_text(8, 259, 1, COL_DIM, T(G_CROSS " Comprar   SELECT Info y mazo   Amuletos:", G_CROSS " Buy   SELECT Info & deck   Charms:"));
    gfx_text(8 + gfx_text_width(T(G_CROSS " Comprar   SELECT Info y mazo   Amuletos: ", G_CROSS " Buy   SELECT Info & deck   Charms: "), 1), 259, 1, COL_TEXT, tmp);
    snprintf(tmp, sizeof(tmp), T("Trucos: %d/%d", "Tricks: %d/%d"), g.trickCount, game_pockets(&g));
    gfx_text_right(472, 259, 1, COL_TRICK, tmp);

    draw_particles();
    if (toastTimer > 0 && toastMsg) {
        int w = gfx_text_width(toastMsg, 1);
        gfx_rect(SCR_W / 2 - w / 2 - 6, 198, w + 12, 14, RGBA(0, 0, 0, 220));
        gfx_text_center(SCR_W / 2, 200, 1, COL_MONEY, toastMsg);
    }
    gfx_shake(0, 0);
    if (showInfo) draw_info();
}

/* ------------------------------------------------------- derrota/victoria */

/* Victoria: se puede seguir en modo infinito o dejarlo aqui */
static void update_end(void)
{
    if (state == ST_VICTORY) {
        if ((stateAge % 8) == 0) confetti(4);
        if (stateAge <= 40) return;
        if (input_repeat(BTN_UP) || input_repeat(BTN_DOWN)) { endSel ^= 1; snd_move(); selPulse = 1.0f; }
        if (input_pressed(BTN_CROSS)) {
            if (endSel == 0) {
                snd_start();
                enter_shop();              /* la partida ya quedo guardada en la tienda */
            } else {
                save_run_ended(&g);        /* suma sus estadisticas y borra la partida */
                set_state(ST_TITLE);
                snd_select(0);
            }
        }
        return;
    }
    if (stateAge > 30 && (input_pressed(BTN_CROSS) || input_pressed(BTN_START))) set_state(ST_TITLE);
}

/* La mano mas jugada en esta partida, o -1 */
static int most_played(const int *plays)
{
    int i, best = -1, max = 0;
    for (i = 0; i < HT_COUNT; i++)
        if (plays[i] > max) { max = plays[i]; best = i; }
    return best;
}

static void draw_end(int won)
{
    char b[96], n[40], t[40];
    int i, fav = most_played(g.handPlays);

    draw_bg();
    draw_particles();        /* el confeti cae por detras del panel para no tapar el texto */
    gfx_shake(shakeX, shakeY);
    popin_begin();
    draw_panel(90, 24, 300, 226);
    if (won) {
        gfx_text_center(SCR_W / 2, 32 + (int)(sinf((float)frame * 0.1f) * 3.0f), 3, COL_MONEY, T("¡VICTORIA!", "VICTORY!"));
        snprintf(b, sizeof(b), T("Has limpiado las %d mesas en %s.", "You cleared all %d tables on %s."),
                 NUM_MESAS, DIFF_INFO[g.difficulty].name[LANG]);
        gfx_text_center(SCR_W / 2, 68, 1, COL_TEXT, b);
    } else {
        gfx_text_center(SCR_W / 2, 32, 3, COL_MULT, T("FIN", "GAME OVER"));
        if (g.endless) snprintf(b, sizeof(b), T("Modo infinito: caíste en la mesa %d.", "Endless mode: you fell at table %d."), g.mesa + 1);
        else snprintf(b, sizeof(b), T("Caíste en la mesa %d, ronda %d.", "You fell at table %d, round %d."), g.mesa + 1, g.round + 1);
        gfx_text_center(SCR_W / 2, 64, 1, COL_TEXT, b);
        fmt_ll(n, g.score);
        fmt_ll(t, g.target);
        snprintf(b, sizeof(b), T("Puntos: %s de %s", "Score: %s of %s"), n, t);
        gfx_text_center(SCR_W / 2, 76, 1, COL_DIM, b);
    }

    /* resumen de la partida */
    fmt_ll(n, g.bestHand);
    snprintf(b, sizeof(b), T("Mejor jugada: %s", "Best hand: %s"), n);
    gfx_text_center(SCR_W / 2, 94, 1, COL_MONEY, b);
    snprintf(b, sizeof(b), T("Jugadas: %d   Descartes: %d   Trucos: %d", "Hands: %d   Discards: %d   Tricks: %d"),
             g.statHands, g.statDiscards, g.statTricks);
    gfx_text_center(SCR_W / 2, 108, 1, COL_TEXT, b);
    snprintf(b, sizeof(b), T("Dinero ganado: $%d   Amuletos: %d", "Money earned: $%d   Charms: %d"), g.statEarned, g.relicCount);
    gfx_text_center(SCR_W / 2, 120, 1, COL_TEXT, b);
    snprintf(b, sizeof(b), T("Mano más jugada: %s", "Most played hand: %s"), fav >= 0 ? HAND_INFO[fav].name[LANG] : "-");
    gfx_text_center(SCR_W / 2, 132, 1, COL_TEXT, b);
    snprintf(b, sizeof(b), T("Semilla: %08X", "Seed: %08X"), g.seed);
    gfx_text_center(SCR_W / 2, 146, 1, COL_DIM, b);

    if (newUnlock >= 0) {
        snprintf(b, sizeof(b), T("¡Nueva baraja: %s!", "New deck: %s!"), DECK_INFO[newUnlock].name[LANG]);
        gfx_text_center(SCR_W / 2, 164, 1, COL_GOOD, b);
    }
    if (newDiff > 0) {
        snprintf(b, sizeof(b), T("¡Nueva dificultad: %s!", "New difficulty: %s!"), DIFF_INFO[newDiff].name[LANG]);
        gfx_text_center(SCR_W / 2, 176, 1, COL_GOOD, b);
    }

    if (won) {
        if (stateAge > 40) {
            for (i = 0; i < 2; i++) {
                int y = 202 + i * 18;
                const char *label = i == 0 ? T("Seguir en modo infinito", "Keep going in endless mode")
                                           : T("Terminar aquí", "Stop here");
                if (i == endSel) {
                    int grow = (int)(selPulse * 3.0f);
                    gfx_rect(130 - grow, y - 3, 220 + grow * 2, 14, COL_BOX);
                    gfx_rect(130 - grow, y - 3, 3, 14, COL_BRASS);
                }
                gfx_text_center(SCR_W / 2, y, 1, i == endSel ? COL_TEXT : COL_DIM, label);
            }
        }
    } else if (stateAge > 30) {
        gfx_text_center(SCR_W / 2, 222, 1, COL_TEXT, T(G_CROSS " Volver al título", G_CROSS " Back to title"));
    }
    gfx_xform_off();
    gfx_shake(0, 0);
    draw_flash();
}

/* ---------------------------------------------------------------- publico */

static unsigned int art_color(char ch, unsigned int vcolor)
{
    int k;
    if (ch == 'v') return vcolor;
    for (k = 0; ART_KEYS[k]; k++)
        if (ART_KEYS[k] == ch) return RGB(ART_RGB[k][0], ART_RGB[k][1], ART_RGB[k][2]);
    return 0;
}

static void art_cell(int cell, int icon, unsigned int vcolor)
{
    int x, y;
    for (y = 0; y < ART_SIZE; y++)
        for (x = 0; x < ART_SIZE; x++)
            if (ART_ROWS[icon][y][x] != '.') gfx_art_put(cell, x, y, art_color(ART_ROWS[icon][y][x], vcolor));
}

/* Las ilustraciones se pasan a una textura al arrancar. Varios trucos comparten dibujo
   y solo cambian el color variable; los retratos de los rivales se montan por capas. */
#define NO_HAT 255

static void art_build(void)
{
    static const struct { unsigned char icon; unsigned int vcolor; } TRICK[T_COUNT] = {
        { ART_T_FRASCO, RGB(74, 74, 104) }, { ART_T_FRASCO, RGB(214, 52, 62) },
        { ART_T_FRASCO, RGB(64, 176, 96) }, { ART_T_FRASCO, RGB(238, 142, 42) },
        { ART_T_LIMA, 0 },
        { ART_T_BURIL, RGB(190, 198, 214) }, { ART_T_BURIL, RGB(214, 52, 62) },
        { ART_T_BURIL, RGB(130, 224, 238) }, { ART_T_BURIL, RGB(246, 206, 82) },
        { ART_T_MUESCA, RGB(150, 84, 204) }, { ART_T_MUESCA, RGB(44, 74, 150) }, { ART_T_MUESCA, RGB(30, 150, 92) },
        { ART_T_CIZALLA, 0 }, { ART_T_GEMELO, 0 }, { ART_T_SOPLO, 0 }, { ART_T_RELOJ, 0 },
        { ART_T_SOBRE, 0 }, { ART_T_CHULETA, 0 },
    };
    /* rival: sombrero, ojos, boca, color de la ropa y del sombrero */
    static const struct { unsigned char hat, eyes, mouth; unsigned int shirt, hatCol; } BOSS[B_COUNT] = {
        { NO_HAT, ART_B_OJOS, ART_B_SERIO, 0, 0 },                                                 /* sin rival */
        { ART_B_BOINA, ART_B_OJOS, ART_B_SERIO, RGB(120, 90, 60), RGB(40, 90, 60) },               /* Manco */
        { NO_HAT, ART_B_OJOS, ART_B_BIGOTE, RGB(200, 60, 50), 0 },                                 /* Impaciente */
        { ART_B_COPA, ART_B_MONOCULO, ART_B_SERIO, RGB(110, 110, 120), 0 },                        /* Tacano */
        { ART_B_PANUELO, ART_B_CERRADOS, ART_B_SERIO, RGB(110, 70, 150), RGB(140, 80, 190) },      /* Supersticioso */
        { ART_B_BOINA, ART_B_OJOS, ART_B_BIGOTE, RGB(180, 40, 50), RGB(200, 40, 50) },             /* Republicano */
        { ART_B_VISERA, ART_B_GAFAS, ART_B_SONRISA, RGB(30, 100, 70), 0 },                         /* Banca */
        { ART_B_COPA, ART_B_MONOCULO, ART_B_BIGOTE, RGB(40, 50, 110), 0 },                         /* Estirado */
        { ART_B_VISERA, ART_B_GAFAS, ART_B_SERIO, RGB(80, 80, 90), 0 },                            /* Recaudador */
        { ART_B_PANUELO, ART_B_PARCHE, ART_B_SONRISA, RGB(50, 40, 50), RGB(200, 50, 60) },         /* Tuerto */
        { ART_B_PELO, ART_B_GAFAS, ART_B_SERIO, RGB(220, 220, 210), 0 },                           /* Maniatico */
        { ART_B_BOINA, ART_B_CERRADOS, ART_B_SONRISA, RGB(190, 170, 130), RGB(70, 100, 180) },     /* Olvidadizo */
        { ART_B_PELO, ART_B_CERRADOS, ART_B_SERIO, RGB(120, 120, 130), 0 },                        /* Aburrido */
        { ART_B_VISERA, ART_B_OJOS, ART_B_PURO, RGB(230, 140, 40), 0 },                            /* Peajero */
        { ART_B_BOINA, ART_B_ANTIFAZ, ART_B_SONRISA, RGB(40, 40, 48), RGB(70, 70, 84) },           /* Ladron */
        { ART_B_COPA, ART_B_OJOS, ART_B_SERIO, RGB(235, 235, 240), 0 },                            /* Purista */
        { ART_B_PANUELO, ART_B_CERRADOS, ART_B_SONRISA, RGB(240, 200, 70), RGB(90, 140, 220) },    /* Madrugon */
        { ART_B_PELO, ART_B_BICOLOR, ART_B_SONRISA, RGB(40, 150, 150), 0 },                        /* Daltonico */
    };
    int i;
    gfx_art_clear();
    for (i = 0; i < R_COUNT; i++) art_cell(i, i, 0);
    for (i = 0; i < T_COUNT; i++) art_cell(TRICK_ART(i), TRICK[i].icon, TRICK[i].vcolor);
    for (i = 1; i < B_COUNT; i++) {
        int cell = BOSS_ART(i);
        art_cell(cell, ART_B_CARA, BOSS[i].shirt);
        if (BOSS[i].hat != NO_HAT) art_cell(cell, BOSS[i].hat, BOSS[i].hatCol);
        art_cell(cell, BOSS[i].eyes, 0);
        art_cell(cell, BOSS[i].mouth, 0);
    }
    gfx_art_commit();
}

/* Cada momento del juego tiene su musica */
static void music_update(void)
{
    int t;
    switch (state) {
    case ST_INTRO:
    case ST_PLAY:
        t = g.bossRule != B_NONE ? TRACK_BOSS : TRACK_PLAY;
        break;
    case ST_WIN:
    case ST_SHOP:
    case ST_VICTORY:
        t = TRACK_SHOP;
        break;
    default:
        t = TRACK_TITLE;
        break;
    }
    synth_set_track(t);
}

void ui_init(void)
{
    int i;
    memset(&g, 0, sizeof(g));
    memset(&res, 0, sizeof(res));
    art_build();
    SEMI[0] = 1.0f;
    for (i = 1; i < 49; i++) SEMI[i] = SEMI[i - 1] * 1.0594631f;
    save_init();
    options_load();
#ifdef AUTOTEST
    game_lang = (AUTOTEST == 4) ? LANG_EN : LANG_ES;    /* la variante 4 comprueba el juego en ingles */
#endif
    newUnlock = -1;
    state = ST_TITLE;
    phase = PH_SELECT;
    frame = 0;
    timer = 0;
    stateAge = 0;
    toastMsg = NULL;
    toastTimer = 0;
}

void ui_update(void)
{
    frame++;
    stateAge++;
    fx_update();
    music_update();
    switch (state) {
    case ST_TITLE:      update_title(); break;
    case ST_OPTIONS:    update_options(); break;
    case ST_DECK:       update_deck(); break;
    case ST_COLLECTION: update_collection(); break;
    case ST_INTRO:      update_intro(); break;
    case ST_PLAY:       update_play();  break;
    case ST_WIN:        update_win();   break;
    case ST_SHOP:       update_shop();  break;
    default:            update_end();   break;
    }
}

void ui_draw(void)
{
    switch (state) {
    case ST_TITLE:      draw_title(); break;
    case ST_OPTIONS:    draw_options(); break;
    case ST_DECK:       draw_deck(); break;
    case ST_COLLECTION: draw_collection(); break;
    case ST_INTRO:      draw_intro(); break;
    case ST_PLAY:       draw_play();  break;
    case ST_WIN:        draw_win();   break;
    case ST_SHOP:       draw_shop();  break;
    case ST_GAMEOVER:   draw_end(0);  break;
    default:            draw_end(1);  break;
    }
}

#ifdef AUTOTEST
#include "autotest.h"
#endif
