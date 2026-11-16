/* FAROL - logica del juego (independiente de la plataforma) */
#include <string.h>
#include "game.h"
#include "gfx.h"   /* solo por las macros de simbolos G_* usadas en los textos */

/* ------------------------------------------------- motor de reglas: tipos */

/* Cuando salta una regla */
enum {
    W_NONE,
    W_CARD,      /* por cada carta que puntua */
    W_KICKER,    /* por cada carta jugada que no puntua */
    W_HELD,      /* por cada carta que se queda en la mano */
    W_END,       /* al final de la jugada, tras las cartas */
    W_WIN,       /* al superar la ronda */
    W_DISCARD,   /* al descartar */
    W_PASSIVE,   /* siempre activa: cambia las reglas de la ronda */
    W_RETRIG     /* decide que cartas se cuentan otra vez */
};

/* Condicion. Las de carta miran la carta en curso; las demas, la jugada entera */
enum {
    C_ALWAYS,
    C_SUIT, C_FACE, C_ACE, C_PRIME, C_LOW, C_HIGH, C_RANK, C_MARKED, C_FIRST_DIAMOND,
    C_HAS_PAIR, C_HAS_TRIPS, C_HAS_QUADS, C_STRAIGHT, C_FLUSH, C_TYPE, C_MAX3, C_FIVE_CARDS,
    C_LAST_HAND, C_FIRST_HAND, C_NO_DISCARD, C_ONE_COLOR, C_FOUR_SUIT, C_KING_QUEEN, C_NO_FACE,
    C_CHANCE, C_LAST_SCORING, C_FIRST_SCORING
};

/* Efecto */
enum {
    E_CHIPS, E_MULT, E_XMULT /* val en decimas */, E_MONEY, E_CARD_VALUE,
    E_MULT_PER_MONEY, E_MULT_PER_DISCARD, E_MULT_PER_HAND, E_MULT_PER_RELIC,
    E_MULT_COUNTER, E_MULT_DICE, E_MULT_THIN, E_COUNTER_ADD,
    E_MONEY_PER_DISCARD, E_MONEY_PER_10,
    E_HANDSIZE, E_HANDS, E_DISCARDS, E_POCKETS, E_DISCOUNT, E_REPEAT
};

/* ------------------------------------------------------------------ datos */

const HandInfo HAND_INFO[HT_COUNT] = {
    { "Carta alta",         10,  1, 10, 1 },
    { "Pareja",             20,  2, 15, 1 },
    { "Doble pareja",       30,  2, 20, 1 },
    { "Trío",               40,  3, 20, 2 },
    { "Escalera",           50,  4, 30, 2 },
    { "Color",              55,  4, 25, 2 },
    { "Full",               60,  5, 30, 2 },
    { "Póker",              80,  7, 35, 3 },
    { "Escalera de color", 120,  8, 45, 4 },
    { "Repóker",           140, 12, 50, 4 },
};

/* Cada amuleto es solo datos: nombre, texto, precio, rareza y hasta dos reglas.
   Anadir uno nuevo es anadir una fila (y su identificador en game.h). */
const RelicInfo RELIC_INFO[R_COUNT] = {
    { "Herradura",       "+4 Multi en cada jugada.", 4, RAR_COMMON,
      { { W_END, C_ALWAYS, 0, E_MULT, 4 } } },
    { "Trébol seco",     "Cada " G_CLUB " que puntúa: +3 Multi.", 5, RAR_COMMON,
      { { W_CARD, C_SUIT, SUIT_CLUBS, E_MULT, 3 } } },
    { "Relicario",       "Cada " G_HEART " que puntúa: +3 Multi.", 5, RAR_COMMON,
      { { W_CARD, C_SUIT, SUIT_HEARTS, E_MULT, 3 } } },
    { "Puñal",           "Cada " G_SPADE " que puntúa: +25 fichas.", 5, RAR_COMMON,
      { { W_CARD, C_SUIT, SUIT_SPADES, E_CHIPS, 25 } } },
    { "Lupa de joyero",  "El primer " G_DIAMOND " que puntúa en cada jugada da $2.", 5, RAR_COMMON,
      { { W_CARD, C_FIRST_DIAMOND, 0, E_MONEY, 2 } } },
    { "Gemelos",         "Si la jugada contiene una pareja: +8 Multi.", 5, RAR_COMMON,
      { { W_END, C_HAS_PAIR, 0, E_MULT, 8 } } },
    { "Trípode",         "Si la jugada contiene un trío: +14 Multi.", 6, RAR_COMMON,
      { { W_END, C_HAS_TRIPS, 0, E_MULT, 14 } } },
    { "Peldaño",         "Si la jugada es escalera: +100 fichas.", 6, RAR_COMMON,
      { { W_END, C_STRAIGHT, 0, E_CHIPS, 100 } } },
    { "Tinte",           "Si la jugada es color: x2 Multi.", 7, RAR_RARE,
      { { W_END, C_FLUSH, 0, E_XMULT, 20 } } },
    { "Navaja",          "Si juegas 3 cartas o menos: +12 Multi.", 5, RAR_COMMON,
      { { W_END, C_MAX3, 0, E_MULT, 12 } } },
    { "Última bala",     "En la última jugada de la ronda: x3 Multi.", 7, RAR_RARE,
      { { W_END, C_LAST_HAND, 0, E_XMULT, 30 } } },
    { "Usurero",         "+1 Multi por cada $3 que tengas.", 6, RAR_RARE,
      { { W_END, C_ALWAYS, 0, E_MULT_PER_MONEY, 3 } } },
    { "Corona",          "Cada figura (J, Q, K) que puntúa: +4 Multi.", 6, RAR_COMMON,
      { { W_CARD, C_FACE, 0, E_MULT, 4 } } },
    { "Números primos",  "Cada 2, 3, 5 o 7 que puntúa: +6 Multi.", 5, RAR_COMMON,
      { { W_CARD, C_PRIME, 0, E_MULT, 6 } } },
    { "Reloj de arena",  "+5 Multi por cada descarte que te quede.", 5, RAR_COMMON,
      { { W_END, C_ALWAYS, 0, E_MULT_PER_DISCARD, 5 } } },
    { "Bola de nieve",   "+2 Multi por cada ronda ganada mientras la tengas.", 6, RAR_RARE,
      { { W_WIN, C_ALWAYS, 0, E_COUNTER_ADD, 1 }, { W_END, C_ALWAYS, 0, E_MULT_COUNTER, 2 } } },
    { "Vitrina",         "+5 Multi por cada amuleto que tengas.", 6, RAR_RARE,
      { { W_END, C_ALWAYS, 0, E_MULT_PER_RELIC, 5 } } },
    { "Eco",             "La última carta que puntúa se cuenta dos veces.", 6, RAR_RARE,
      { { W_RETRIG, C_LAST_SCORING, 0, E_REPEAT, 1 } } },
    { "Propina",         "Ganas $4 extra al superar cada ronda.", 5, RAR_COMMON,
      { { W_WIN, C_ALWAYS, 0, E_MONEY, 4 } } },
    { "Guante",          "+1 carta en la mano.", 6, RAR_RARE,
      { { W_PASSIVE, C_ALWAYS, 0, E_HANDSIZE, 1 } } },
    { "Manga ancha",     "+1 descarte en cada ronda.", 5, RAR_COMMON,
      { { W_PASSIVE, C_ALWAYS, 0, E_DISCARDS, 1 } } },
    { "Segundo aliento", "+1 jugada en cada ronda.", 7, RAR_RARE,
      { { W_PASSIVE, C_ALWAYS, 0, E_HANDS, 1 } } },
    { "Bicolor",         "Si todo lo jugado es rojo o todo negro: x1,5 Multi.", 6, RAR_COMMON,
      { { W_END, C_ONE_COLOR, 0, E_XMULT, 15 } } },
    { "Farol",           "Si la jugada es carta alta: x4 Multi.", 6, RAR_RARE,
      { { W_END, C_TYPE, HT_HIGH, E_XMULT, 40 } } },
    { "Dado cargado",    "Cada jugada suma entre +3 y +18 Multi al azar.", 5, RAR_COMMON,
      { { W_END, C_ALWAYS, 0, E_MULT_DICE, 3 } } },
    { "As en la manga",  "Cada As que puntúa: +50 fichas.", 4, RAR_COMMON,
      { { W_CARD, C_ACE, 0, E_CHIPS, 50 } } },

    { "Banquillo",       "Cada figura que se queda en tu mano: +3 Multi.", 5, RAR_COMMON,
      { { W_HELD, C_FACE, 0, E_MULT, 3 } } },
    { "Lastre",          "Cada carta que se queda en tu mano: +8 fichas.", 4, RAR_COMMON,
      { { W_HELD, C_ALWAYS, 0, E_CHIPS, 8 } } },
    { "Calderilla",      "Cada carta del 2 al 6 que puntúa: +18 fichas.", 4, RAR_COMMON,
      { { W_CARD, C_LOW, 0, E_CHIPS, 18 } } },
    { "Alta sociedad",   "Cada 10, J, Q, K o As que puntúa: +2 Multi.", 5, RAR_COMMON,
      { { W_CARD, C_HIGH, 0, E_MULT, 2 } } },
    { "Primera impresión", "En la primera jugada de la ronda: +60 fichas y +6 Multi.", 5, RAR_COMMON,
      { { W_END, C_FIRST_HAND, 0, E_CHIPS, 60 }, { W_END, C_FIRST_HAND, 0, E_MULT, 6 } } },
    { "Mano limpia",     "Si aún no has descartado en esta ronda: x2 Multi.", 8, RAR_EPIC,
      { { W_END, C_NO_DISCARD, 0, E_XMULT, 20 } } },
    { "Doblete",         "Si la jugada es doble pareja: +20 Multi.", 5, RAR_COMMON,
      { { W_END, C_TYPE, HT_TWOPAIR, E_MULT, 20 } } },
    { "Casa llena",      "Si la jugada es full: x2,5 Multi.", 8, RAR_EPIC,
      { { W_END, C_TYPE, HT_FULL, E_XMULT, 25 } } },
    { "Cuarteto",        "Si la jugada contiene cuatro cartas iguales: x3 Multi.", 8, RAR_EPIC,
      { { W_END, C_HAS_QUADS, 0, E_XMULT, 30 } } },
    { "Ojo clínico",     "Cada carta con marca que puntúa: +5 Multi.", 6, RAR_RARE,
      { { W_CARD, C_MARKED, 0, E_MULT, 5 } } },
    { "Hucha",           "Al superar la ronda: $1 por cada descarte sin usar.", 5, RAR_COMMON,
      { { W_WIN, C_ALWAYS, 0, E_MONEY_PER_DISCARD, 1 } } },
    { "Trapero",         "Gana +1 Multi para siempre cada vez que descartas.", 6, RAR_RARE,
      { { W_DISCARD, C_ALWAYS, 0, E_COUNTER_ADD, 1 }, { W_END, C_ALWAYS, 0, E_MULT_COUNTER, 1 } } },
    { "Metrónomo",       "Gana +1 Multi para siempre después de cada jugada.", 6, RAR_RARE,
      { { W_END, C_ALWAYS, 0, E_MULT_COUNTER, 1 }, { W_END, C_ALWAYS, 0, E_COUNTER_ADD, 1 } } },
    { "Mazo fino",       "+4 Multi por cada carta que le falte a tu mazo para llegar a 52.", 6, RAR_RARE,
      { { W_END, C_ALWAYS, 0, E_MULT_THIN, 4 } } },
    { "Palo mayor",      "Si juegas 4 o más cartas del mismo palo: +10 Multi.", 5, RAR_COMMON,
      { { W_END, C_FOUR_SUIT, 0, E_MULT, 10 } } },
    { "Ruleta",          "En cada jugada, 1 posibilidad entre 4 de x3 Multi.", 6, RAR_RARE,
      { { W_END, C_CHANCE, 4, E_XMULT, 30 } } },
    { "Caja fuerte",     "Al superar la ronda: $1 por cada $10 que tengas.", 5, RAR_COMMON,
      { { W_WIN, C_ALWAYS, 0, E_MONEY_PER_10, 1 } } },
    { "Séptimo cielo",   "Cada 7 que puntúa: +7 Multi.", 5, RAR_COMMON,
      { { W_CARD, C_RANK, 7, E_MULT, 7 } } },
    { "Pareja real",     "Si puntúan a la vez una K y una Q: x2 Multi.", 7, RAR_RARE,
      { { W_END, C_KING_QUEEN, 0, E_XMULT, 20 } } },
    { "Plebe",           "Si no juegas ninguna figura: +9 Multi.", 5, RAR_COMMON,
      { { W_END, C_NO_FACE, 0, E_MULT, 9 } } },
    { "Mano llena",      "Si juegas 5 cartas: +40 fichas.", 4, RAR_COMMON,
      { { W_END, C_FIVE_CARDS, 0, E_CHIPS, 40 } } },
    { "Preludio",        "La primera carta que puntúa se cuenta dos veces.", 6, RAR_RARE,
      { { W_RETRIG, C_FIRST_SCORING, 0, E_REPEAT, 1 } } },
    { "Suplente",        "Las cartas jugadas que no puntúan dan igualmente sus fichas.", 4, RAR_COMMON,
      { { W_KICKER, C_ALWAYS, 0, E_CARD_VALUE, 0 } } },
    { "Bolsillo secreto", "+1 hueco para trucos.", 4, RAR_COMMON,
      { { W_PASSIVE, C_ALWAYS, 0, E_POCKETS, 1 } } },
    { "Regateo",         "Todo cuesta $1 menos en la tienda.", 6, RAR_RARE,
      { { W_PASSIVE, C_ALWAYS, 0, E_DISCOUNT, 1 } } },
    { "Racha",           "+3 Multi por cada jugada que te quede después de esta.", 5, RAR_COMMON,
      { { W_END, C_ALWAYS, 0, E_MULT_PER_HAND, 3 } } },
};

const TrickInfo TRICK_INFO[T_COUNT] = {
    { "Tinte " G_SPADE,    "Convierte hasta 3 cartas elegidas en picas.", 3, 1, 3 },
    { "Tinte " G_HEART,    "Convierte hasta 3 cartas elegidas en corazones.", 3, 1, 3 },
    { "Tinte " G_CLUB,     "Convierte hasta 3 cartas elegidas en tréboles.", 3, 1, 3 },
    { "Tinte " G_DIAMOND,  "Convierte hasta 3 cartas elegidas en diamantes.", 3, 1, 3 },
    { "Lima",              "Sube un número a hasta 2 cartas elegidas (la K pasa a As).", 3, 1, 2 },
    { "Buril de plata",    "Marca de Plata en hasta 2 cartas elegidas: +30 fichas.", 3, 1, 2 },
    { "Buril de rubí",     "Marca de Rubí en hasta 2 cartas elegidas: +5 Multi.", 3, 1, 2 },
    { "Buril de cristal",  "Marca de Cristal en 1 carta elegida: x1,5 Multi.", 4, 1, 1 },
    { "Buril de oro",      "Marca de Oro en 1 carta elegida: da $2 al puntuar.", 4, 1, 1 },
    { "Muesca de eco",     "Muesca en 1 carta elegida: puntúa dos veces.", 4, 1, 1 },
    { "Muesca de ancla",   "Muesca en hasta 2 cartas: +4 Multi si se quedan en la mano.", 3, 1, 2 },
    { "Muesca de trueque", "Muesca en hasta 2 cartas: dan $1 al descartarlas.", 3, 1, 2 },
    { "Cizalla",           "Destruye hasta 2 cartas elegidas. Salen del mazo para siempre.", 3, 1, 2 },
    { "Gemelo",            "Copia 1 carta elegida. La copia se queda en tu mazo.", 4, 1, 1 },
    { "Soplo",             "+1 descarte en esta ronda.", 2, 0, 0 },
    { "Prórroga",          "+1 jugada en esta ronda.", 3, 0, 0 },
    { "Sobre lacrado",     "Ganas $8 al abrirlo.", 4, 0, 0 },
    { "Chuleta",           "Sube de nivel tu mano más jugada.", 4, 0, 0 },
};

const DeckInfo DECK_INFO[DECK_COUNT] = {
    { "Clásica", "Las 52 cartas de siempre.", "" },
    { "Corta",   "Sin doses, treses ni cuatros: 40 cartas.", "Llega a la mesa 2." },
    { "Real",    "Con una copia extra de cada J, Q y K: 64 cartas.", "Llega a la mesa 3." },
    { "Marcada", "8 cartas empiezan con una marca al azar, pero sales con $0.", "Descubre 12 amuletos." },
    { "Del avaro", "Sales con $15, pero solo tienes 3 jugadas por ronda.", "Llega a la mesa 5." },
};

const BossInfo BOSS_INFO[B_COUNT] = {
    { "",                 "", 0 },
    { "El Manco",         "Tienes 1 carta menos en la mano.", 0 },
    { "El Impaciente",    "Tienes 1 jugada menos.", 0 },
    { "El Tacaño",        "No hay descartes en esta ronda.", 0 },
    { "El Supersticioso", "Un palo entero no puntúa.", 0 },
    { "El Republicano",   "Las figuras (J, Q, K) no puntúan.", 0 },
    { "La Banca",         "El objetivo es un 50% más alto.", 0 },
    { "El Estirado",      "Solo valen jugadas de 5 cartas.", 0 },
    { "El Recaudador",    "Pierdes $1 por cada carta descartada.", 0 },
    { "El Tuerto",        "La carta de más a la izquierda de cada jugada no puntúa.", 0 },
    { "El Maniático",     "Las cartas pares (2, 4, 6, 8, 10) no puntúan.", 0 },
    { "El Olvidadizo",    "Todas las manos cuentan como nivel 1.", 1 },
    { "El Aburrido",      "Repetir un tipo de mano en la ronda puntúa la mitad.", 0 },
    { "El Peajero",       "Cada jugada te cuesta $2.", 0 },
    { "El Ladrón",        "Tu amuleto de más a la izquierda no actúa.", 1 },
    { "El Purista",       "Las marcas y las muescas no actúan.", 1 },
    { "El Madrugón",      "La primera jugada de la ronda no puntúa.", 2 },
    { "El Daltónico",     "Medio mazo no puntúa.", 3 },
};

const DiffInfo DIFF_INFO[DIFF_COUNT] = {
    { "Aficionado", "Las reglas normales." },
    { "Tahúr",      "Objetivos un 30% más altos." },
    { "Leyenda",    "Objetivos un 70% más altos, 1 descarte menos y la tienda cobra $1 más." },
};

const char *const MARK_NAME[MARK_COUNT] = { "", "Plata", "Rubí", "Cristal", "Oro" };
const char *const MARK_DESC[MARK_COUNT] = {
    "",
    "+30 fichas al puntuar.",
    "+5 Multi al puntuar.",
    "x1,5 Multi al puntuar.",
    "Da $2 al puntuar.",
};
const char *const NOTCH_NAME[NOTCH_COUNT] = { "", "Eco", "Ancla", "Trueque" };
const char *const NOTCH_DESC[NOTCH_COUNT] = {
    "",
    "puntúa dos veces.",
    "+4 Multi si se queda en la mano.",
    "da $1 al descartarla.",
};
const char *const RARITY_NAME[RAR_COUNT] = { "Común", "Raro", "Épico" };
const char *const SUIT_NAME[4] = { "picas", "corazones", "tréboles", "diamantes" };

static const int MESA_BASE[NUM_MESAS] = { 240, 600, 1400, 3000, 6000, 11000, 19000, 32000 };
static const int ROUND_MULT[3]  = { 10, 16, 24 };   /* decimas */
static const int ROUND_PRIZE[3] = { 3, 4, 6 };
static const int DIFF_MULT[DIFF_COUNT] = { 10, 13, 17 };   /* decimas */
static const int RARITY_WEIGHT[RAR_COUNT] = { 6, 3, 1 };

/* -------------------------------------------------------------------- rng */

static unsigned int rng_state = 2463534242u;

void rng_seed(unsigned int s)
{
    rng_state = s ? s : 0x9E3779B9u;
}

unsigned int rng_get(void) { return rng_state; }
void rng_set(unsigned int s) { rng_seed(s); }

static unsigned int rng_next(void)
{
    unsigned int x = rng_state;
    x ^= x << 13;
    x ^= x >> 17;
    x ^= x << 5;
    rng_state = x;
    return x;
}

int rng_range(int n)
{
    if (n <= 1) return 0;
    return (int)((rng_next() >> 8) % (unsigned int)n);
}

/* ---------------------------------------------------------------- partida */

static void deck_add(Game *g, int rank, int suit)
{
    Card *c;
    if (g->deckCount >= MAX_DECK) return;
    c = &g->deck[g->deckCount];
    c->rank = (unsigned char)rank;
    c->suit = (unsigned char)suit;
    c->mark = MARK_NONE;
    c->notch = NOTCH_NONE;
    c->id = (unsigned char)g->deckCount;
    g->deckCount++;
}

void game_new_run(Game *g, unsigned int seed, int deckType, int difficulty)
{
    int s, r, t, i;
    memset(g, 0, sizeof(*g));
    rng_seed(seed);
    if (deckType < 0 || deckType >= DECK_COUNT) deckType = DECK_CLASICA;
    if (difficulty < 0 || difficulty >= DIFF_COUNT) difficulty = DIFF_NORMAL;
    g->deckType = deckType;
    g->difficulty = difficulty;
    g->money = 4;
    g->baseHands = 4;

    for (s = 0; s < 4; s++)
        for (r = (deckType == DECK_CORTA ? 5 : 2); r <= 14; r++) deck_add(g, r, s);

    switch (deckType) {
    case DECK_REAL:
        for (s = 0; s < 4; s++)
            for (r = 11; r <= 13; r++) deck_add(g, r, s);
        break;
    case DECK_MARCADA:
        for (i = 0; i < 8; i++) {
            int k = rng_range(g->deckCount), tries = 0;
            while (g->deck[k].mark && tries++ < 60) k = rng_range(g->deckCount);
            g->deck[k].mark = (unsigned char)(1 + rng_range(MARK_COUNT - 1));
        }
        g->money = 0;
        break;
    case DECK_AVARO:
        g->money = 15;
        g->baseHands = 3;
        break;
    default:
        break;
    }

    for (t = 0; t < HT_COUNT; t++) g->handLevel[t] = 1;
    g->lastBoss = B_NONE;
}

int game_has_relic(const Game *g, int id)
{
    int i;
    for (i = 0; i < g->relicCount; i++)
        if (g->relics[i] == id) return 1;
    return 0;
}

/* Suma de un efecto pasivo entre todos los amuletos */
static int passive(const Game *g, int eff)
{
    int s, k, total = 0;
    for (s = 0; s < g->relicCount; s++) {
        const Rule *r = RELIC_INFO[g->relics[s]].rules;
        for (k = 0; k < MAX_RULES; k++)
            if (r[k].when == W_PASSIVE && r[k].eff == eff) total += r[k].val;
    }
    return total;
}

int game_hand_size(const Game *g)
{
    int n = 8 + passive(g, E_HANDSIZE);
    if (g->bossRule == B_SHORT) n--;
    if (n > MAX_HAND - 2) n = MAX_HAND - 2;   /* deja sitio a las copias del truco Gemelo */
    return n;
}

int game_pockets(const Game *g)
{
    int n = 2 + passive(g, E_POCKETS);
    return n > MAX_TRICKS ? MAX_TRICKS : n;
}

int game_deck_alive(const Game *g)
{
    int i, n = 0;
    for (i = 0; i < g->deckCount; i++)
        if (g->deck[i].rank) n++;
    return n;
}

int game_round_prize(const Game *g)
{
    return ROUND_PRIZE[g->round];
}

/* Objetivo base de una mesa. Pasada la octava (modo infinito) crece un 60% por mesa. */
static long long mesa_base(int mesa)
{
    long long t;
    int i;
    if (mesa < NUM_MESAS) return MESA_BASE[mesa < 0 ? 0 : mesa];
    t = MESA_BASE[NUM_MESAS - 1];
    for (i = NUM_MESAS; i <= mesa && t < 4000000000000000LL; i++) t = t * 16 / 10;
    return t;
}

void game_setup_round(Game *g)
{
    long long t = mesa_base(g->mesa);
    g->bossRule = B_NONE;
    g->bossSuit = 0;
    if (g->round == 2) {
        /* se sortea entre los rivales que ya pueden salir en esta mesa, sin repetir el anterior */
        int pool[B_COUNT], n = 0, r;
        for (r = 1; r < B_COUNT; r++)
            if (r != g->lastBoss && BOSS_INFO[r].minMesa <= g->mesa) pool[n++] = r;
        r = pool[rng_range(n)];
        g->bossRule = r;
        g->lastBoss = r;
        if (r == B_SUIT) g->bossSuit = rng_range(4);
        if (r == B_COLOR) g->bossSuit = rng_range(2);     /* 0 = negras, 1 = rojas */
    }
    t = t * ROUND_MULT[g->round] / 10;
    t = t * DIFF_MULT[g->difficulty] / 10;
    if (g->bossRule == B_WALL) t = t * 3 / 2;
    g->target = t;
    g->score = 0;
}

static int card_before(Card a, Card b, int mode)
{
    if (mode == 0) {
        if (a.rank != b.rank) return a.rank > b.rank;
        return a.suit < b.suit;
    }
    if (a.suit != b.suit) return a.suit < b.suit;
    return a.rank > b.rank;
}

void game_sort_hand(Game *g)
{
    int i, j;
    for (i = 1; i < g->handCount; i++) {
        Card c = g->hand[i];
        unsigned char s = g->sel[i];
        j = i - 1;
        while (j >= 0 && card_before(c, g->hand[j], g->sortMode)) {
            g->hand[j + 1] = g->hand[j];
            g->sel[j + 1] = g->sel[j];
            j--;
        }
        g->hand[j + 1] = c;
        g->sel[j + 1] = s;
    }
}

static void deal(Game *g)
{
    int hs = game_hand_size(g);
    while (g->handCount < hs && g->drawCount > 0) {
        g->hand[g->handCount] = g->draw[--g->drawCount];
        g->sel[g->handCount] = 0;
        g->handCount++;
    }
    game_sort_hand(g);
}

void game_begin_round(Game *g)
{
    int i, n = 0;

    /* las cartas destruidas desaparecen del mazo y se renumeran las demas */
    for (i = 0; i < g->deckCount; i++)
        if (g->deck[i].rank) g->deck[n++] = g->deck[i];
    g->deckCount = n;

    for (i = 0; i < g->deckCount; i++) {
        g->deck[i].id = (unsigned char)i;
        g->draw[i] = g->deck[i];
    }
    g->drawCount = g->deckCount;
    for (i = g->drawCount - 1; i > 0; i--) {
        int j = rng_range(i + 1);
        Card t = g->draw[i];
        g->draw[i] = g->draw[j];
        g->draw[j] = t;
    }
    g->handCount = 0;
    g->playedCount = 0;
    g->score = 0;
    g->handsPlayed = 0;
    g->discardsUsed = 0;
    g->typesPlayed = 0;
    memset(g->sel, 0, sizeof(g->sel));
    g->handsLeft = g->baseHands + passive(g, E_HANDS) - (g->bossRule == B_FEWER ? 1 : 0);
    if (g->handsLeft < 1) g->handsLeft = 1;
    g->discardsLeft = 3 + passive(g, E_DISCARDS) - (g->difficulty == DIFF_EXPERT ? 1 : 0);
    if (g->discardsLeft < 0 || g->bossRule == B_NODISC) g->discardsLeft = 0;
    deal(g);
}

/* ----------------------------------------------------------------- cartas */

int game_card_chips(Card c)
{
    if (c.rank == 14) return 11;
    if (c.rank >= 11) return 10;
    return c.rank;
}

int game_card_debuffed(const Game *g, Card c)
{
    if (g->bossRule == B_SUIT && c.suit == g->bossSuit) return 1;
    if (g->bossRule == B_FACES && c.rank >= 11 && c.rank <= 13) return 1;
    if (g->bossRule == B_EVEN && c.rank <= 10 && (c.rank & 1) == 0) return 1;
    if (g->bossRule == B_COLOR && (c.suit & 1) == g->bossSuit) return 1;
    return 0;
}

void game_hand_base(const Game *g, int type, int *chips, int *mult)
{
    int lv = g->bossRule == B_FORGET ? 0 : g->handLevel[type] - 1;
    *chips = HAND_INFO[type].chips + lv * HAND_INFO[type].upChips;
    *mult  = HAND_INFO[type].mult  + lv * HAND_INFO[type].upMult;
}

void game_eval(const Card *c, int n, HandEval *e)
{
    int cnt[15];
    int i, r, max1 = 0, max2 = 0, r1 = 0, r2 = 0;
    int flush = 0, straight = 0, all = 0;

    memset(e, 0, sizeof(*e));
    memset(cnt, 0, sizeof(cnt));
    e->type = HT_HIGH;
    if (n <= 0) return;
    if (n > MAX_PLAY) n = MAX_PLAY;

    for (i = 0; i < n; i++) cnt[c[i].rank]++;
    for (r = 14; r >= 2; r--) {
        if (cnt[r] > max1) {
            max2 = max1; r2 = r1;
            max1 = cnt[r]; r1 = r;
        } else if (cnt[r] > max2) {
            max2 = cnt[r]; r2 = r;
        }
    }

    if (n == 5) {
        int lo = 15, hi = 0;
        flush = 1;
        for (i = 1; i < 5; i++)
            if (c[i].suit != c[0].suit) flush = 0;
        if (max1 == 1) {
            for (i = 0; i < 5; i++) {
                if (c[i].rank < lo) lo = c[i].rank;
                if (c[i].rank > hi) hi = c[i].rank;
            }
            if (hi - lo == 4) straight = 1;
            else if (cnt[14] && cnt[2] && cnt[3] && cnt[4] && cnt[5]) straight = 1;
        }
    }

    if (max1 == 5)                         { e->type = HT_FIVE;     all = 1; }
    else if (straight && flush)            { e->type = HT_STRFLUSH; all = 1; }
    else if (max1 == 4)                    { e->type = HT_QUADS; }
    else if (max1 == 3 && max2 == 2)       { e->type = HT_FULL;     all = 1; }
    else if (flush)                        { e->type = HT_FLUSH;    all = 1; }
    else if (straight)                     { e->type = HT_STRAIGHT; all = 1; }
    else if (max1 == 3)                    { e->type = HT_TRIPS; }
    else if (max1 == 2 && max2 == 2)       { e->type = HT_TWOPAIR; }
    else if (max1 == 2)                    { e->type = HT_PAIR; }
    else                                   { e->type = HT_HIGH; }

    if (all) {
        for (i = 0; i < n; i++) e->scores[i] = 1;
    } else if (e->type == HT_HIGH) {
        for (i = 0; i < n; i++) {
            if (c[i].rank == r1) { e->scores[i] = 1; break; }
        }
    } else {
        for (i = 0; i < n; i++) {
            if (c[i].rank == r1) e->scores[i] = 1;
            if (e->type == HT_TWOPAIR && c[i].rank == r2) e->scores[i] = 1;
        }
    }

    e->hasPair = (max1 >= 2);
    e->hasTrips = (max1 >= 3);
    e->hasQuads = (max1 >= 4);
    e->hasStraight = straight;
    e->hasFlush = flush;
}

/* ------------------------------------------------------------- puntuacion */

/* Todo lo que una regla puede necesitar saber de la jugada en curso */
typedef struct {
    const Game *g;
    const HandEval *e;
    int n;                  /* cartas jugadas */
    Card card;              /* carta en curso (reglas por carta) */
    int cardIdx;            /* su indice en la jugada, o -1 */
    int first, last;        /* primera y ultima carta que puntuan */
    int oneColor, fourSuit, kingQueen, noFace;
    int diamondDone;
} Ctx;

static void ev_push(ScoreResult *r, int kind, int card, int relic, int held, float value)
{
    switch (kind) {
    case EV_CHIPS: r->chips += value; break;
    case EV_MULT:  r->mult += value; break;
    case EV_XMULT: r->mult *= value; break;
    case EV_MONEY: r->money += (int)value; break;
    default: break;
    }
    if (r->evCount < MAX_EVENTS) {
        ScoreEvent *e = &r->ev[r->evCount++];
        e->kind = (unsigned char)kind;
        e->card = (signed char)card;
        e->relic = (signed char)relic;
        e->held = (signed char)held;
        e->value = value;
        e->chips = r->chips;
        e->mult = r->mult;
    }
}

static int cond_ok(const Rule *r, Ctx *x)
{
    const Game *g = x->g;
    const HandEval *e = x->e;
    Card c = x->card;

    switch (r->cond) {
    case C_ALWAYS:        return 1;
    case C_SUIT:          return c.suit == r->arg;
    case C_FACE:          return c.rank >= 11 && c.rank <= 13;
    case C_ACE:           return c.rank == 14;
    case C_PRIME:         return c.rank == 2 || c.rank == 3 || c.rank == 5 || c.rank == 7;
    case C_LOW:           return c.rank >= 2 && c.rank <= 6;
    case C_HIGH:          return c.rank >= 10;
    case C_RANK:          return c.rank == r->arg;
    case C_MARKED:        return c.mark != MARK_NONE;
    case C_FIRST_DIAMOND: return c.suit == SUIT_DIAMONDS && !x->diamondDone;
    case C_HAS_PAIR:      return e->hasPair;
    case C_HAS_TRIPS:     return e->hasTrips;
    case C_HAS_QUADS:     return e->hasQuads;
    case C_STRAIGHT:      return e->hasStraight;
    case C_FLUSH:         return e->hasFlush;
    case C_TYPE:          return e->type == r->arg;
    case C_MAX3:          return x->n <= 3;
    case C_FIVE_CARDS:    return x->n == 5;
    case C_LAST_HAND:     return g->handsLeft == 1;
    case C_FIRST_HAND:    return g->handsPlayed == 0;
    case C_NO_DISCARD:    return g->discardsUsed == 0;
    case C_ONE_COLOR:     return x->oneColor;
    case C_FOUR_SUIT:     return x->fourSuit;
    case C_KING_QUEEN:    return x->kingQueen;
    case C_NO_FACE:       return x->noFace;
    case C_CHANCE:        return rng_range(r->arg) == 0;
    case C_LAST_SCORING:  return x->cardIdx >= 0 && x->cardIdx == x->last;
    case C_FIRST_SCORING: return x->cardIdx >= 0 && x->cardIdx == x->first;
    default:              return 0;
    }
}

/* Aplica el efecto de una regla que ya ha pasado su condicion */
static void apply_rule(ScoreResult *res, const Rule *r, Ctx *x, int slot, int card, int held)
{
    const Game *g = x->g;
    int v = r->val, n;

    switch (r->eff) {
    case E_CHIPS:
        ev_push(res, EV_CHIPS, card, slot, held, (float)v);
        break;
    case E_MULT:
        ev_push(res, EV_MULT, card, slot, held, (float)v);
        break;
    case E_XMULT:
        ev_push(res, EV_XMULT, card, slot, held, (float)v / 10.0f);
        break;
    case E_MONEY:
        if (r->cond == C_FIRST_DIAMOND) x->diamondDone = 1;
        ev_push(res, EV_MONEY, card, slot, held, (float)v);
        break;
    case E_CARD_VALUE:
        ev_push(res, EV_CHIPS, card, slot, held, (float)game_card_chips(x->card));
        break;
    case E_MULT_PER_MONEY:
        n = v > 0 ? g->money / v : 0;
        if (n > 0) ev_push(res, EV_MULT, card, slot, held, (float)n);
        break;
    case E_MULT_PER_DISCARD:
        if (g->discardsLeft > 0) ev_push(res, EV_MULT, card, slot, held, (float)(g->discardsLeft * v));
        break;
    case E_MULT_PER_HAND:
        if (g->handsLeft > 1) ev_push(res, EV_MULT, card, slot, held, (float)((g->handsLeft - 1) * v));
        break;
    case E_MULT_PER_RELIC:
        ev_push(res, EV_MULT, card, slot, held, (float)(g->relicCount * v));
        break;
    case E_MULT_COUNTER:
        if (g->relicCounter[slot] > 0) ev_push(res, EV_MULT, card, slot, held, (float)(g->relicCounter[slot] * v));
        break;
    case E_MULT_DICE:
        ev_push(res, EV_MULT, card, slot, held, (float)(v * (1 + rng_range(6))));
        break;
    case E_MULT_THIN:
        n = 52 - game_deck_alive(g);
        if (n > 0) ev_push(res, EV_MULT, card, slot, held, (float)(n * v));
        break;
    default:
        break;   /* contadores, pasivas y repeticiones no puntuan aqui */
    }
}

/* Recorre los amuletos de izquierda a derecha y dispara las reglas de un momento dado */
static void run_rules(ScoreResult *res, Ctx *x, int when, int card, int held)
{
    const Game *g = x->g;
    int s, k;
    for (s = (g->bossRule == B_THIEF ? 1 : 0); s < g->relicCount; s++) {
        const Rule *r = RELIC_INFO[g->relics[s]].rules;
        for (k = 0; k < MAX_RULES; k++) {
            if (r[k].when != when) continue;
            if (cond_ok(&r[k], x)) apply_rule(res, &r[k], x, s, card, held);
        }
    }
}

/* Cuantas veces de mas se cuenta la carta en curso */
static int extra_repeats(Ctx *x)
{
    const Game *g = x->g;
    int s, k, n = 0;
    if (x->card.notch == NOTCH_ECO && g->bossRule != B_PURE) n++;
    for (s = (g->bossRule == B_THIEF ? 1 : 0); s < g->relicCount; s++) {
        const Rule *r = RELIC_INFO[g->relics[s]].rules;
        for (k = 0; k < MAX_RULES; k++)
            if (r[k].when == W_RETRIG && cond_ok(&r[k], x)) n += r[k].val;
    }
    return n;
}

static void score_card(ScoreResult *res, Ctx *x, int i)
{
    Card c = x->card;
    if (game_card_debuffed(x->g, c) || (x->g->bossRule == B_LEFT && i == 0)) {
        ev_push(res, EV_NULL, i, -1, -1, 0);
        return;
    }
    ev_push(res, EV_CHIPS, i, -1, -1, (float)game_card_chips(c));
    switch (x->g->bossRule == B_PURE ? MARK_NONE : c.mark) {
    case MARK_SILVER:  ev_push(res, EV_CHIPS, i, -1, -1, 30); break;
    case MARK_RUBY:    ev_push(res, EV_MULT,  i, -1, -1, 5); break;
    case MARK_CRYSTAL: ev_push(res, EV_XMULT, i, -1, -1, 1.5f); break;
    case MARK_GOLD:    ev_push(res, EV_MONEY, i, -1, -1, 2); break;
    default: break;
    }
    run_rules(res, x, W_CARD, i, -1);
}

/* Orden estricto: cartas que puntuan (de izquierda a derecha), cartas jugadas que no
   puntuan, cartas que se quedan en la mano y, por ultimo, amuletos de izquierda a derecha. */
static void game_score(const Game *g, const Card *c, int n, ScoreResult *res)
{
    Ctx x;
    int i, h, bc, bm, rep, suitCount[4];
    int king = 0, queen = 0;
    const HandEval *e;

    memset(res, 0, sizeof(*res));
    game_eval(c, n, &res->eval);
    e = &res->eval;
    res->level = g->handLevel[e->type];
    game_hand_base(g, e->type, &bc, &bm);
    res->baseChips = res->chips = bc;
    res->baseMult = res->mult = bm;

    memset(&x, 0, sizeof(x));
    memset(suitCount, 0, sizeof(suitCount));
    x.g = g;
    x.e = e;
    x.n = n;
    x.cardIdx = -1;
    x.first = x.last = -1;
    x.oneColor = 1;
    x.noFace = 1;
    for (i = 0; i < n; i++) {
        if (e->scores[i]) {
            if (x.first < 0) x.first = i;
            x.last = i;
            if (!game_card_debuffed(g, c[i]) && !(g->bossRule == B_LEFT && i == 0)) {
                if (c[i].rank == 13) king = 1;
                if (c[i].rank == 12) queen = 1;
            }
        }
        if ((c[i].suit & 1) != (c[0].suit & 1)) x.oneColor = 0;
        if (c[i].rank >= 11 && c[i].rank <= 13) x.noFace = 0;
        if (++suitCount[c[i].suit & 3] >= 4) x.fourSuit = 1;
    }
    x.kingQueen = king && queen;

    for (i = 0; i < n; i++) {
        if (!e->scores[i]) continue;
        x.card = c[i];
        x.cardIdx = i;
        rep = 1 + extra_repeats(&x);
        while (rep-- > 0) score_card(res, &x, i);
    }

    for (i = 0; i < n; i++) {
        if (e->scores[i] || game_card_debuffed(g, c[i])) continue;
        x.card = c[i];
        x.cardIdx = i;
        run_rules(res, &x, W_KICKER, i, -1);
    }

    x.cardIdx = -1;
    for (h = 0; h < g->handCount; h++) {
        if (game_card_debuffed(g, g->hand[h])) continue;
        x.card = g->hand[h];
        if (x.card.notch == NOTCH_ANCLA && g->bossRule != B_PURE) ev_push(res, EV_MULT, -1, -1, h, 4);
        run_rules(res, &x, W_HELD, -1, h);
    }

    memset(&x.card, 0, sizeof(x.card));
    run_rules(res, &x, W_END, -1, -1);

    /* castigos de rival que afectan al total de la jugada */
    if (g->bossRule == B_REPEAT && ((g->typesPlayed >> e->type) & 1)) ev_push(res, EV_XMULT, -1, -1, -1, 0.5f);
    if (g->bossRule == B_EARLY && g->handsPlayed == 0) ev_push(res, EV_XMULT, -1, -1, -1, 0.0f);

    res->total = (long long)(res->chips * res->mult);
    if (res->total < 0) res->total = 0;
}

/* Suma `val` al contador de cada amuleto que tenga una regla E_COUNTER_ADD en ese momento */
static void bump_counters(Game *g, int when)
{
    int s, k;
    for (s = 0; s < g->relicCount; s++) {
        const Rule *r = RELIC_INFO[g->relics[s]].rules;
        for (k = 0; k < MAX_RULES; k++)
            if (r[k].when == when && r[k].eff == E_COUNTER_ADD) g->relicCounter[s] += r[k].val;
    }
}

/* ------------------------------------------------------- jugar/descartar */

int game_selected_count(const Game *g)
{
    int i, n = 0;
    for (i = 0; i < g->handCount; i++)
        if (g->sel[i]) n++;
    return n;
}

int game_selected_cards(const Game *g, Card *out)
{
    int i, n = 0;
    for (i = 0; i < g->handCount && n < MAX_PLAY; i++)
        if (g->sel[i]) out[n++] = g->hand[i];
    return n;
}

void game_toggle_select(Game *g, int i)
{
    if (i < 0 || i >= g->handCount) return;
    if (g->sel[i]) g->sel[i] = 0;
    else if (game_selected_count(g) < MAX_PLAY) g->sel[i] = 1;
}

void game_clear_selection(Game *g)
{
    memset(g->sel, 0, sizeof(g->sel));
}

static int remove_selected(Game *g, Card *out)
{
    int i, n = 0, k = 0;
    for (i = 0; i < g->handCount; i++) {
        if (g->sel[i] && n < MAX_PLAY) {
            out[n++] = g->hand[i];
        } else {
            g->hand[k] = g->hand[i];
            g->sel[k] = 0;
            k++;
        }
    }
    g->handCount = k;
    memset(g->sel, 0, sizeof(g->sel));
    return n;
}

int game_play_block(const Game *g)
{
    int n = game_selected_count(g);
    if (n < 1 || g->handsLeft <= 0) return PLAY_NO_CARDS;
    if (g->bossRule == B_FIVE && n != 5) return PLAY_NEED_FIVE;
    return PLAY_OK;
}

void game_play(Game *g, ScoreResult *res)
{
    g->playedCount = remove_selected(g, g->played);
    game_score(g, g->played, g->playedCount, res);
}

void game_finish_play(Game *g, const ScoreResult *res)
{
    g->score += res->total;
    if (res->total > g->bestHand) g->bestHand = res->total;
    g->money += res->money;
    g->handsLeft--;
    g->handsPlayed++;
    g->typesPlayed |= 1 << res->eval.type;
    g->handPlays[res->eval.type]++;
    if (g->bossRule == B_TOLL) {
        g->money -= 2;
        if (g->money < 0) g->money = 0;
    }
    g->playedCount = 0;
    bump_counters(g, W_END);
    deal(g);
}

int game_discard(Game *g)
{
    Card tmp[MAX_PLAY];
    int i, n = game_selected_count(g);
    if (n < 1 || g->discardsLeft <= 0) return 0;
    n = remove_selected(g, tmp);
    g->discardsLeft--;
    g->discardsUsed++;
    for (i = 0; i < n; i++)
        if (tmp[i].notch == NOTCH_TRUEQUE && g->bossRule != B_PURE) g->money++;
    if (g->bossRule == B_TAX) {
        g->money -= n;
        if (g->money < 0) g->money = 0;
    }
    bump_counters(g, W_DISCARD);
    deal(g);
    return 1;
}

int game_round_won(const Game *g)
{
    return g->score >= g->target;
}

int game_round_lost(const Game *g)
{
    if (game_round_won(g)) return 0;
    if (g->bossRule == B_FIVE && g->handCount < 5) return 1;   /* ya no se puede jugar */
    return g->handsLeft <= 0 || g->handCount == 0;
}

/* ----------------------------------------------------------------- trucos */

/* Los cambios hechos a una carta de la mano se copian tambien a su ficha del mazo */
static void write_back(Game *g, Card c)
{
    if (c.id < g->deckCount) g->deck[c.id] = c;
}

int game_use_trick(Game *g, int slot)
{
    const TrickInfo *info;
    int id, i, n, used = 0;

    if (slot < 0 || slot >= g->trickCount) return TRICK_NO_EFFECT;
    id = g->tricks[slot];
    info = &TRICK_INFO[id];
    n = game_selected_count(g);
    if (n < info->minSel || n > info->maxSel) return TRICK_NEED_CARDS;

    switch (id) {
    case T_SOPLO:
        g->discardsLeft++;
        used = 1;
        break;
    case T_INTENTO:
        g->handsLeft++;
        used = 1;
        break;
    case T_PROPINA:
        g->money += 8;
        used = 1;
        break;
    case T_LECCION: {
        int best = HT_HIGH;
        for (i = 1; i < HT_COUNT; i++)
            if (g->handPlays[i] > g->handPlays[best]) best = i;
        g->handLevel[best]++;
        used = 1;
        break;
    }
    case T_CIZALLA: {
        int k = 0;
        for (i = 0; i < g->handCount; i++) {
            if (g->sel[i]) {
                if (g->hand[i].id < g->deckCount) g->deck[g->hand[i].id].rank = 0;
            } else {
                g->hand[k++] = g->hand[i];
            }
        }
        g->handCount = k;
        used = 1;
        break;
    }
    case T_GEMELO:
        if (g->deckCount >= MAX_DECK || g->handCount >= MAX_HAND) return TRICK_NO_EFFECT;
        for (i = 0; i < g->handCount; i++) {
            if (g->sel[i]) {
                Card copy = g->hand[i];
                copy.id = (unsigned char)g->deckCount;
                g->deck[g->deckCount++] = copy;
                g->hand[g->handCount++] = copy;
                used = 1;
                break;
            }
        }
        break;
    default:
        for (i = 0; i < g->handCount; i++) {
            Card *c = &g->hand[i];
            if (!g->sel[i]) continue;
            switch (id) {
            case T_TINTE_S: c->suit = SUIT_SPADES; break;
            case T_TINTE_H: c->suit = SUIT_HEARTS; break;
            case T_TINTE_C: c->suit = SUIT_CLUBS; break;
            case T_TINTE_D: c->suit = SUIT_DIAMONDS; break;
            case T_LIMA:    if (c->rank < 14) c->rank++; break;
            case T_BURIL_PLATA:   c->mark = MARK_SILVER; break;
            case T_BURIL_RUBI:    c->mark = MARK_RUBY; break;
            case T_BURIL_CRISTAL: c->mark = MARK_CRYSTAL; break;
            case T_BURIL_ORO:     c->mark = MARK_GOLD; break;
            case T_MUESCA_ECO:     c->notch = NOTCH_ECO; break;
            case T_MUESCA_ANCLA:   c->notch = NOTCH_ANCLA; break;
            case T_MUESCA_TRUEQUE: c->notch = NOTCH_TRUEQUE; break;
            default: break;
            }
            write_back(g, *c);
            used = 1;
        }
        break;
    }
    if (!used) return TRICK_NO_EFFECT;

    for (i = slot; i < g->trickCount - 1; i++) g->tricks[i] = g->tricks[i + 1];
    g->trickCount--;
    game_clear_selection(g);
    if (id == T_CIZALLA) deal(g);
    else game_sort_hand(g);
    return TRICK_OK;
}

int game_trick_shop_usable(int id)
{
    return id == T_PROPINA || id == T_LECCION;
}

int game_trick_sell_value(int id)
{
    int v = TRICK_INFO[id].price / 2;
    return v < 1 ? 1 : v;
}

void game_sell_trick(Game *g, int slot)
{
    int i;
    if (slot < 0 || slot >= g->trickCount) return;
    g->money += game_trick_sell_value(g->tricks[slot]);
    for (i = slot; i < g->trickCount - 1; i++) g->tricks[i] = g->tricks[i + 1];
    g->trickCount--;
}

/* ------------------------------------------------------------- progresion */

void game_collect_reward(Game *g, Reward *r)
{
    int s, k;
    r->base = ROUND_PRIZE[g->round];
    r->hands = g->handsLeft > 0 ? g->handsLeft : 0;
    r->interest = g->money / 5;
    if (r->interest > 5) r->interest = 5;
    r->relic = 0;
    for (s = 0; s < g->relicCount; s++) {
        const Rule *rule = RELIC_INFO[g->relics[s]].rules;
        for (k = 0; k < MAX_RULES; k++) {
            if (rule[k].when != W_WIN) continue;
            switch (rule[k].eff) {
            case E_MONEY:             r->relic += rule[k].val; break;
            case E_MONEY_PER_DISCARD: r->relic += g->discardsLeft * rule[k].val; break;
            case E_MONEY_PER_10:      r->relic += g->money / 10 * rule[k].val; break;
            default: break;
            }
        }
    }
    r->total = r->base + r->hands + r->interest + r->relic;
    g->money += r->total;
    bump_counters(g, W_WIN);
}

int game_next_round(Game *g)
{
    g->round++;
    if (g->round > 2) {
        g->round = 0;
        g->mesa++;
    }
    g->bossRule = B_NONE;
    /* devuelve 1 una sola vez: al superar la octava mesa. En modo infinito ya no se "gana". */
    return !g->endless && g->mesa >= NUM_MESAS;
}

/* ----------------------------------------------------------------- tienda */

static int shop_price(const Game *g, int base)
{
    int p = base - passive(g, E_DISCOUNT) + (g->difficulty == DIFF_EXPERT ? 1 : 0);
    return p < 1 ? 1 : p;
}

/* Saca un amuleto de la bolsa; los raros y epicos salen menos */
static int pick_relic(int *pool, int *n)
{
    int total = 0, i, r;
    for (i = 0; i < *n; i++) total += RARITY_WEIGHT[RELIC_INFO[pool[i]].rarity];
    r = rng_range(total);
    for (i = 0; i < *n; i++) {
        r -= RARITY_WEIGHT[RELIC_INFO[pool[i]].rarity];
        if (r < 0) break;
    }
    if (i >= *n) i = *n - 1;
    r = pool[i];
    pool[i] = pool[--(*n)];
    return r;
}

static void shop_roll(Game *g)
{
    int pool[R_COUNT];
    int n = 0, i, k;
    Offer *o;

    for (i = 0; i < R_COUNT; i++)
        if (!game_has_relic(g, i)) pool[n++] = i;

    for (k = 0; k < 2; k++) {
        o = &g->shop[k];
        memset(o, 0, sizeof(*o));
        o->kind = OF_RELIC;
        if (n > 0) {
            o->id = pick_relic(pool, &n);
            o->price = shop_price(g, RELIC_INFO[o->id].price);
        } else {
            o->sold = 1;
        }
    }

    o = &g->shop[2];
    memset(o, 0, sizeof(*o));
    o->kind = OF_LESSON;
    o->id = rng_range(g->handPlays[HT_FIVE] > 0 ? HT_COUNT : HT_FIVE);
    o->price = shop_price(g, 4);

    o = &g->shop[3];
    memset(o, 0, sizeof(*o));
    o->kind = OF_CARD;
    o->card.rank = (unsigned char)(2 + rng_range(13));
    o->card.suit = (unsigned char)rng_range(4);
    o->card.mark = (unsigned char)(1 + rng_range(MARK_COUNT - 1));
    if (rng_range(10) < 3) o->card.notch = (unsigned char)(1 + rng_range(NOTCH_COUNT - 1));
    o->price = shop_price(g, o->card.notch ? 4 : 3);

    o = &g->shop[4];
    memset(o, 0, sizeof(*o));
    o->kind = OF_TRICK;
    o->id = rng_range(T_COUNT);
    o->price = shop_price(g, TRICK_INFO[o->id].price);
}

void game_shop_enter(Game *g)
{
    g->rerollCost = 3;
    shop_roll(g);
}

int game_shop_reroll(Game *g)
{
    if (g->money < g->rerollCost) return 0;
    g->money -= g->rerollCost;
    g->rerollCost++;
    shop_roll(g);
    return 1;
}

int game_shop_buy(Game *g, int idx)
{
    Offer *o;
    if (idx < 0 || idx >= SHOP_OFFERS) return BUY_SOLD;
    o = &g->shop[idx];
    if (o->sold) return BUY_SOLD;
    if (g->money < o->price) return BUY_NO_MONEY;

    switch (o->kind) {
    case OF_RELIC:
        if (g->relicCount >= MAX_RELICS) return BUY_NO_SPACE;
        g->relics[g->relicCount] = o->id;
        g->relicCounter[g->relicCount] = 0;
        g->relicCount++;
        break;
    case OF_LESSON:
        g->handLevel[o->id]++;
        break;
    case OF_TRICK:
        if (g->trickCount >= game_pockets(g)) return BUY_NO_SPACE;
        g->tricks[g->trickCount++] = o->id;
        break;
    default:
        if (g->deckCount >= MAX_DECK) return BUY_NO_SPACE;
        g->deck[g->deckCount] = o->card;
        g->deck[g->deckCount].id = (unsigned char)g->deckCount;
        g->deckCount++;
        break;
    }
    g->money -= o->price;
    o->sold = 1;
    return BUY_OK;
}

int game_relic_sell_value(int id)
{
    int v = RELIC_INFO[id].price / 2;
    return v < 1 ? 1 : v;
}

void game_shop_sell(Game *g, int slot)
{
    int i;
    if (slot < 0 || slot >= g->relicCount) return;
    g->money += game_relic_sell_value(g->relics[slot]);
    for (i = slot; i < g->relicCount - 1; i++) {
        g->relics[i] = g->relics[i + 1];
        g->relicCounter[i] = g->relicCounter[i + 1];
    }
    g->relicCount--;
    /* si se vende el bolsillo extra, el truco que ya no cabe se pierde */
    if (g->trickCount > game_pockets(g)) g->trickCount = game_pockets(g);
}
