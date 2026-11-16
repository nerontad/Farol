/* FAROL - pantallas y flujo del juego */
#ifndef UI_H
#define UI_H

void ui_init(void);
void ui_update(void);
void ui_draw(void);

#ifdef AUTOTEST
#ifndef AUTOTEST_SEED
#define AUTOTEST_SEED 12345u
#endif
unsigned int ui_autotest_buttons(void);
const char *ui_autotest_shot(void);
int ui_autotest_done(void);
int ui_autotest_summary(char *b, int n);
#endif

/* la proporciona la plataforma (main.c) */
unsigned int plat_ticks(void);

#endif
