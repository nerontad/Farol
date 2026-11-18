/* Prueba del sintetizador en PC: comprueba que suena, que no satura y deja un WAV para escucharlo.
 *   gcc -O2 -Wall -Isrc tests/test_synth.c src/synth.c -o test_synth && ./test_synth salida.wav
 *
 * El WAV recorre las cuatro piezas de musica: titulo (0-3 s), partida con un recuento que sube
 * de tono (3-6 s), el momento mas ruidoso (6-8 s), rival (8-10,5 s) y tienda (10,5-13 s).
 * Despues comprueba que no suena nada con la musica apagada o con su volumen a 0.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "synth.h"

#define SECONDS 17
#define BLOCK   1024

static short pcm[SYNTH_RATE * SECONDS * 2];

static void put32(FILE *f, unsigned int v) { fputc(v & 255, f); fputc((v >> 8) & 255, f); fputc((v >> 16) & 255, f); fputc(v >> 24, f); }
static void put16(FILE *f, unsigned int v) { fputc(v & 255, f); fputc((v >> 8) & 255, f); }

/* Analiza un tramo: pico, volumen medio (RMS) y muestras pegadas al tope */
static void analyse(const char *name, double from, double to, int *failures, int wantSound)
{
    double sum = 0, rms = 0;
    int a = (int)(from * SYNTH_RATE), b = (int)(to * SYNTH_RATE);
    int peak = 0, clipped = 0, i;
    for (i = a * 2; i < b * 2; i += 2) {
        int v = pcm[i] < 0 ? -pcm[i] : pcm[i];
        if (v > peak) peak = v;
        if (v >= 29990) clipped++;
        sum += (double)pcm[i] * pcm[i];
    }
    if (sum > 0) {
        double x = sum / (b - a), r = x;
        int k;
        for (k = 0; k < 40; k++) r = 0.5 * (r + x / r);
        rms = r;
    }
    printf("  %-30s pico %5d (%3d%%)  rms %6.0f  saturadas %d\n", name, peak, peak * 100 / 30000, rms, clipped);
    if (wantSound && peak < 300) { printf("    FALLO: silencio\n"); (*failures)++; }
    if (!wantSound && peak > 40) { printf("    FALLO: deberia haber silencio\n"); (*failures)++; }
    if (clipped > (b - a) / 200) { printf("    FALLO: satura\n"); (*failures)++; }
}

int main(int argc, char **argv)
{
    static const int PENT[5] = { 0, 2, 4, 7, 9 };
    int failures = 0, pos = 0, block = 0, i;
    int total = SYNTH_RATE * SECONDS;

    synth_init();
    synth_set_volume(7, 10);
    while (pos < total) {
        int n = total - pos < BLOCK ? total - pos : BLOCK;
        double t = (double)pos / SYNTH_RATE;

        if (t < 3.0) {
            synth_set_track(TRACK_TITLE);
        } else if (t < 6.0) {
            /* partida: recuento que sube de tono, con la musica animada */
            synth_set_track(TRACK_PLAY);
            synth_set_intensity(1.0f);
            if (block % 6 == 0) {
                int k = (int)((t - 3.0) * 4.0), semis = 12 * (k / 5) + PENT[k % 5];
                float f = 330.0f;
                for (i = 0; i < semis && i < 36; i++) f *= 1.0594631f;
                synth_note(W_TRI, f, 0.15f, 0.18f, 0.0f);
                synth_note(W_SINE, f * 2.0f, 0.05f, 0.12f, 0.0f);
            }
        } else if (t < 8.0) {
            /* el peor caso: todo a la vez */
            if (block % 8 == 0) {
                synth_note(W_SQUARE, 1320.0f, 0.10f, 0.30f, 0.0f);
                synth_note(W_SQUARE, 1977.0f, 0.08f, 0.30f, 0.0f);
                synth_note(W_TRI, 2640.0f, 0.10f, 0.35f, 0.0f);
                synth_note(W_NOISE, 3000.0f, 0.08f, 0.12f, -40.0f);
                synth_note(W_TRI, 392.0f, 0.13f, 0.25f, 0.0f);
                synth_note(W_TRI, 523.0f, 0.14f, 0.30f, 0.0f);
                synth_note(W_SINE, 1568.0f, 0.12f, 0.12f, 0.0f);
            }
        } else if (t < 10.5) {
            synth_set_intensity(0.0f);
            synth_set_track(TRACK_BOSS);
        } else if (t < 13.0) {
            synth_set_track(TRACK_SHOP);
        } else if (t < 15.0) {
            synth_set_music(0);
        } else {
            /* musica encendida pero con su volumen a 0 */
            synth_set_music(1);
            synth_set_volume(0, 10);
        }
        synth_render(pcm + pos * 2, n);
        pos += n;
        block++;
    }

    printf("sintetizador, %d s a %d Hz:\n", SECONDS, SYNTH_RATE);
    analyse("titulo (0-3 s)", 0.0, 3.0, &failures, 1);
    analyse("partida + recuento (3-6 s)", 3.0, 6.0, &failures, 1);
    analyse("peor caso (6-8 s)", 6.0, 8.0, &failures, 1);
    analyse("rival (8,5-10,5 s)", 8.5, 10.5, &failures, 1);
    analyse("tienda (11-13 s)", 11.0, 13.0, &failures, 1);
    analyse("musica apagada (13,5-15 s)", 13.5, 15.0, &failures, 0);
    analyse("volumen de musica a 0 (15,5-17)", 15.5, 17.0, &failures, 0);
    for (i = 0; i < total * 2; i += 2)
        if (pcm[i] != pcm[i + 1]) { printf("FALLO: canales distintos\n"); failures++; break; }

    if (argc > 1) {
        /* el WAV solo lleva la parte que suena: los 13 primeros segundos */
        FILE *f = fopen(argv[1], "wb");
        if (f) {
            unsigned int bytes = (unsigned int)(SYNTH_RATE * 13) * 4;
            fwrite("RIFF", 1, 4, f); put32(f, 36 + bytes); fwrite("WAVEfmt ", 1, 8, f);
            put32(f, 16); put16(f, 1); put16(f, 2); put32(f, SYNTH_RATE); put32(f, SYNTH_RATE * 4);
            put16(f, 4); put16(f, 16); fwrite("data", 1, 4, f); put32(f, bytes);
            fwrite(pcm, 1, bytes, f);
            fclose(f);
            printf("WAV escrito en %s\n", argv[1]);
        }
    }
    printf("%s (%d fallos)\n", failures ? "HAY FALLOS" : "TODO BIEN", failures);
    return failures ? 1 : 0;
}
