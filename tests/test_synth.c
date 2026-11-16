/* Prueba del sintetizador en PC: comprueba que suena, que no satura y deja un WAV para escucharlo.
 *   gcc -O2 -Wall -Isrc tests/test_synth.c src/synth.c -o test_synth && ./test_synth salida.wav
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "synth.h"

#define SECONDS 14
#define BLOCK   1024

static short pcm[SYNTH_RATE * SECONDS * 2];

static void put32(FILE *f, unsigned int v) { fputc(v & 255, f); fputc((v >> 8) & 255, f); fputc((v >> 16) & 255, f); fputc(v >> 24, f); }
static void put16(FILE *f, unsigned int v) { fputc(v & 255, f); fputc((v >> 8) & 255, f); }

/* Analiza un tramo: pico, volumen medio (RMS) y muestras pegadas al tope */
static void analyse(const char *name, int from, int to, int *failures, int wantSound)
{
    double sum = 0;
    int peak = 0, clipped = 0, i;
    for (i = from * 2; i < to * 2; i += 2) {
        int v = pcm[i] < 0 ? -pcm[i] : pcm[i];
        if (v > peak) peak = v;
        if (v >= 29990) clipped++;
        sum += (double)pcm[i] * pcm[i];
    }
    {
        double rms = 0, x = sum / (to - from), r = x > 0 ? x : 1;
        int k;
        for (k = 0; k < 40; k++) r = 0.5 * (r + x / r);
        rms = x > 0 ? r : 0;
        printf("  %-28s pico %5d (%3d%%)  rms %6.0f  saturadas %d\n", name, peak, peak * 100 / 30000, rms, clipped);
        if (wantSound && peak < 300) { printf("    FALLO: silencio\n"); (*failures)++; }
        if (clipped > (to - from) / 200) { printf("    FALLO: satura\n"); (*failures)++; }
    }
}

int main(int argc, char **argv)
{
    int failures = 0, pos = 0, block = 0, i;
    int total = SYNTH_RATE * SECONDS;
    static const int PENT[5] = { 0, 2, 4, 7, 9 };

    synth_init();
    while (pos < total) {
        int n = total - pos < BLOCK ? total - pos : BLOCK;
        double t = (double)pos / SYNTH_RATE;

        /* 0-4 s: solo musica. 4-8 s: recuento que sube de tono, con la musica animada.
           8-10 s: el peor caso (todo a la vez). 10-14 s: musica apagada, debe quedar en silencio. */
        if (t >= 4.0 && t < 8.0) {
            synth_set_intensity(1.0f);
            if (block % 6 == 0) {
                int k = (int)((t - 4.0) * 4.0), semis = 12 * (k / 5) + PENT[k % 5];
                float f = 330.0f;
                for (i = 0; i < semis && i < 36; i++) f *= 1.0594631f;
                synth_note(W_TRI, f, 0.15f, 0.18f, 0.0f);
                synth_note(W_SINE, f * 2.0f, 0.05f, 0.12f, 0.0f);
            }
        }
        if (t >= 8.0 && t < 10.0 && block % 8 == 0) {
            synth_note(W_SQUARE, 1320.0f, 0.10f, 0.30f, 0.0f);
            synth_note(W_SQUARE, 1977.0f, 0.08f, 0.30f, 0.0f);
            synth_note(W_TRI, 2640.0f, 0.10f, 0.35f, 0.0f);
            synth_note(W_NOISE, 3000.0f, 0.08f, 0.12f, -40.0f);
            synth_note(W_TRI, 392.0f, 0.13f, 0.25f, 0.0f);
            synth_note(W_TRI, 523.0f, 0.14f, 0.30f, 0.0f);
            synth_note(W_SINE, 1568.0f, 0.12f, 0.12f, 0.0f);
        }
        if (t >= 10.0) {
            synth_set_music(0);
            synth_set_intensity(0.0f);
        }
        synth_render(pcm + pos * 2, n);
        pos += n;
        block++;
    }

    printf("sintetizador, %d s a %d Hz:\n", SECONDS, SYNTH_RATE);
    analyse("musica sola (0-4 s)", 0, 4 * SYNTH_RATE, &failures, 1);
    analyse("recuento + musica (4-8 s)", 4 * SYNTH_RATE, 8 * SYNTH_RATE, &failures, 1);
    analyse("peor caso (8-10 s)", 8 * SYNTH_RATE, 10 * SYNTH_RATE, &failures, 1);
    analyse("musica apagada (12-14 s)", 12 * SYNTH_RATE, 14 * SYNTH_RATE, &failures, 0);
    {
        int peak = 0;
        for (i = 12 * SYNTH_RATE * 2; i < 14 * SYNTH_RATE * 2; i += 2) {
            int v = pcm[i] < 0 ? -pcm[i] : pcm[i];
            if (v > peak) peak = v;
        }
        if (peak > 40) { printf("    FALLO: deberia haber silencio con la musica apagada\n"); failures++; }
    }
    for (i = 0; i < total * 2; i += 2)
        if (pcm[i] != pcm[i + 1]) { printf("FALLO: canales distintos\n"); failures++; break; }

    if (argc > 1) {
        FILE *f = fopen(argv[1], "wb");
        if (f) {
            unsigned int bytes = (unsigned int)total * 4;
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
