/* FAROL - guardado: perfil del jugador y partida en curso (independiente de la plataforma) */
#include <stdio.h>
#include <string.h>
#include "save.h"

#define SAVE_MAGIC   0x4C524146u   /* "FARL" */
#define SAVE_VERSION 4u

typedef struct {
    unsigned int magic, version, size, checksum;
    Profile profile;
    int hasRun, resume;
    unsigned int rng;
    Game game;
} SaveData;

static SaveData data;

/* FNV-1a sobre todo lo que va detras de la cabecera */
static unsigned int checksum(const SaveData *d)
{
    const unsigned char *p = (const unsigned char *)&d->profile;
    unsigned int n = (unsigned int)(sizeof(SaveData) - ((const unsigned char *)&d->profile - (const unsigned char *)d));
    unsigned int h = 2166136261u, i;
    for (i = 0; i < n; i++) {
        h ^= p[i];
        h *= 16777619u;
    }
    return h;
}

static void reset(void)
{
    memset(&data, 0, sizeof(data));
    data.magic = SAVE_MAGIC;
    data.version = SAVE_VERSION;
    data.size = (unsigned int)sizeof(SaveData);
}

static int bit_get(const unsigned char *bits, int i) { return (bits[i / 8] >> (i % 8)) & 1; }
static void bit_set(unsigned char *bits, int i) { bits[i / 8] |= (unsigned char)(1 << (i % 8)); }

static void flush(void)
{
    char detail[200];
    int n;

    n = snprintf(detail, sizeof(detail), "Mesas superadas: %d. Amuletos: %d/%d.",
                 data.profile.bestMesa, save_found_count(), R_COUNT);
    if (data.hasRun && n > 0 && n < (int)sizeof(detail))
        snprintf(detail + n, sizeof(detail) - (size_t)n, " Partida en curso: mesa %d, ronda %d.",
                 data.game.mesa + 1, data.game.round + 1);
    plat_storage_label(detail);

    data.magic = SAVE_MAGIC;
    data.version = SAVE_VERSION;
    data.size = (unsigned int)sizeof(SaveData);
    data.checksum = checksum(&data);
    plat_storage_write(&data, (int)sizeof(data));
}

void save_init(void)
{
    static SaveData tmp;
    int n;

    reset();
    memset(&tmp, 0, sizeof(tmp));
    n = plat_storage_read(&tmp, (int)sizeof(tmp));
    if (n != (int)sizeof(tmp)) return;
    if (tmp.magic != SAVE_MAGIC || tmp.version != SAVE_VERSION || tmp.size != sizeof(SaveData)) return;
    if (tmp.checksum != checksum(&tmp)) return;

    /* comprobaciones minimas para no cargar una partida imposible */
    if (tmp.hasRun) {
        const Game *g = &tmp.game;
        if (g->deckCount < 1 || g->deckCount > MAX_DECK || g->relicCount < 0 || g->relicCount > MAX_RELICS
            || g->trickCount < 0 || g->trickCount > MAX_TRICKS || g->mesa < 0 || g->mesa > 100000
            || g->round < 0 || g->round > 2 || g->money < 0
            || g->difficulty < 0 || g->difficulty >= DIFF_COUNT)
            tmp.hasRun = 0;
    }
    if (tmp.profile.maxDifficulty < 0 || tmp.profile.maxDifficulty >= DIFF_COUNT) tmp.profile.maxDifficulty = 0;
    data = tmp;
}

const Profile *save_profile(void)
{
    return &data.profile;
}

void save_set_music(int on)
{
    data.profile.musicOff = (unsigned char)(on ? 0 : 1);
    flush();
}

int save_is_found(int relic)
{
    return relic >= 0 && relic < R_COUNT && bit_get(data.profile.found, relic);
}

int save_found_count(void)
{
    int i, n = 0;
    for (i = 0; i < R_COUNT; i++) n += save_is_found(i);
    return n;
}

void save_mark_found(int relic)
{
    if (relic >= 0 && relic < R_COUNT) bit_set(data.profile.found, relic);
}

int save_is_trick_found(int trick)
{
    return trick >= 0 && trick < T_COUNT && bit_get(data.profile.trickFound, trick);
}

int save_trick_found_count(void)
{
    int i, n = 0;
    for (i = 0; i < T_COUNT; i++) n += save_is_trick_found(i);
    return n;
}

void save_mark_trick_found(int trick)
{
    if (trick >= 0 && trick < T_COUNT) bit_set(data.profile.trickFound, trick);
}

int save_deck_unlocked(int deck)
{
    switch (deck) {
    case DECK_CLASICA: return 1;
    case DECK_CORTA:   return data.profile.bestMesa >= 1;
    case DECK_REAL:    return data.profile.bestMesa >= 2;
    case DECK_MARCADA: return save_found_count() >= 12;
    case DECK_AVARO:   return data.profile.bestMesa >= 4;
    default:           return 0;
    }
}

int save_difficulty_unlocked(int difficulty)
{
    return difficulty >= 0 && difficulty <= data.profile.maxDifficulty;
}

int save_has_run(void)
{
    return data.hasRun;
}

void save_run(const Game *g, int resume)
{
    data.game = *g;
    data.hasRun = 1;
    data.resume = resume;
    data.rng = rng_get();
    flush();
}

int save_load_run(Game *g, int *resume)
{
    if (!data.hasRun) return 0;
    *g = data.game;
    *resume = data.resume;
    rng_set(data.rng);
    return 1;
}

void save_clear_run(void)
{
    data.hasRun = 0;
    memset(&data.game, 0, sizeof(data.game));
    flush();
}

static void update_bests(const Game *g, int cleared)
{
    Profile *p = &data.profile;
    if (cleared > p->bestMesa) p->bestMesa = cleared;
    if (g->bestHand > p->bestHand) p->bestHand = g->bestHand;
}

void save_run_won(const Game *g)
{
    Profile *p = &data.profile;
    p->runs++;
    p->wins++;
    update_bests(g, NUM_MESAS);
    /* ganar en una dificultad abre la siguiente */
    if (g->difficulty + 1 < DIFF_COUNT && g->difficulty + 1 > p->maxDifficulty) p->maxDifficulty = g->difficulty + 1;
}

void save_run_ended(const Game *g)
{
    /* una partida que ya gano y siguio en modo infinito ya se conto al ganar */
    if (!g->endless) data.profile.runs++;
    update_bests(g, g->mesa);
    save_clear_run();
}
