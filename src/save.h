/* FAROL - guardado: perfil del jugador y partida en curso (independiente de la plataforma) */
#ifndef SAVE_H
#define SAVE_H

#include "game.h"

enum { RESUME_INTRO, RESUME_SHOP };
enum { LANG_AUTO, LANG_SET_ES, LANG_SET_EN };       /* valor guardado del idioma */

typedef struct {
    unsigned int runs, wins;
    int bestMesa;                       /* mesas superadas en la mejor partida (puede pasar de 8 en modo infinito) */
    int maxDifficulty;                  /* dificultad mas alta desbloqueada */
    long long bestHand;
    /* estadisticas acumuladas de todas las partidas terminadas */
    unsigned int totalHands, totalDiscards, totalRounds, totalTricks, totalEarned;
    unsigned int handPlays[HT_COUNT];
    unsigned short relicBought[R_COUNT];
    unsigned char found[(R_COUNT + 7) / 8];        /* amuletos descubiertos (comprados alguna vez) */
    unsigned char trickFound[(T_COUNT + 7) / 8];   /* trucos descubiertos */
    unsigned char musicVol, sfxVol;                /* 0..10 */
    unsigned char lang;                            /* LANG_AUTO, LANG_SET_ES o LANG_SET_EN */
} Profile;

/* Carga el guardado. Si falta o esta danado, empieza con un perfil vacio. */
void save_init(void);

const Profile *save_profile(void);
void save_set_options(int musicVol, int sfxVol, int lang);

/* Coleccion. Marcar no escribe en disco: se guarda con el siguiente save_run. */
int  save_is_found(int relic);
int  save_found_count(void);
void save_relic_bought(int relic);      /* lo marca como descubierto y cuenta la compra */
int  save_is_trick_found(int trick);
int  save_trick_found_count(void);
void save_mark_trick_found(int trick);

int  save_deck_unlocked(int deck);
int  save_difficulty_unlocked(int difficulty);

/* Partida en curso */
int  save_has_run(void);
void save_run(const Game *g, int resume);
int  save_load_run(Game *g, int *resume);
void save_clear_run(void);
void save_run_won(const Game *g);       /* victoria: actualiza records; no escribe ni borra la partida */
void save_run_ended(const Game *g);     /* fin de la partida: records, estadisticas y partida borrada */

/* Las proporciona la plataforma. read/write devuelven bytes, o <= 0 si fallan.
   label recibe el texto que acompana al guardado en el menu de la consola. */
int  plat_storage_read(void *buf, int size);
int  plat_storage_write(const void *buf, int size);
void plat_storage_label(const char *detail);

#endif
