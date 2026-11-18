/* FAROL - logica del juego (independiente de la plataforma) */
#ifndef GAME_H
#define GAME_H

#define MAX_DECK    96
#define MAX_HAND    12
#define MAX_PLAY    5
#define MAX_RELICS  5
#define MAX_TRICKS  3
#define MAX_RULES   2
#define NUM_MESAS   8
#define SHOP_OFFERS 5
#define MAX_EVENTS  160

enum { SUIT_SPADES, SUIT_HEARTS, SUIT_CLUBS, SUIT_DIAMONDS };

/* Una carta puede llevar dos capas de modificadores: marca (al puntuar) y muesca (en otros momentos) */
enum { MARK_NONE, MARK_SILVER, MARK_RUBY, MARK_CRYSTAL, MARK_GOLD, MARK_COUNT };
enum { NOTCH_NONE, NOTCH_ECO, NOTCH_ANCLA, NOTCH_TRUEQUE, NOTCH_COUNT };

enum {
    HT_HIGH, HT_PAIR, HT_TWOPAIR, HT_TRIPS, HT_STRAIGHT, HT_FLUSH,
    HT_FULL, HT_QUADS, HT_STRFLUSH, HT_FIVE, HT_COUNT
};

enum {
    R_HERRADURA, R_TREBOL, R_RELICARIO, R_PUNAL, R_LUPA, R_GEMELOS, R_TRIPODE,
    R_PELDANO, R_TINTE, R_NAVAJA, R_ULTIMA, R_USURERO, R_CORONA, R_PRIMOS,
    R_RELOJ, R_NIEVE, R_VITRINA, R_ECO, R_PROPINA, R_GUANTE, R_MANGA,
    R_ALIENTO, R_BICOLOR, R_FAROL, R_DADO, R_AS,
    R_BANQUILLO, R_LASTRE, R_CALDERILLA, R_SOCIEDAD, R_IMPRESION, R_LIMPIA,
    R_DOBLETE, R_CASA, R_CUARTETO, R_MARCAS, R_HUCHA, R_TRAPERO, R_METRONOMO,
    R_FINO, R_PALO, R_RULETA, R_CAJA, R_SIETE, R_PAREJA_REAL, R_SIN_FIGURAS,
    R_LLENA, R_PRELUDIO, R_SUPLENTE, R_BOLSILLO, R_REGATEO, R_RACHA,
    R_SUIZA, R_CHATARRERO, R_PRESTI, R_ALQUIMISTA, R_CARAMBOLA, R_GRABADOR, R_ARCHIVERO, R_SOLITARIO,
    R_COUNT
};

enum { RAR_COMMON, RAR_RARE, RAR_EPIC, RAR_COUNT };

/* Trucos: objetos de un solo uso que se gastan durante la ronda */
enum {
    T_TINTE_S, T_TINTE_H, T_TINTE_C, T_TINTE_D, T_LIMA,
    T_BURIL_PLATA, T_BURIL_RUBI, T_BURIL_CRISTAL, T_BURIL_ORO,
    T_MUESCA_ECO, T_MUESCA_ANCLA, T_MUESCA_TRUEQUE,
    T_CIZALLA, T_GEMELO, T_SOPLO, T_INTENTO, T_PROPINA, T_LECCION,
    T_COUNT
};

enum { DECK_CLASICA, DECK_CORTA, DECK_REAL, DECK_MARCADA, DECK_AVARO, DECK_COUNT };

enum {
    B_NONE, B_SHORT, B_FEWER, B_NODISC, B_SUIT, B_FACES, B_WALL, B_FIVE, B_TAX,
    B_LEFT, B_EVEN, B_FORGET, B_REPEAT, B_TOLL, B_THIEF, B_PURE, B_EARLY, B_COLOR,
    B_COUNT
};

enum { DIFF_NORMAL, DIFF_HARD, DIFF_EXPERT, DIFF_COUNT };

enum { EV_CHIPS, EV_MULT, EV_XMULT, EV_MONEY, EV_NULL };
enum { OF_RELIC, OF_LESSON, OF_CARD, OF_TRICK };
enum { BUY_OK, BUY_NO_MONEY, BUY_NO_SPACE, BUY_SOLD };
enum { PLAY_OK, PLAY_NO_CARDS, PLAY_NEED_FIVE };
enum { TRICK_OK, TRICK_NEED_CARDS, TRICK_NO_EFFECT };

typedef struct { unsigned char rank, suit, mark, notch, id; } Card;   /* rank 2..14 (14 = As); 0 = destruida */

/* Regla de un amuleto: en que momento salta, con que condicion y que efecto tiene */
typedef struct { unsigned char when, cond, arg, eff; short val; } Rule;

/* Idiomas: todos los textos de las tablas van como { espanol, ingles } */
#define NUM_LANGS 2
enum { LANG_ES, LANG_EN };
extern int game_lang;
#define LANG game_lang

typedef struct { const char *name[NUM_LANGS]; int chips, mult, upChips, upMult; } HandInfo;
typedef struct {
    const char *name[NUM_LANGS];
    const char *desc[NUM_LANGS];
    unsigned char price, rarity;
    Rule rules[MAX_RULES];
} RelicInfo;
typedef struct { const char *name[NUM_LANGS]; const char *desc[NUM_LANGS]; unsigned char price, minSel, maxSel; } TrickInfo;
typedef struct { const char *name[NUM_LANGS]; const char *desc[NUM_LANGS]; const char *unlock[NUM_LANGS]; } DeckInfo;
typedef struct { const char *name[NUM_LANGS]; const char *desc[NUM_LANGS]; int minMesa; } BossInfo;   /* minMesa: desde que mesa sale */
typedef struct { const char *name[NUM_LANGS]; const char *desc[NUM_LANGS]; } DiffInfo;

extern const HandInfo  HAND_INFO[HT_COUNT];
extern const RelicInfo RELIC_INFO[R_COUNT];
extern const TrickInfo TRICK_INFO[T_COUNT];
extern const DeckInfo  DECK_INFO[DECK_COUNT];
extern const BossInfo  BOSS_INFO[B_COUNT];
extern const DiffInfo  DIFF_INFO[DIFF_COUNT];
extern const char *const MARK_NAME[NUM_LANGS][MARK_COUNT];
extern const char *const MARK_DESC[NUM_LANGS][MARK_COUNT];
extern const char *const NOTCH_NAME[NUM_LANGS][NOTCH_COUNT];
extern const char *const NOTCH_DESC[NUM_LANGS][NOTCH_COUNT];
extern const char *const RARITY_NAME[NUM_LANGS][RAR_COUNT];
extern const char *const SUIT_NAME[NUM_LANGS][4];

typedef struct {
    int type;
    unsigned char scores[MAX_PLAY];
    int hasPair, hasTrips, hasQuads, hasStraight, hasFlush;
} HandEval;

typedef struct {
    unsigned char kind;
    signed char card;      /* indice en la jugada, o -1 */
    signed char relic;     /* hueco de amuleto, o -1 */
    signed char held;      /* indice en la mano (carta que se quedo sin jugar), o -1 */
    float value;
    double chips, mult;    /* valores acumulados tras el evento */
} ScoreEvent;

typedef struct {
    HandEval eval;
    int level;
    double baseChips, baseMult;
    ScoreEvent ev[MAX_EVENTS];
    int evCount;
    double chips, mult;
    long long total;
    int money;
} ScoreResult;

typedef struct { int kind, id, price, sold; Card card; } Offer;
typedef struct { int base, hands, interest, relic, total; } Reward;

typedef struct {
    /* partida */
    Card deck[MAX_DECK];
    int deckCount;
    int relics[MAX_RELICS];
    int relicCounter[MAX_RELICS];
    int relicCount;
    int tricks[MAX_TRICKS];
    int trickCount;
    int handLevel[HT_COUNT];
    int handPlays[HT_COUNT];
    int money;
    int mesa, round;
    int lastBoss;
    int deckType, baseHands;
    unsigned int seed;          /* semilla con la que empezo la partida */
    /* estadisticas de la partida */
    int statHands, statDiscards, statRounds, statTricks, statEarned;
    int difficulty;
    int endless;                /* 1 cuando ya se han superado las 8 mesas y se sigue jugando */
    long long bestHand;
    /* ronda */
    Card draw[MAX_DECK];
    int drawCount;
    Card hand[MAX_HAND];
    unsigned char sel[MAX_HAND];
    int handCount;
    Card played[MAX_PLAY];
    int playedCount;
    int handsLeft, discardsLeft;
    int handsPlayed, discardsUsed;
    int typesPlayed;            /* bits: tipos de mano ya jugados en esta ronda */
    long long target, score;
    int bossRule, bossSuit;
    int sortMode;
    /* tienda */
    Offer shop[SHOP_OFFERS];
    int rerollCost;
} Game;

void rng_seed(unsigned int s);
int  rng_range(int n);
unsigned int rng_get(void);
void rng_set(unsigned int s);

void game_new_run(Game *g, unsigned int seed, int deckType, int difficulty);
void game_setup_round(Game *g);
void game_begin_round(Game *g);
int  game_has_relic(const Game *g, int id);
int  game_hand_size(const Game *g);
int  game_pockets(const Game *g);
int  game_deck_alive(const Game *g);
int  game_round_prize(const Game *g);
int  game_card_chips(Card c);
int  game_card_debuffed(const Game *g, Card c);
void game_hand_base(const Game *g, int type, int *chips, int *mult);
void game_eval(const Card *c, int n, HandEval *e);
void game_sort_hand(Game *g);
int  game_selected_count(const Game *g);
int  game_selected_cards(const Game *g, Card *out);
void game_toggle_select(Game *g, int i);
void game_clear_selection(Game *g);
int  game_play_block(const Game *g);
void game_play(Game *g, ScoreResult *res);
void game_finish_play(Game *g, const ScoreResult *res);
int  game_discard(Game *g);
int  game_use_trick(Game *g, int slot);
int  game_trick_shop_usable(int id);    /* trucos que tambien sirven fuera de una ronda */
int  game_trick_sell_value(int id);
void game_sell_trick(Game *g, int slot);
int  game_round_won(const Game *g);
int  game_round_lost(const Game *g);
void game_collect_reward(Game *g, Reward *r);
int  game_next_round(Game *g);
void game_shop_enter(Game *g);
int  game_shop_reroll(Game *g);
int  game_shop_buy(Game *g, int idx);
int  game_relic_sell_value(int id);
void game_shop_sell(Game *g, int slot);
int  game_move_relic(Game *g, int slot, int dir);   /* cambia el orden; devuelve el hueco nuevo */

#endif
