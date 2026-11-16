/* FAROL - entrada */
#include <pspctrl.h>
#include "input.h"
#ifdef AUTOTEST
#include "ui.h"
#endif

#define DPAD (BTN_UP | BTN_DOWN | BTN_LEFT | BTN_RIGHT)

static unsigned int cur, prev, rep;
static int holdFrames;

void input_init(void)
{
    sceCtrlSetSamplingCycle(0);
    sceCtrlSetSamplingMode(PSP_CTRL_MODE_ANALOG);
    cur = prev = rep = 0;
    holdFrames = 0;
}

void input_update(void)
{
    SceCtrlData pad;
    unsigned int b = 0, d;

    pad.Buttons = 0;
    pad.Lx = 128;
    pad.Ly = 128;
    sceCtrlPeekBufferPositive(&pad, 1);

    if (pad.Buttons & PSP_CTRL_UP)       b |= BTN_UP;
    if (pad.Buttons & PSP_CTRL_DOWN)     b |= BTN_DOWN;
    if (pad.Buttons & PSP_CTRL_LEFT)     b |= BTN_LEFT;
    if (pad.Buttons & PSP_CTRL_RIGHT)    b |= BTN_RIGHT;
    if (pad.Buttons & PSP_CTRL_CROSS)    b |= BTN_CROSS;
    if (pad.Buttons & PSP_CTRL_CIRCLE)   b |= BTN_CIRCLE;
    if (pad.Buttons & PSP_CTRL_SQUARE)   b |= BTN_SQUARE;
    if (pad.Buttons & PSP_CTRL_TRIANGLE) b |= BTN_TRIANGLE;
    if (pad.Buttons & PSP_CTRL_LTRIGGER) b |= BTN_L;
    if (pad.Buttons & PSP_CTRL_RTRIGGER) b |= BTN_R;
    if (pad.Buttons & PSP_CTRL_START)    b |= BTN_START;
    if (pad.Buttons & PSP_CTRL_SELECT)   b |= BTN_SELECT;

    /* el stick analogico tambien mueve el cursor */
    if (pad.Lx < 48)  b |= BTN_LEFT;
    if (pad.Lx > 208) b |= BTN_RIGHT;
    if (pad.Ly < 48)  b |= BTN_UP;
    if (pad.Ly > 208) b |= BTN_DOWN;

#ifdef AUTOTEST
    b = ui_autotest_buttons();   /* el bot sustituye al mando */
#endif
    prev = cur;
    cur = b;
    rep = cur & ~prev;

    d = cur & DPAD;
    if (d && d == (prev & DPAD)) {
        holdFrames++;
        if (holdFrames >= 16 && ((holdFrames - 16) % 4) == 0) rep |= d;
    } else {
        holdFrames = 0;
    }
}

int input_held(unsigned int b)    { return (cur & b) != 0; }
int input_pressed(unsigned int b) { return (cur & ~prev & b) != 0; }
int input_repeat(unsigned int b)  { return (rep & b) != 0; }
