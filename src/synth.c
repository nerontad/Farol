/* FAROL - sintetizador de efectos y musica (independiente de la plataforma) */
#include <string.h>
#include "synth.h"

#define VOICES 16
#define QUEUE  32
#define CHUNK  256
#define STEP_SAMPLES (SYNTH_RATE * 60 / 100 / 4)   /* semicorchea a 100 pulsos por minuto */

typedef struct {
    int wave, att;
    float phase, inc, incMul, amp, ampMul, last;
} Voice;

typedef struct { int wave; float freq, vol, secs, slide; } Note;

static Voice voices[VOICES];
static Note queue[QUEUE];
static volatile unsigned int qHead, qTail;
static volatile int musicOn = 1;
static volatile int intensity = 0;      /* 0..100 */
static float midiFreq[128];
static unsigned int noiseSeed = 22222u;
static int seqPos, seqLeft;
static int statPeak;
static unsigned int statNonZero;

/* progresion La menor - Fa - Do - Sol */
static const signed char CH_BASS[4] = { 45, 41, 48, 43 };
static const signed char CH_TONES[4][4] = {
    { 57, 60, 64, 69 }, { 53, 57, 60, 65 }, { 60, 64, 67, 72 }, { 55, 59, 62, 67 }
};
static const signed char ARP[8] = { 0, 2, 1, 2, 3, 2, 1, 2 };

void synth_init(void)
{
    int i;
    memset(voices, 0, sizeof(voices));
    qHead = qTail = 0;
    seqPos = 0;
    seqLeft = 0;
    statPeak = 0;
    statNonZero = 0;
    /* La4 (nota 69) = 440 Hz; el resto, multiplicando por la raiz doceava de 2 */
    midiFreq[69] = 440.0f;
    for (i = 70; i < 128; i++) midiFreq[i] = midiFreq[i - 1] * 1.0594631f;
    for (i = 68; i >= 0; i--) midiFreq[i] = midiFreq[i + 1] / 1.0594631f;
}

static void note_on(int wave, float freq, float vol, float secs, float slide)
{
    Voice *v = &voices[0];
    float n;
    int i;
    for (i = 1; i < VOICES; i++)
        if (voices[i].amp < v->amp) v = &voices[i];

    n = secs * SYNTH_RATE;
    if (n < 200.0f) n = 200.0f;
    v->wave = wave;
    v->att = 0;
    v->phase = 0.0f;
    v->inc = freq / SYNTH_RATE;
    v->incMul = 1.0f + slide * 0.0577623f / SYNTH_RATE;
    v->amp = vol;
    v->ampMul = 1.0f - 4.6f / n;      /* cae al 1% en `secs` segundos */
    v->last = 0.0f;
}

void synth_note(int wave, float freq, float vol, float secs, float slide)
{
    unsigned int h = qHead, next = (h + 1) % QUEUE;
    if (next == qTail) return;         /* cola llena: se pierde la nota */
    queue[h].wave = wave;
    queue[h].freq = freq;
    queue[h].vol = vol;
    queue[h].secs = secs;
    queue[h].slide = slide;
    qHead = next;
}

static void music_step(void)
{
    int bar = (seqPos / 16) % 4, st = seqPos % 16;
    int hot = intensity;

    if (st == 0 || st == 8) note_on(W_TRI, midiFreq[CH_BASS[bar]], 0.11f, 0.45f, 0.0f);
    if (st == 14) note_on(W_TRI, midiFreq[CH_BASS[bar]], 0.07f, 0.20f, 0.0f);
    if ((st & 1) == 0) note_on(W_SINE, midiFreq[CH_TONES[bar][ARP[st / 2]]], 0.055f, 0.30f, 0.0f);
    if (hot > 30 && (st & 3) == 2) note_on(W_NOISE, 9000.0f, 0.02f, 0.03f, 0.0f);
    if (hot > 60 && (st & 1) == 1)
        note_on(W_TRI, midiFreq[CH_TONES[bar][ARP[(st / 2 + 2) % 8]] + 12], 0.035f, 0.12f, 0.0f);
    seqPos++;
}

static void mix(float *buf, int n)
{
    int i, k;
    for (i = 0; i < n; i++) buf[i] = 0.0f;
    for (k = 0; k < VOICES; k++) {
        Voice *v = &voices[k];
        if (v->amp < 0.0005f) { v->amp = 0.0f; continue; }
        for (i = 0; i < n; i++) {
            float ph = v->phase, s, a = v->amp;
            switch (v->wave) {
            case W_SQUARE: s = ph < 0.5f ? 0.6f : -0.6f; break;
            case W_TRI:    s = ph < 0.5f ? 4.0f * ph - 1.0f : 3.0f - 4.0f * ph; break;
            case W_SINE: {
                float x = 2.0f * ph - 1.0f;
                s = 4.0f * x * (1.0f - (x < 0.0f ? -x : x));
                break;
            }
            default:       s = v->last; break;
            }
            ph += v->inc;
            if (ph >= 1.0f) {
                ph -= 1.0f;
                if (ph >= 1.0f) ph = 0.0f;
                if (v->wave == W_NOISE) {
                    noiseSeed = noiseSeed * 1664525u + 1013904223u;
                    v->last = (float)((noiseSeed >> 16) & 0x7FFF) / 16384.0f - 1.0f;
                }
            }
            v->phase = ph;
            v->inc *= v->incMul;
            if (v->att < 64) { a *= (float)v->att / 64.0f; v->att++; }
            v->amp *= v->ampMul;
            buf[i] += s * a;
        }
    }
}

void synth_render(short *out, int frames)
{
    static float buf[CHUNK];

    while (qTail != qHead) {
        Note *q = &queue[qTail];
        note_on(q->wave, q->freq, q->vol, q->secs, q->slide);
        qTail = (qTail + 1) % QUEUE;
    }

    while (frames > 0) {
        int n = frames < CHUNK ? frames : CHUNK, i;
        if (seqLeft <= 0) {
            if (musicOn) music_step();
            seqLeft = STEP_SAMPLES;
        }
        if (n > seqLeft) n = seqLeft;
        mix(buf, n);
        for (i = 0; i < n; i++) {
            float s = buf[i] * 1.8f;   /* ganancia general: el peor caso medido llega al 80% */
            int v;
            if (s > 1.0f) s = 1.0f;
            if (s < -1.0f) s = -1.0f;
            v = (int)(s * 30000.0f);
            out[0] = out[1] = (short)v;
            out += 2;
            if (v < 0) v = -v;
            if (v > statPeak) statPeak = v;
            if (v) statNonZero++;
        }
        seqLeft -= n;
        frames -= n;
    }
}

void synth_set_music(int on) { musicOn = on; }
int  synth_get_music(void)   { return musicOn; }

void synth_set_intensity(float v)
{
    if (v < 0.0f) v = 0.0f;
    if (v > 1.0f) v = 1.0f;
    intensity = (int)(v * 100.0f);
}

void synth_stats(int *peak, unsigned int *nonzero)
{
    *peak = statPeak;
    *nonzero = statNonZero;
}
