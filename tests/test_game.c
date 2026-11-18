/* Pruebas de la logica de FAROL. Se compilan para el PC, sin nada de PSP:
 *   gcc -O2 -Wall -Isrc tests/test_game.c src/game.c src/save.c -o test_game && ./test_game
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "game.h"
#include "save.h"

static int failures = 0;

#define CHECK(cond, ...) do { if (!(cond)) { failures++; printf("FALLO (linea %d): ", __LINE__); printf(__VA_ARGS__); printf("\n"); } } while (0)

/* ------------------------------------------- almacenamiento falso (memoria) */

static unsigned char disk[65536];
static int diskSize = 0;

int plat_storage_read(void *buf, int size)
{
    if (diskSize <= 0) return 0;
    if (size > diskSize) size = diskSize;
    memcpy(buf, disk, (size_t)size);
    return size;
}

void plat_storage_label(const char *detail) { (void)detail; }

int plat_storage_write(const void *buf, int size)
{
    if (size > (int)sizeof(disk)) return -1;
    memcpy(disk, buf, (size_t)size);
    diskSize = size;
    return size;
}

static Card C(int rank, int suit) { Card c = { (unsigned char)rank, (unsigned char)suit, 0, 0, 0 }; return c; }

/* ------------------------------------------------------------ evaluacion */

static void expect_eval(const char *name, const Card *c, int n, int type, const char *scores)
{
    HandEval e;
    int i;
    game_eval(c, n, &e);
    CHECK(e.type == type, "%s: tipo %d, esperado %d", name, e.type, type);
    for (i = 0; i < n; i++)
        CHECK(e.scores[i] == (scores[i] == '1'), "%s: carta %d puntua=%d", name, i, e.scores[i]);
}

static void test_eval(void)
{
    Card h[5];

    h[0] = C(14, 0); h[1] = C(9, 1); h[2] = C(5, 2);
    expect_eval("carta alta", h, 3, HT_HIGH, "100");

    h[0] = C(3, 0); h[1] = C(9, 1); h[2] = C(9, 2); h[3] = C(14, 3);
    expect_eval("pareja", h, 4, HT_PAIR, "0110");

    h[0] = C(3, 0); h[1] = C(9, 1); h[2] = C(9, 2); h[3] = C(3, 3); h[4] = C(13, 0);
    expect_eval("doble pareja", h, 5, HT_TWOPAIR, "11110");

    h[0] = C(7, 0); h[1] = C(7, 1); h[2] = C(7, 2);
    expect_eval("trio", h, 3, HT_TRIPS, "111");

    h[0] = C(5, 0); h[1] = C(6, 1); h[2] = C(7, 2); h[3] = C(8, 3); h[4] = C(9, 0);
    expect_eval("escalera", h, 5, HT_STRAIGHT, "11111");

    h[0] = C(14, 0); h[1] = C(2, 1); h[2] = C(3, 2); h[3] = C(4, 3); h[4] = C(5, 0);
    expect_eval("escalera baja con as", h, 5, HT_STRAIGHT, "11111");

    h[0] = C(14, 0); h[1] = C(13, 1); h[2] = C(12, 2); h[3] = C(11, 3); h[4] = C(10, 0);
    expect_eval("escalera alta", h, 5, HT_STRAIGHT, "11111");

    h[0] = C(13, 0); h[1] = C(14, 1); h[2] = C(2, 2); h[3] = C(3, 3); h[4] = C(4, 0);
    expect_eval("K-A-2-3-4 no es escalera", h, 5, HT_HIGH, "01000");

    h[0] = C(2, 1); h[1] = C(9, 1); h[2] = C(5, 1); h[3] = C(13, 1); h[4] = C(11, 1);
    expect_eval("color", h, 5, HT_FLUSH, "11111");

    h[0] = C(2, 1); h[1] = C(9, 1); h[2] = C(5, 1); h[3] = C(13, 1);
    expect_eval("4 del mismo palo no es color", h, 4, HT_HIGH, "0001");

    h[0] = C(4, 0); h[1] = C(4, 1); h[2] = C(12, 2); h[3] = C(12, 3); h[4] = C(4, 2);
    expect_eval("full", h, 5, HT_FULL, "11111");

    h[0] = C(4, 0); h[1] = C(4, 1); h[2] = C(4, 2); h[3] = C(4, 3); h[4] = C(14, 2);
    expect_eval("poker", h, 5, HT_QUADS, "11110");

    h[0] = C(5, 2); h[1] = C(6, 2); h[2] = C(7, 2); h[3] = C(8, 2); h[4] = C(9, 2);
    expect_eval("escalera de color", h, 5, HT_STRFLUSH, "11111");

    h[0] = C(8, 0); h[1] = C(8, 1); h[2] = C(8, 2); h[3] = C(8, 3); h[4] = C(8, 0);
    expect_eval("repoker", h, 5, HT_FIVE, "11111");
}

/* ------------------------------------------------------------ puntuacion */

/* Monta una mano concreta: las `play` primeras cartas se juegan y el resto se queda en la mano */
static long long play_fixed2(Game *g, const Card *c, int n, int play, ScoreResult *res)
{
    int i;
    g->handCount = n;
    for (i = 0; i < n; i++) {
        g->hand[i] = c[i];
        g->hand[i].id = (unsigned char)i;
        g->sel[i] = (unsigned char)(i < play);
    }
    game_play(g, res);
    return res->total;
}

static long long play_fixed(Game *g, const Card *c, int n, ScoreResult *res)
{
    return play_fixed2(g, c, n, n, res);
}

static void fresh(Game *g)
{
    game_new_run(g, 12345, DECK_CLASICA, DIFF_NORMAL);
    game_setup_round(g);
    game_begin_round(g);
}

static void add_relic(Game *g, int id)
{
    g->relics[g->relicCount] = id;
    g->relicCounter[g->relicCount] = 0;
    g->relicCount++;
}

static void test_score(void)
{
    static Game g;
    static ScoreResult res;
    Card h[8];
    long long t;

    /* pareja de ases: (20 + 11 + 11) x 2 = 84 */
    fresh(&g);
    h[0] = C(14, 0); h[1] = C(14, 1);
    t = play_fixed(&g, h, 2, &res);
    CHECK(t == 84, "pareja de ases = %lld, esperado 84", t);

    /* carta alta K con un 2 que no puntua: (10 + 10) x 1 = 20 */
    fresh(&g);
    h[0] = C(13, 0); h[1] = C(2, 1);
    t = play_fixed(&g, h, 2, &res);
    CHECK(t == 20, "carta alta K = %lld, esperado 20", t);

    /* color de corazones 2,5,9,J,K: (55 + 2+5+9+10+10) x 4 = 364 */
    fresh(&g);
    h[0] = C(2, 1); h[1] = C(5, 1); h[2] = C(9, 1); h[3] = C(11, 1); h[4] = C(13, 1);
    t = play_fixed(&g, h, 5, &res);
    CHECK(t == 364, "color = %lld, esperado 364", t);

    /* el mismo color con Relicario (+3 por corazon) y Tinte (x2): 91 x (4 + 15) x 2 = 3458 */
    fresh(&g);
    add_relic(&g, R_RELICARIO);
    add_relic(&g, R_TINTE);
    t = play_fixed(&g, h, 5, &res);
    CHECK(t == 3458, "color con amuletos = %lld, esperado 3458", t);

    /* el orden de los amuletos importa: Tinte antes que Herradura -> 91 x (4 x 2 + 4) = 1092 */
    fresh(&g);
    add_relic(&g, R_TINTE);
    add_relic(&g, R_HERRADURA);
    t = play_fixed(&g, h, 5, &res);
    CHECK(t == 1092, "tinte+herradura = %lld, esperado 1092", t);

    /* Farol: carta alta As x4 -> (10 + 11) x 4 = 84 */
    fresh(&g);
    add_relic(&g, R_FAROL);
    h[0] = C(14, 0);
    t = play_fixed(&g, h, 1, &res);
    CHECK(t == 84, "farol = %lld, esperado 84", t);

    /* Eco repite la ultima carta: pareja de 9 -> (20 + 9 + 9 + 9) x 2 = 94 */
    fresh(&g);
    add_relic(&g, R_ECO);
    h[0] = C(9, 0); h[1] = C(9, 1);
    t = play_fixed(&g, h, 2, &res);
    CHECK(t == 94, "eco = %lld, esperado 94", t);

    /* Eco + Preludio + muesca de eco en la primera: 9 x3 la primera, x2 la segunda -> (20 + 45) x 2 = 130 */
    fresh(&g);
    add_relic(&g, R_ECO);
    add_relic(&g, R_PRELUDIO);
    h[0] = C(9, 0); h[0].notch = NOTCH_ECO;
    h[1] = C(9, 1);
    t = play_fixed(&g, h, 2, &res);
    CHECK(t == 130, "repeticiones = %lld, esperado 130", t);

    /* marcas: pareja de 9, una Plata (+30) y otra Cristal (x1,5) -> (20+9+30+9) x 3 = 204 */
    fresh(&g);
    h[0] = C(9, 0); h[0].mark = MARK_SILVER;
    h[1] = C(9, 1); h[1].mark = MARK_CRYSTAL;
    t = play_fixed(&g, h, 2, &res);
    CHECK(t == 204, "marcas = %lld, esperado 204", t);

    /* marca de Oro y Lupa: dinero */
    fresh(&g);
    add_relic(&g, R_LUPA);
    h[0] = C(9, 3); h[0].mark = MARK_GOLD;
    h[1] = C(9, 3);
    play_fixed(&g, h, 2, &res);
    CHECK(res.money == 4, "dinero = %d, esperado 4", res.money);

    /* rival El Republicano: las figuras no puntuan -> pareja de K = 20 x 2 = 40 */
    fresh(&g);
    g.bossRule = B_FACES;
    h[0] = C(13, 0); h[1] = C(13, 1);
    t = play_fixed(&g, h, 2, &res);
    CHECK(t == 40, "figuras anuladas = %lld, esperado 40", t);

    /* nivel 3 de pareja: (20 + 2x15 + 22) x (2 + 2) = 288 */
    fresh(&g);
    g.handLevel[HT_PAIR] = 3;
    h[0] = C(14, 0); h[1] = C(14, 1);
    t = play_fixed(&g, h, 2, &res);
    CHECK(t == 288, "pareja nivel 3 = %lld, esperado 288", t);

    /* cartas en la mano: pareja de 9 jugada; se quedan K, J (Banquillo +3 cada una) y un 4 con
       muesca de ancla (+4), y Lastre da +8 fichas por las tres -> (20+18+24) x (2+3+3+4) = 744 */
    fresh(&g);
    add_relic(&g, R_BANQUILLO);
    add_relic(&g, R_LASTRE);
    h[0] = C(9, 0); h[1] = C(9, 1); h[2] = C(13, 2); h[3] = C(11, 3);
    h[4] = C(4, 0); h[4].notch = NOTCH_ANCLA;
    t = play_fixed2(&g, h, 5, 2, &res);
    CHECK(t == 744, "cartas en mano = %lld, esperado 744", t);

    /* Suplente: la carta que no puntua da sus fichas. Pareja de 9 + un As -> (20+9+9+11) x 2 = 98 */
    fresh(&g);
    add_relic(&g, R_SUPLENTE);
    h[0] = C(9, 0); h[1] = C(9, 1); h[2] = C(14, 2);
    t = play_fixed(&g, h, 3, &res);
    CHECK(t == 98, "suplente = %lld, esperado 98", t);

    /* Primera impresion solo en la primera jugada; Mano limpia mientras no se descarte */
    fresh(&g);
    add_relic(&g, R_IMPRESION);
    add_relic(&g, R_LIMPIA);
    h[0] = C(14, 0); h[1] = C(14, 1);
    t = play_fixed(&g, h, 2, &res);          /* (42 + 60) x (2 + 6) x 2 = 1632 */
    CHECK(t == 1632, "primera impresion = %lld, esperado 1632", t);
    game_finish_play(&g, &res);
    g.discardsUsed = 1;
    t = play_fixed(&g, h, 2, &res);          /* ya no aplica ninguno: 84 */
    CHECK(t == 84, "segunda jugada = %lld, esperado 84", t);

    /* Pareja real y Plebe son excluyentes: K y Q puntuando en doble pareja */
    fresh(&g);
    add_relic(&g, R_PAREJA_REAL);
    add_relic(&g, R_SIN_FIGURAS);
    h[0] = C(13, 0); h[1] = C(13, 1); h[2] = C(12, 2); h[3] = C(12, 3);
    t = play_fixed(&g, h, 4, &res);          /* (30 + 40) x 2 x 2 = 280 */
    CHECK(t == 280, "pareja real = %lld, esperado 280", t);

    /* Metronomo: +0 la primera vez, +1 despues de cada jugada */
    fresh(&g);
    add_relic(&g, R_METRONOMO);
    h[0] = C(14, 0);
    t = play_fixed(&g, h, 1, &res);
    CHECK(t == 21, "metronomo 1 = %lld, esperado 21", t);
    game_finish_play(&g, &res);
    t = play_fixed(&g, h, 1, &res);
    CHECK(t == 42, "metronomo 2 = %lld, esperado 42", t);

    /* Mazo fino: con 48 cartas vivas, +16 Multi */
    fresh(&g);
    add_relic(&g, R_FINO);
    g.deck[0].rank = 0; g.deck[1].rank = 0; g.deck[2].rank = 0; g.deck[3].rank = 0;
    h[0] = C(14, 0);
    t = play_fixed(&g, h, 1, &res);
    CHECK(t == 21 * 17, "mazo fino = %lld, esperado %d", t, 21 * 17);

    /* finish_play suma, gasta jugada y repone la mano */
    fresh(&g);
    {
        int hands = g.handsLeft, deck = g.drawCount + g.handCount;
        g.sel[0] = 1;
        game_play(&g, &res);
        game_finish_play(&g, &res);
        CHECK(g.handsLeft == hands - 1, "jugadas restantes %d", g.handsLeft);
        CHECK(g.handCount == game_hand_size(&g), "mano de %d cartas", g.handCount);
        CHECK(g.drawCount + g.handCount == deck - 1, "cartas en juego %d", g.drawCount + g.handCount);
        CHECK(g.score == res.total, "puntos %lld", g.score);
    }
}

/* ---------------------------------------------------------------- trucos */

static void test_tricks(void)
{
    static Game g;
    int id, alive, n;

    /* Tinte: cambia el palo en la mano y en el mazo */
    fresh(&g);
    g.tricks[0] = T_TINTE_H; g.trickCount = 1;
    CHECK(game_use_trick(&g, 0) == TRICK_NEED_CARDS, "tinte sin cartas deberia pedir cartas");
    CHECK(g.trickCount == 1, "el truco no debe gastarse si falla");
    g.sel[0] = g.sel[1] = 1;
    id = g.hand[0].id;
    CHECK(game_use_trick(&g, 0) == TRICK_OK, "tinte");
    CHECK(g.trickCount == 0, "el truco debe gastarse");
    CHECK(g.deck[id].suit == SUIT_HEARTS, "el mazo no recibio el cambio de palo");
    CHECK(game_selected_count(&g) == 0, "la seleccion debe limpiarse");

    /* Lima: K pasa a As, el As se queda */
    fresh(&g);
    g.hand[0] = C(13, 0); g.hand[0].id = 0;
    g.hand[1] = C(14, 1); g.hand[1].id = 1;
    g.handCount = 2;
    g.sel[0] = g.sel[1] = 1;
    g.tricks[0] = T_LIMA; g.trickCount = 1;
    CHECK(game_use_trick(&g, 0) == TRICK_OK, "lima");
    CHECK(g.hand[0].rank == 14 && g.hand[1].rank == 14, "lima: rangos %d %d", g.hand[0].rank, g.hand[1].rank);

    /* Cizalla: destruye, repone la mano y el mazo encoge en la ronda siguiente */
    fresh(&g);
    alive = game_deck_alive(&g);
    g.sel[0] = g.sel[1] = 1;
    g.tricks[0] = T_CIZALLA; g.trickCount = 1;
    CHECK(game_use_trick(&g, 0) == TRICK_OK, "cizalla");
    CHECK(game_deck_alive(&g) == alive - 2, "cartas vivas %d", game_deck_alive(&g));
    CHECK(g.handCount == game_hand_size(&g), "la mano deberia reponerse");
    game_begin_round(&g);
    CHECK(g.deckCount == alive - 2, "mazo tras compactar %d", g.deckCount);

    /* Gemelo: la copia entra en la mano y en el mazo */
    fresh(&g);
    n = g.handCount;
    g.sel[2] = 1;
    g.tricks[0] = T_GEMELO; g.trickCount = 1;
    CHECK(game_use_trick(&g, 0) == TRICK_OK, "gemelo");
    CHECK(g.handCount == n + 1 && g.deckCount == 53, "gemelo: mano %d, mazo %d", g.handCount, g.deckCount);

    /* instantaneos */
    fresh(&g);
    g.tricks[0] = T_SOPLO; g.tricks[1] = T_INTENTO; g.tricks[2] = T_PROPINA; g.trickCount = 3;
    n = g.discardsLeft;
    CHECK(game_use_trick(&g, 0) == TRICK_OK && g.discardsLeft == n + 1, "soplo");
    n = g.handsLeft;
    CHECK(game_use_trick(&g, 0) == TRICK_OK && g.handsLeft == n + 1, "prorroga");
    n = g.money;
    CHECK(game_use_trick(&g, 0) == TRICK_OK && g.money == n + 8, "sobre");
    CHECK(g.trickCount == 0, "deberian haberse gastado los tres");

    /* muesca de trueque: $1 al descartar */
    fresh(&g);
    g.hand[0].notch = NOTCH_TRUEQUE;
    g.sel[0] = 1;
    n = g.money;
    CHECK(game_discard(&g) && g.money == n + 1, "trueque: dinero %d", g.money);

    /* bolsillos: 2 de base, 3 con el amuleto */
    fresh(&g);
    CHECK(game_pockets(&g) == 2, "bolsillos base %d", game_pockets(&g));
    add_relic(&g, R_BOLSILLO);
    CHECK(game_pockets(&g) == 3, "bolsillos con amuleto %d", game_pockets(&g));
}

/* ---------------------------------------------------------------- barajas */

static void test_decks(void)
{
    static Game g;
    int i, n;

    game_new_run(&g, 7, DECK_CLASICA, DIFF_NORMAL);
    CHECK(g.deckCount == 52 && g.money == 4 && g.baseHands == 4, "clasica");
    game_new_run(&g, 7, DECK_CORTA, DIFF_NORMAL);
    CHECK(g.deckCount == 40, "corta: %d cartas", g.deckCount);
    for (i = 0; i < g.deckCount; i++) CHECK(g.deck[i].rank >= 5, "corta: hay un %d", g.deck[i].rank);
    game_new_run(&g, 7, DECK_REAL, DIFF_NORMAL);
    CHECK(g.deckCount == 64, "real: %d cartas", g.deckCount);
    game_new_run(&g, 7, DECK_MARCADA, DIFF_NORMAL);
    for (n = 0, i = 0; i < g.deckCount; i++) n += g.deck[i].mark != 0;
    CHECK(n == 8 && g.money == 0, "marcada: %d marcas, $%d", n, g.money);
    game_new_run(&g, 7, DECK_AVARO, DIFF_NORMAL);
    game_setup_round(&g);
    game_begin_round(&g);
    CHECK(g.money == 15 && g.handsLeft == 3, "avaro: $%d, %d jugadas", g.money, g.handsLeft);
}

/* --------------------------------------------------------------- guardado */

static void test_save(void)
{
    static Game a, b;
    static ScoreResult res;
    Reward r;
    int resume = -1, i;
    long long t1, t2;

    diskSize = 0;
    save_init();
    CHECK(!save_has_run() && save_profile()->runs == 0, "perfil nuevo");
    CHECK(save_deck_unlocked(DECK_CLASICA) && !save_deck_unlocked(DECK_CORTA), "bloqueos iniciales");

    /* una partida a medias, con un amuleto y un truco */
    game_new_run(&a, 4242, DECK_CLASICA, DIFF_NORMAL);
    add_relic(&a, R_DADO);
    a.tricks[0] = T_LIMA; a.trickCount = 1;
    a.money = 17;
    game_setup_round(&a);
    save_relic_bought(R_DADO);
    save_run(&a, RESUME_INTRO);
    CHECK(diskSize > 0, "no se escribio nada");

    /* seguir jugando sin guardar... */
    game_begin_round(&a);
    a.sel[0] = a.sel[1] = 1;
    game_play(&a, &res);
    t1 = res.total;

    /* ...y "reiniciar la consola": al cargar, la misma jugada debe dar exactamente lo mismo
       (incluido el azar del Dado cargado y del barajado) */
    save_init();
    CHECK(save_has_run(), "deberia haber partida guardada");
    CHECK(save_load_run(&b, &resume) && resume == RESUME_INTRO, "carga de partida");
    CHECK(b.money == 17 && b.relicCount == 1 && b.trickCount == 1 && b.tricks[0] == T_LIMA, "estado restaurado");
    CHECK(save_is_found(R_DADO) && save_found_count() == 1, "amuletos descubiertos");
    game_begin_round(&b);
    b.sel[0] = b.sel[1] = 1;
    game_play(&b, &res);
    t2 = res.total;
    CHECK(t1 == t2, "tras cargar la jugada da %lld en vez de %lld", t2, t1);

    /* fin de partida: records, desbloqueos y partida borrada */
    b.mesa = 2;
    b.bestHand = 999;
    save_run_ended(&b);
    save_init();
    CHECK(!save_has_run(), "la partida deberia haberse borrado");
    CHECK(save_profile()->runs == 1 && save_profile()->bestMesa == 2 && save_profile()->bestHand == 999, "records");
    CHECK(save_deck_unlocked(DECK_CORTA) && save_deck_unlocked(DECK_REAL) && !save_deck_unlocked(DECK_AVARO), "desbloqueos");

    /* un archivo danado se ignora en vez de cargarse */
    disk[diskSize / 2] ^= 0x5A;
    save_init();
    CHECK(save_profile()->runs == 0 && !save_has_run(), "un guardado danado no debe cargarse");
    diskSize = 7;
    save_init();
    CHECK(save_profile()->runs == 0, "un guardado truncado no debe cargarse");

    /* 12 amuletos descubiertos desbloquean la baraja marcada */
    diskSize = 0;
    save_init();
    for (i = 0; i < 12; i++) save_relic_bought(i);
    CHECK(save_deck_unlocked(DECK_MARCADA), "baraja marcada");

    /* la recompensa usa las reglas de fin de ronda */
    fresh(&a);
    add_relic(&a, R_PROPINA);
    add_relic(&a, R_HUCHA);
    add_relic(&a, R_NIEVE);
    a.money = 20;
    add_relic(&a, R_CAJA);
    game_collect_reward(&a, &r);
    CHECK(r.relic == 4 + a.discardsLeft + 2, "premio de amuletos %d", r.relic);
    CHECK(a.relicCounter[2] == 1, "bola de nieve %d", a.relicCounter[2]);
    diskSize = 0;
    save_init();
}

/* ------------------------------------------- rivales, dificultad, infinito */

static void test_bosses(void)
{
    static Game g;
    static ScoreResult res;
    Card h[8];
    long long t, base;
    int i, seen[B_COUNT];

    /* El Tuerto: la carta de la izquierda no puntua -> pareja de ases = (20 + 11) x 2 */
    fresh(&g);
    g.bossRule = B_LEFT;
    h[0] = C(14, 0); h[1] = C(14, 1);
    t = play_fixed(&g, h, 2, &res);
    CHECK(t == 62, "tuerto = %lld, esperado 62", t);

    /* El Maniatico: los pares no puntuan -> pareja de 8 = 20 x 2; pareja de 9 normal */
    fresh(&g);
    g.bossRule = B_EVEN;
    h[0] = C(8, 0); h[1] = C(8, 1);
    t = play_fixed(&g, h, 2, &res);
    CHECK(t == 40, "maniatico = %lld, esperado 40", t);
    h[0] = C(9, 0); h[1] = C(9, 1);
    t = play_fixed(&g, h, 2, &res);
    CHECK(t == 76, "maniatico impares = %lld, esperado 76", t);

    /* El Olvidadizo: los niveles no cuentan */
    fresh(&g);
    g.handLevel[HT_PAIR] = 5;
    g.bossRule = B_FORGET;
    h[0] = C(14, 0); h[1] = C(14, 1);
    t = play_fixed(&g, h, 2, &res);
    CHECK(t == 84, "olvidadizo = %lld, esperado 84", t);

    /* El Aburrido: la segunda pareja de la ronda vale la mitad */
    fresh(&g);
    g.bossRule = B_REPEAT;
    t = play_fixed(&g, h, 2, &res);
    CHECK(t == 84, "aburrido 1 = %lld", t);
    game_finish_play(&g, &res);
    t = play_fixed(&g, h, 2, &res);
    CHECK(t == 42, "aburrido 2 = %lld, esperado 42", t);

    /* El Peajero: cada jugada cuesta $2 */
    fresh(&g);
    g.bossRule = B_TOLL;
    g.money = 5;
    play_fixed(&g, h, 2, &res);
    game_finish_play(&g, &res);
    CHECK(g.money == 3, "peajero: $%d", g.money);

    /* El Ladron: el amuleto de la izquierda no actua, el segundo si */
    fresh(&g);
    add_relic(&g, R_HERRADURA);
    add_relic(&g, R_GEMELOS);
    g.bossRule = B_THIEF;
    t = play_fixed(&g, h, 2, &res);
    CHECK(t == 42 * 10, "ladron = %lld, esperado 420", t);

    /* El Purista: ni marcas ni muescas */
    fresh(&g);
    g.bossRule = B_PURE;
    h[0] = C(9, 0); h[0].mark = MARK_CRYSTAL; h[0].notch = NOTCH_ECO;
    h[1] = C(9, 1); h[1].mark = MARK_SILVER;
    t = play_fixed(&g, h, 2, &res);
    CHECK(t == 76, "purista = %lld, esperado 76", t);

    /* El Madrugon: la primera jugada vale 0, la segunda es normal */
    fresh(&g);
    g.bossRule = B_EARLY;
    h[0] = C(14, 0); h[1] = C(14, 1);
    t = play_fixed(&g, h, 2, &res);
    CHECK(t == 0, "madrugon 1 = %lld, esperado 0", t);
    game_finish_play(&g, &res);
    t = play_fixed(&g, h, 2, &res);
    CHECK(t == 84, "madrugon 2 = %lld, esperado 84", t);

    /* El Daltonico: un color entero anulado */
    fresh(&g);
    g.bossRule = B_COLOR;
    g.bossSuit = 1;
    h[0] = C(14, SUIT_HEARTS); h[1] = C(14, SUIT_DIAMONDS);
    t = play_fixed(&g, h, 2, &res);
    CHECK(t == 40, "daltonico rojas = %lld, esperado 40", t);
    h[0] = C(14, SUIT_SPADES); h[1] = C(14, SUIT_CLUBS);
    t = play_fixed(&g, h, 2, &res);
    CHECK(t == 84, "daltonico negras = %lld, esperado 84", t);

    /* el sorteo respeta la mesa minima y no repite rival */
    memset(seen, 0, sizeof(seen));
    game_new_run(&g, 99, DECK_CLASICA, DIFF_NORMAL);
    g.round = 2;
    for (i = 0; i < 400; i++) {
        int last = g.lastBoss;
        g.mesa = 0;
        game_setup_round(&g);
        CHECK(g.bossRule != last, "rival repetido");
        CHECK(BOSS_INFO[g.bossRule].minMesa == 0, "rival %d demasiado pronto", g.bossRule);
    }
    for (i = 0; i < 800; i++) {
        g.mesa = 5;
        game_setup_round(&g);
        seen[g.bossRule] = 1;
    }
    for (i = 1; i < B_COUNT; i++) CHECK(seen[i], "el rival %d no sale nunca", i);

    /* dificultad: objetivos mas altos, un descarte menos y tienda mas cara en Leyenda */
    game_new_run(&g, 5, DECK_CLASICA, DIFF_NORMAL);
    game_setup_round(&g);
    base = g.target;
    game_new_run(&g, 5, DECK_CLASICA, DIFF_HARD);
    game_setup_round(&g);
    CHECK(g.target == base * 13 / 10, "objetivo tahur %lld", g.target);
    game_new_run(&g, 5, DECK_CLASICA, DIFF_EXPERT);
    game_setup_round(&g);
    game_begin_round(&g);
    CHECK(g.target == base * 17 / 10 && g.discardsLeft == 2, "leyenda: objetivo %lld, descartes %d", g.target, g.discardsLeft);
    game_shop_enter(&g);
    CHECK(g.shop[2].price == 5, "leccion en leyenda cuesta $%d", g.shop[2].price);

    /* modo infinito: se gana una sola vez y los objetivos siguen creciendo */
    game_new_run(&g, 5, DECK_CLASICA, DIFF_NORMAL);
    g.mesa = NUM_MESAS - 1;
    g.round = 2;
    CHECK(game_next_round(&g) == 1, "deberia avisar de la victoria");
    g.endless = 1;
    game_setup_round(&g);
    base = g.target;
    CHECK(base > 32000, "objetivo de la mesa 9: %lld", base);
    g.round = 2;
    CHECK(game_next_round(&g) == 0 && g.mesa == NUM_MESAS + 1, "en infinito no se vuelve a ganar");
    game_setup_round(&g);
    CHECK(g.target > base, "la mesa 10 deberia pedir mas que la 9");
    g.mesa = 60;
    game_setup_round(&g);
    CHECK(g.target > 0, "desbordamiento del objetivo en mesas muy altas");

    /* vender trucos y usarlos fuera de la ronda */
    fresh(&g);
    g.tricks[0] = T_LIMA; g.tricks[1] = T_PROPINA; g.trickCount = 2;
    i = g.money;
    game_sell_trick(&g, 0);
    CHECK(g.trickCount == 1 && g.tricks[0] == T_PROPINA && g.money == i + 1, "venta de truco");
    CHECK(game_trick_shop_usable(T_PROPINA) && game_trick_shop_usable(T_LECCION) && !game_trick_shop_usable(T_SOPLO)
          && !game_trick_shop_usable(T_LIMA), "trucos usables en la tienda");
}

static void test_profile(void)
{
    static Game g;

    diskSize = 0;
    save_init();
    CHECK(save_difficulty_unlocked(DIFF_NORMAL) && !save_difficulty_unlocked(DIFF_HARD), "dificultades iniciales");
    save_mark_trick_found(T_GEMELO);
    CHECK(save_is_trick_found(T_GEMELO) && save_trick_found_count() == 1, "trucos descubiertos");

    /* ganar abre la dificultad siguiente y cuenta partida y victoria una sola vez */
    game_new_run(&g, 1, DECK_CLASICA, DIFF_NORMAL);
    g.mesa = NUM_MESAS;
    g.endless = 1;
    g.bestHand = 5000;
    save_run_won(&g);
    save_run(&g, RESUME_SHOP);
    save_init();
    CHECK(save_profile()->wins == 1 && save_profile()->runs == 1 && save_profile()->bestMesa == NUM_MESAS, "victoria");
    CHECK(save_difficulty_unlocked(DIFF_HARD) && !save_difficulty_unlocked(DIFF_EXPERT), "tahur desbloqueado");
    CHECK(save_has_run() && save_is_trick_found(T_GEMELO), "la partida infinita queda guardada");

    /* caer en la mesa 11 del modo infinito: record nuevo, sin contar otra partida */
    g.mesa = 10;
    save_run_ended(&g);
    save_init();
    CHECK(save_profile()->runs == 1 && save_profile()->bestMesa == 10 && !save_has_run(), "fin del modo infinito");
    diskSize = 0;
    save_init();
}

/* ------------------------ amuletos nuevos, reordenar, semilla, estadisticas */

static void test_extras(void)
{
    static Game g, g2;
    static ScoreResult res;
    Card h[8];
    long long t;
    int i;

    /* Navaja suiza: x1,3 por cada truco que lleves. As solo con 2 trucos: 21 x 1,69 = 35 */
    fresh(&g);
    add_relic(&g, R_SUIZA);
    g.tricks[0] = T_SOPLO; g.tricks[1] = T_LIMA; g.trickCount = 2;
    h[0] = C(14, 0);
    t = play_fixed(&g, h, 1, &res);
    CHECK(t == 35, "navaja suiza = %lld, esperado 35", t);

    /* Solitario (x3) con una carta; Carambola (x2,5) solo si puntuan las cinco */
    fresh(&g);
    add_relic(&g, R_SOLITARIO);
    add_relic(&g, R_CARAMBOLA);
    t = play_fixed(&g, h, 1, &res);
    CHECK(t == 63, "solitario = %lld, esperado 63", t);
    h[0] = C(2, 1); h[1] = C(5, 1); h[2] = C(9, 1); h[3] = C(11, 1); h[4] = C(13, 1);
    t = play_fixed(&g, h, 5, &res);
    CHECK(t == 910, "carambola = %lld, esperado 910", t);

    /* Alquimista: las cartas con marca puntuan dos veces. Pareja de 9, una de Plata */
    fresh(&g);
    add_relic(&g, R_ALQUIMISTA);
    h[0] = C(9, 0); h[0].mark = MARK_SILVER;
    h[1] = C(9, 1);
    t = play_fixed(&g, h, 2, &res);          /* (20 + 39 + 39 + 9) x 2 = 214 */
    CHECK(t == 214, "alquimista = %lld, esperado 214", t);

    /* Grabador (+40 por muesca) y Archivero (+2 Multi por carta modificada del mazo) */
    fresh(&g);
    add_relic(&g, R_GRABADOR);
    add_relic(&g, R_ARCHIVERO);
    for (i = 0; i < g.deckCount; i++) { g.deck[i].mark = 0; g.deck[i].notch = 0; }
    g.deck[40].mark = MARK_RUBY; g.deck[41].notch = NOTCH_ANCLA; g.deck[42].mark = MARK_GOLD;
    h[0] = C(9, 0); h[0].notch = NOTCH_TRUEQUE;
    h[1] = C(9, 1);
    t = play_fixed(&g, h, 2, &res);          /* (20 + 9 + 40 + 9) x (2 + 6) = 624 */
    CHECK(t == 624, "grabador + archivero = %lld, esperado 624", t);

    /* Chatarrero y Prestidigitador reaccionan al usar un truco */
    fresh(&g);
    add_relic(&g, R_CHATARRERO);
    add_relic(&g, R_PRESTI);
    g.tricks[0] = T_SOPLO; g.trickCount = 1;
    i = g.money;
    CHECK(game_use_trick(&g, 0) == TRICK_OK, "soplo");
    CHECK(g.money == i + 2 && g.relicCounter[0] == 3 && g.statTricks == 1, "reaccion al truco: $%d, contador %d", g.money, g.relicCounter[0]);
    h[0] = C(14, 0);
    t = play_fixed(&g, h, 1, &res);          /* 21 x (1 + 3) = 84 */
    CHECK(t == 84, "chatarrero = %lld, esperado 84", t);

    /* reordenar amuletos: el contador viaja con su amuleto y los extremos no se salen */
    fresh(&g);
    add_relic(&g, R_NIEVE);
    add_relic(&g, R_TINTE);
    add_relic(&g, R_HERRADURA);
    g.relicCounter[0] = 7;
    CHECK(game_move_relic(&g, 0, 1) == 1, "mover a la derecha");
    CHECK(g.relics[0] == R_TINTE && g.relics[1] == R_NIEVE && g.relicCounter[1] == 7 && g.relicCounter[0] == 0, "orden tras mover");
    CHECK(game_move_relic(&g, 0, -1) == 0 && game_move_relic(&g, 2, 1) == 2, "los extremos no se mueven");
    /* y el orden cambia la puntuacion: Tinte antes o despues de Herradura */
    h[0] = C(2, 1); h[1] = C(5, 1); h[2] = C(9, 1); h[3] = C(11, 1); h[4] = C(13, 1);
    g.relicCount = 0;
    add_relic(&g, R_TINTE);
    add_relic(&g, R_HERRADURA);
    t = play_fixed(&g, h, 5, &res);
    CHECK(t == 1092, "tinte, herradura = %lld", t);
    game_move_relic(&g, 0, 1);
    t = play_fixed(&g, h, 5, &res);
    CHECK(t == 1456, "herradura, tinte = %lld, esperado 1456", t);

    /* la misma semilla da la misma partida */
    game_new_run(&g, 0xC0FFEE, DECK_CLASICA, DIFF_NORMAL);
    CHECK(g.seed == 0xC0FFEE, "semilla guardada %x", g.seed);
    game_setup_round(&g);
    game_begin_round(&g);
    game_new_run(&g2, 0xC0FFEE, DECK_CLASICA, DIFF_NORMAL);
    game_setup_round(&g2);
    game_begin_round(&g2);
    for (i = 0; i < g.handCount; i++)
        CHECK(g.hand[i].rank == g2.hand[i].rank && g.hand[i].suit == g2.hand[i].suit, "mano distinta con la misma semilla");

    /* estadisticas de la partida y acumuladas */
    diskSize = 0;
    save_init();
    CHECK(save_profile()->musicVol == 7 && save_profile()->sfxVol == 10 && save_profile()->lang == LANG_AUTO, "opciones por defecto");
    fresh(&g);
    g.sel[0] = 1;
    game_play(&g, &res);
    game_finish_play(&g, &res);
    g.sel[0] = 1;
    CHECK(game_discard(&g), "descarte");
    CHECK(g.statHands == 1 && g.statDiscards == 1, "estadisticas de partida %d %d", g.statHands, g.statDiscards);
    save_relic_bought(R_ECO);
    save_relic_bought(R_ECO);
    save_run_ended(&g);
    save_set_options(3, 5, LANG_SET_EN);
    save_init();
    CHECK(save_profile()->totalHands == 1 && save_profile()->totalDiscards == 1, "estadisticas acumuladas");
    CHECK(save_profile()->relicBought[R_ECO] == 2 && save_is_found(R_ECO), "compras de un amuleto");
    CHECK(save_profile()->handPlays[res.eval.type] == 1, "manos por tipo");
    CHECK(save_profile()->musicVol == 3 && save_profile()->sfxVol == 5 && save_profile()->lang == LANG_SET_EN, "opciones guardadas");
    diskSize = 0;
    save_init();

    /* todos los textos existen en los dos idiomas */
    for (i = 0; i < R_COUNT; i++)
        CHECK(RELIC_INFO[i].name[0] && RELIC_INFO[i].name[1] && RELIC_INFO[i].desc[0] && RELIC_INFO[i].desc[1]
              && RELIC_INFO[i].name[1][0] && RELIC_INFO[i].desc[1][0] && RELIC_INFO[i].rules[0].when, "amuleto %d incompleto", i);
    for (i = 0; i < T_COUNT; i++) CHECK(TRICK_INFO[i].name[1] && TRICK_INFO[i].desc[1], "truco %d sin ingles", i);
    for (i = 1; i < B_COUNT; i++) CHECK(BOSS_INFO[i].name[1] && BOSS_INFO[i].desc[1][0], "rival %d sin ingles", i);
}

/* ----------------------------------------------------------- partida bot */

static long long estimate(const Game *g, const Card *c, int n, HandEval *e)
{
    int bc, bm, i;
    game_eval(c, n, e);
    game_hand_base(g, e->type, &bc, &bm);
    for (i = 0; i < n; i++)
        if (e->scores[i] && !game_card_debuffed(g, c[i])) bc += game_card_chips(c[i]);
    return (long long)bc * bm;
}

static int popcount(int m) { int n = 0; while (m) { n += m & 1; m >>= 1; } return n; }

static void check_round_state(const Game *g)
{
    unsigned char seen[MAX_DECK];
    int i;
    memset(seen, 0, sizeof(seen));
    CHECK(g->handCount >= 0 && g->handCount <= MAX_HAND, "tamano de mano %d", g->handCount);
    CHECK(g->drawCount >= 0 && g->drawCount <= g->deckCount, "mazo %d", g->drawCount);
    for (i = 0; i < g->handCount; i++) {
        CHECK(g->hand[i].id < g->deckCount, "id de carta fuera de rango");
        CHECK(g->hand[i].rank >= 2 && g->hand[i].rank <= 14, "rango %d", g->hand[i].rank);
        CHECK(!seen[g->hand[i].id], "carta duplicada en la mano");
        seen[g->hand[i].id] = 1;
    }
    for (i = 0; i < g->drawCount; i++) {
        CHECK(!seen[g->draw[i].id], "carta duplicada en el mazo");
        seen[g->draw[i].id] = 1;
    }
    CHECK(g->money >= 0, "dinero negativo");
    CHECK(g->trickCount >= 0 && g->trickCount <= game_pockets(g), "trucos %d", g->trickCount);
}

/* Gasta los trucos que tenga: los instantaneos tal cual y los de cartas sobre las mas bajas */
static void bot_use_tricks(Game *g)
{
    int guard = 0;
    while (g->trickCount > 0 && guard++ < 8) {
        const TrickInfo *info = &TRICK_INFO[g->tricks[0]];
        int i, code;
        game_clear_selection(g);
        if (g->tricks[0] == T_CIZALLA && game_deck_alive(g) < 40) break;
        for (i = 0; i < info->minSel && i < g->handCount; i++) g->sel[g->handCount - 1 - i] = 1;
        code = game_use_trick(g, 0);
        CHECK(code == TRICK_OK || code == TRICK_NO_EFFECT, "truco %d rechazado (%d)", g->tricks[0], code);
        if (code != TRICK_OK) break;
    }
    game_clear_selection(g);
}

/* Juega una partida entera con una estrategia sencilla. Devuelve la mesa alcanzada (8 = victoria). */
static int bot_run(unsigned int seed, int deck, long long *best)
{
    static Game g;
    static ScoreResult res;
    int guard = 0;

    game_new_run(&g, seed, deck, DIFF_NORMAL);
    for (;;) {
        game_setup_round(&g);
        game_begin_round(&g);
        check_round_state(&g);
        bot_use_tricks(&g);
        check_round_state(&g);

        while (!game_round_won(&g) && !game_round_lost(&g)) {
            int bestMask = 0, m, i, n;
            long long bestVal = -1;
            HandEval be;
            Card c[MAX_PLAY];

            if (++guard > 100000) { CHECK(0, "bucle infinito, semilla %u", seed); return -1; }
            memset(&be, 0, sizeof(be));
            for (m = 1; m < (1 << g.handCount); m++) {
                HandEval e;
                long long v;
                int pc = popcount(m);
                if (pc > MAX_PLAY) continue;
                if (g.bossRule == B_FIVE && pc != 5) continue;
                n = 0;
                for (i = 0; i < g.handCount; i++) if (m & (1 << i)) c[n++] = g.hand[i];
                v = estimate(&g, c, n, &e);
                if (v > bestVal) { bestVal = v; bestMask = m; be = e; }
            }
            CHECK(bestMask != 0, "sin jugada posible");
            if (!bestMask) return -1;

            game_clear_selection(&g);
            if (g.discardsLeft > 0 && be.type <= HT_PAIR && (g.score + bestVal * g.handsLeft) < g.target) {
                /* descarta hasta 5 cartas que no forman parte de la mejor jugada, las mas bajas */
                int k, picked = 0;
                for (k = g.handCount - 1; k >= 0 && picked < 5; k--) {
                    int inBest = 0, j = 0;
                    for (i = 0; i < g.handCount; i++) {
                        if (bestMask & (1 << i)) {
                            if (i == k && be.scores[j]) inBest = 1;
                            j++;
                        }
                    }
                    if (!inBest) { g.sel[k] = 1; picked++; }
                }
                CHECK(game_discard(&g), "descarte rechazado");
            } else {
                for (i = 0; i < g.handCount; i++) if (bestMask & (1 << i)) g.sel[i] = 1;
                CHECK(game_play_block(&g) == PLAY_OK, "jugada bloqueada");
                game_play(&g, &res);
                CHECK(res.evCount <= MAX_EVENTS, "demasiados eventos");
                CHECK(res.total >= 0, "total negativo");
                game_finish_play(&g, &res);
            }
            check_round_state(&g);
        }

        if (g.bestHand > *best) *best = g.bestHand;
        if (!game_round_won(&g)) return g.mesa;

        {
            Reward r;
            int before = g.money, i;
            game_collect_reward(&g, &r);
            CHECK(g.money == before + r.total && r.total >= 3, "premio incoherente");
            if (game_next_round(&g)) return NUM_MESAS;

            game_shop_enter(&g);
            for (i = 0; i < SHOP_OFFERS; i++) {
                int code = game_shop_buy(&g, i);
                CHECK(code == BUY_OK || code == BUY_NO_MONEY || code == BUY_NO_SPACE || code == BUY_SOLD, "codigo de compra %d", code);
            }
            if (g.money >= 12 && g.relicCount < MAX_RELICS && game_shop_reroll(&g))
                for (i = 0; i < 2; i++) game_shop_buy(&g, i);
            CHECK(g.relicCount <= MAX_RELICS && g.deckCount <= MAX_DECK, "limites superados");
        }
    }
}

int main(int argc, char **argv)
{
    int runs = argc > 1 ? atoi(argv[1]) : 2000;
    int hist[NUM_MESAS + 1];
    long long best = 0;
    int i, d;

    test_eval();
    test_score();
    test_tricks();
    test_decks();
    test_save();
    test_bosses();
    test_profile();
    test_extras();
    printf("pruebas de reglas, trucos, barajas y guardado: %s\n", failures ? "CON FALLOS" : "bien");

    for (d = 0; d < DECK_COUNT; d++) {
        int n = d == 0 ? runs : runs / 4, sum = 0;
        memset(hist, 0, sizeof(hist));
        for (i = 0; i < n; i++) {
            int m = bot_run(1000u + (unsigned int)i * 7919u, d, &best);
            if (m >= 0 && m <= NUM_MESAS) { hist[m]++; sum += m; }
        }
        printf("baraja %-10s %5d partidas | mesas superadas de media %.2f | victorias %.1f%% | ", DECK_INFO[d].name[0], n,
               (double)sum / n, 100.0 * hist[NUM_MESAS] / n);
        for (i = 0; i <= NUM_MESAS; i++) printf("%d ", hist[i] * 100 / n);
        printf("\n");
    }
    printf("mejor jugada vista: %lld\n", best);
    printf("%s (%d fallos)\n", failures ? "HAY FALLOS" : "TODO BIEN", failures);
    return failures ? 1 : 0;
}
