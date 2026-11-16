/* FAROL - modo de autoprueba (solo con -DAUTOTEST=n).
 *
 * Se incluye al final de ui.c. Un bot sustituye al mando: juega la partida a
 * traves de la interfaz real (menus, cartas, trucos, tienda) y pide capturas
 * de pantalla en los momentos interesantes. Las capturas las vuelca
 * gfx_dump() a ms0:/farol_<nombre>.raw.
 *
 *   AUTOTEST=1  partida completa desde un perfil vacio
 *   AUTOTEST=2  se detiene al llegar a la primera tienda (deja una partida guardada)
 *   AUTOTEST=3  lee el guardado, elige "Continuar" y comprueba donde aparece
 *   AUTOTEST=4  gana todas las rondas: llega a la victoria y sigue dos mesas en modo infinito
 */

int savedata_official(void);

enum {
    SH_TITLE, SH_COLLECTION, SH_COL_TRICKS, SH_RECORDS, SH_DECK, SH_INTRO, SH_HAND, SH_SELECT, SH_SCORING,
    SH_TOTAL, SH_ROUNDWIN, SH_REWARD, SH_SHOP, SH_SHOP2, SH_HAND2, SH_INFO, SH_INFO_ITEMS, SH_INFO_DECK,
    SH_PAUSE, SH_RELIC_EV, SH_BOSS_INTRO, SH_BOSS_PLAY, SH_INFO_SHOP, SH_SHOP_TRICK, SH_TRICK, SH_TRICK_DONE,
    SH_HELD_EV, SH_END, SH_RESUME, SH_VICTORY, SH_ENDLESS, SH_COUNT
};

static const char *const SH_NAME[SH_COUNT] = {
    "01_titulo", "02_coleccion", "03_coleccion_trucos", "04_records", "05_barajas", "06_intro", "07_mano",
    "08_seleccion", "09_puntuando", "10_total", "11_ronda_superada", "12_premio", "13_tienda",
    "14_tienda_compra", "15_mano_amuletos", "16_info_manos", "17_info_amuletos", "18_info_mazo",
    "19_pausa", "20_evento_amuleto", "21_intro_rival", "22_ronda_rival", "23_mazo_en_tienda",
    "24_tienda_truco", "25_truco_menu", "26_truco_usado", "27_evento_en_mano", "28_fin", "29_continuar",
    "30_victoria", "31_infinito"
};

static unsigned int at_seen;
static const char *at_pending;
static int at_done, at_cool, at_frames, at_lastState = -1, at_lastPhase = -1;
static int at_plan, at_planMask, at_planDiscard, at_pauseAsked;
static int at_colStep, at_trickTries, at_lastTricks, at_resumeOk = -1;
static int at_infoStep, at_infoShop, at_soldTrick;

static int at_shot(int id)
{
    if (at_seen & (1u << id)) return 0;
    at_seen |= 1u << id;
    at_pending = SH_NAME[id];
    return 1;
}

static unsigned int at_press(unsigned int b)
{
    at_cool = 5;
    return b;
}

static long long at_estimate(const Card *c, int n, HandEval *e)
{
    int bc, bm, i;
    game_eval(c, n, e);
    game_hand_base(&g, e->type, &bc, &bm);
    for (i = 0; i < n; i++)
        if (e->scores[i] && !game_card_debuffed(&g, c[i])) bc += game_card_chips(c[i]);
    return (long long)bc * bm;
}

/* Elige la mejor jugada de la mano, o un descarte si la jugada es floja */
static void at_make_plan(void)
{
    int m, i, n, best = 0;
    long long bestVal = -1;
    HandEval be;

    memset(&be, 0, sizeof(be));
    for (m = 1; m < (1 << g.handCount); m++) {
        Card c[MAX_PLAY];
        HandEval e;
        long long v;
        int pc = 0;
        for (i = 0; i < g.handCount; i++) pc += (m >> i) & 1;
        if (pc > MAX_PLAY) continue;
        if (g.bossRule == B_FIVE && pc != 5) continue;
        n = 0;
        for (i = 0; i < g.handCount; i++)
            if (m & (1 << i)) c[n++] = g.hand[i];
        v = at_estimate(c, n, &e);
        if (v > bestVal) { bestVal = v; best = m; be = e; }
    }

    at_planMask = best;
    at_planDiscard = 0;
#if AUTOTEST != 4
    if (g.discardsLeft > 0 && be.type <= HT_PAIR && g.score + bestVal * g.handsLeft < g.target) {
        int k, picked = 0, mask = 0;
        for (k = g.handCount - 1; k >= 0 && picked < 5; k--) {
            int inBest = 0, j = 0;
            for (i = 0; i < g.handCount; i++) {
                if (best & (1 << i)) {
                    if (i == k && be.scores[j]) inBest = 1;
                    j++;
                }
            }
            if (!inBest) { mask |= 1 << k; picked++; }
        }
        if (picked > 0) {
            at_planMask = mask;
            at_planDiscard = 1;
        }
    }
#endif
}

/* Lleva la seleccion de la mano a `mask` con un boton por llamada. Devuelve 0 si ya coincide. */
static unsigned int at_select_mask(int mask)
{
    int i;
    for (i = 0; i < g.handCount; i++) {
        int want = (mask >> i) & 1;
        if ((g.sel[i] != 0) != want) {
            if (cursor < i) return at_press(BTN_RIGHT);
            if (cursor > i) return at_press(BTN_LEFT);
            return at_press(BTN_CROSS);
        }
    }
    return 0;
}

unsigned int ui_autotest_buttons(void)
{
    unsigned int b;
    int i, n;

    if (at_done) return 0;
    if (state != at_lastState || (state == ST_PLAY && phase != at_lastPhase)) {
        at_lastState = state;
        at_lastPhase = phase;
        at_frames = 0;
    } else {
        at_frames++;
    }
    if (frame > 120000) { at_done = 1; return 0; }
    if (at_cool > 0) { at_cool--; return 0; }

    switch (state) {
    case ST_TITLE: {
        int ids[3], want = TI_NEW, target = -1;
        n = title_items(ids);
        if (at_frames < 45) return 0;
        if (at_shot(SH_TITLE)) return 0;
#if AUTOTEST == 3
        want = TI_CONTINUE;
#else
        if (at_colStep < 4) want = TI_COLLECTION;
#endif
        for (i = 0; i < n; i++)
            if (ids[i] == want) target = i;
        if (target < 0) {            /* no hay partida que continuar: la prueba falla */
            at_resumeOk = 0;
            at_done = 1;
            return 0;
        }
        if (titleSel < target) return at_press(BTN_DOWN);
        if (titleSel > target) return at_press(BTN_UP);
        return at_press(BTN_CROSS);
    }

    case ST_COLLECTION:
        if (at_frames < 15) return 0;
        /* amuletos -> (L) records -> (L) trucos -> salir */
        if (at_colStep == 0) { if (at_shot(SH_COLLECTION)) return 0; at_colStep = 1; return at_press(BTN_L); }
        if (at_colStep == 1) { if (at_shot(SH_RECORDS)) return 0; at_colStep = 2; return at_press(BTN_L); }
        if (at_colStep == 2) { if (at_shot(SH_COL_TRICKS)) return 0; at_colStep = 3; return 0; }
        at_colStep = 4;
        return at_press(BTN_CIRCLE);

    case ST_DECK:
        if (at_frames < 20) return 0;
        if (at_shot(SH_DECK)) return 0;
        return at_press(BTN_CROSS);

    case ST_INTRO:
        at_trickTries = 0;
        if (at_frames < 40) return 0;
#if AUTOTEST == 4
        if (g.endless && g.mesa >= NUM_MESAS + 2) {      /* ya ha superado dos mesas del modo infinito */
            if (at_shot(SH_ENDLESS)) return 0;
            at_done = 1;
            return 0;
        }
#endif
        if (at_shot(g.bossRule != B_NONE ? SH_BOSS_INTRO : SH_INTRO)) return 0;
        return at_press(BTN_CROSS);

    case ST_PLAY:
        if (paused) {
            if (at_shot(SH_PAUSE)) return 0;
            return at_press(BTN_START);
        }
        if (phase != PH_SELECT) {
            if (phase == PH_EVENTS && evIdx < res.evCount && evTimer == 3) {
                if (evIdx == 1 && at_shot(SH_SCORING)) return 0;
                if (res.ev[evIdx].held >= 0 && at_shot(SH_HELD_EV)) return 0;
                if (res.ev[evIdx].relic >= 0 && res.ev[evIdx].held < 0 && at_shot(SH_RELIC_EV)) return 0;
            }
            if (phase == PH_TOTAL && timer == 5 && at_shot(SH_TOTAL)) return 0;
            if (phase == PH_END && timer == 20 && game_round_won(&g) && at_shot(SH_ROUNDWIN)) return 0;
            /* tras la primera jugada, acelera el recuento */
            return ((at_seen & (1u << SH_TOTAL)) && phase != PH_END) ? BTN_CROSS : 0;
        }
        if (at_frames < 30) return 0;
        if (g.trickCount < at_lastTricks && at_shot(SH_TRICK_DONE)) {
            at_lastTricks = g.trickCount;
            return 0;
        }
        at_lastTricks = g.trickCount;

        /* la informacion se abre con SELECT y se pasa de pagina con R: manos, amuletos, mazo */
        if (showInfo) {
            if (at_infoStep == 1) { if (at_shot(SH_INFO)) return 0; at_infoStep = 2; return at_press(BTN_R); }
            if (at_infoStep == 2) { if (at_shot(SH_INFO_ITEMS)) return 0; at_infoStep = 3; return at_press(BTN_R); }
            if (at_infoStep == 3) { if (at_shot(SH_INFO_DECK)) return 0; at_infoStep = 4; }
            return at_press(BTN_SELECT);
        }
        if (at_shot(g.bossRule != B_NONE ? SH_BOSS_PLAY : SH_HAND)) return 0;
        if (g.relicCount > 0) {
            if (at_shot(SH_HAND2)) return 0;
            if (at_infoStep == 0) {
                at_infoStep = 1;
                return at_press(BTN_SELECT);
            }
        }
        if (g.round == 1 && !at_pauseAsked) {
            at_pauseAsked = 1;
            return at_press(BTN_START);
        }

        /* gasta los trucos: elige las cartas mas bajas que pida y lo usa desde el menu */
        if (trickMode && (g.trickCount == 0 || at_trickTries >= 8)) return at_press(BTN_CIRCLE);
        if (g.trickCount > 0 && at_trickTries < 8) {
            const TrickInfo *info = &TRICK_INFO[g.tricks[0]];
            if (!trickMode) {
                int mask = 0;
                for (i = 0; i < info->minSel && i < g.handCount; i++) mask |= 1 << (g.handCount - 1 - i);
                b = at_select_mask(mask);
                if (b) return b;
                return at_press(BTN_R);
            }
            if (at_shot(SH_TRICK)) return 0;
            at_trickTries++;
            at_plan = 0;
            return at_press(BTN_CROSS);
        }

        if (!at_plan) {
            at_make_plan();
            at_plan = 1;
        }
        b = at_select_mask(at_planMask);
        if (b) return b;
        if (!at_planDiscard && at_shot(SH_SELECT)) return 0;
        at_plan = 0;
        return at_press(at_planDiscard ? BTN_TRIANGLE : BTN_SQUARE);

    case ST_WIN:
        if (at_frames < 85) return 0;
        if (at_shot(SH_REWARD)) return 0;
        return at_press(BTN_CROSS);

    case ST_SHOP:
        if (at_frames < 30) return 0;
#if AUTOTEST == 3
        at_resumeOk = (g.relicCount >= 0 && g.money >= 0);
        if (at_shot(SH_RESUME)) return 0;
        at_done = 1;
        return 0;
#else
        if (showInfo) {
            if (at_shot(SH_INFO_SHOP)) return 0;
            return at_press(BTN_SELECT);
        }
        if (at_shot(SH_SHOP)) return 0;
#if AUTOTEST == 2
        at_done = 1;
        return 0;
#endif
        for (i = 0; i < SHOP_OFFERS; i++) {
            const Offer *o = &g.shop[i];
            if (o->sold || g.money < o->price) continue;
            if (o->kind == OF_RELIC && g.relicCount >= MAX_RELICS) continue;
            if (o->kind == OF_TRICK && g.trickCount >= game_pockets(&g)) continue;
            if (o->kind == OF_CARD && g.deckCount >= MAX_DECK) continue;
            if (shopSel < i) return at_press(BTN_DOWN);
            if (shopSel > i) return at_press(BTN_UP);
            return at_press(BTN_CROSS);
        }
        if (at_shot(SH_SHOP2)) return 0;
        if (g.relicCount > 0 && !at_infoShop) {
            at_infoShop = 1;
            return at_press(BTN_SELECT);
        }
        /* una vez, vende un truco propio con Cuadrado para probar esa fila */
        if (g.trickCount > 1 && !at_soldTrick) {
            int row = SHOP_OFFERS + 1 + g.relicCount;
            if (shopSel < row) return at_press(BTN_DOWN);
            if (shopSel > row) return at_press(BTN_UP);
            if (at_shot(SH_SHOP_TRICK)) return 0;
            at_soldTrick = 1;
            return at_press(BTN_SQUARE);
        }
        n = SHOP_OFFERS + 1 + g.relicCount + g.trickCount + 1;
        if (shopSel < n - 1) return at_press(BTN_DOWN);
        return at_press(BTN_CROSS);
#endif

    case ST_VICTORY:
        if (at_frames < 70) return 0;
        if (at_shot(SH_VICTORY)) return 0;
        return at_press(BTN_CROSS);      /* "Seguir en modo infinito" */

    default: /* derrota */
        if (at_frames < 45) return 0;
        if (at_shot(SH_END)) return 0;
        at_done = 1;
        return 0;
    }
}

const char *ui_autotest_shot(void)
{
    const char *s = at_pending;
    at_pending = NULL;
    return s;
}

int ui_autotest_done(void)
{
    return at_done;
}

int ui_autotest_summary(char *b, int n)
{
    int peak;
    unsigned int nonzero;
    synth_stats(&peak, &nonzero);
    return snprintf(b, n, "modo=%d estado=%d mesa=%d ronda=%d infinito=%d dinero=%d amuletos=%d trucos=%d mazo=%d mejor=%d "
                    "frames=%d audio_pico=%d guardada=%d descubiertos=%d+%d partidas=%u victorias=%u dificultad_max=%d "
                    "continuar=%d guardado_oficial=%d\n",
                    AUTOTEST, state, g.mesa + 1, g.round + 1, g.endless, g.money, g.relicCount, g.trickCount,
                    game_deck_alive(&g), (int)g.bestHand, frame, peak, save_has_run(), save_found_count(),
                    save_trick_found_count(), save_profile()->runs, save_profile()->wins,
                    save_profile()->maxDifficulty, at_resumeOk, savedata_official());
}
