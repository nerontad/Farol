/* FAROL - sintetizador de efectos y musica (independiente de la plataforma) */
#include <string.h>
#include "synth.h"

#define VOICES 16
#define QUEUE  32
#define CHUNK  256

typedef struct {
    int wave, att;
    int bus;                            /* 0 = efectos, 1 = musica: cada uno con su volumen */
    float phase, inc, incMul, amp, ampMul, last;
} Voice;

typedef struct { int wave; float freq, vol, secs, slide; } Note;

static Voice voices[VOICES];
static Note queue[QUEUE];
static volatile unsigned int qHead, qTail;
static volatile int musicOn = 1;
static volatile int intensity = 0;      /* 0..100 */
static volatile int volume[2] = { 10, 7 };   /* efectos y musica, de 0 a 10 */
static float midiFreq[128];
static unsigned int noiseSeed = 22222u;
static int seqPos, seqLeft;
static int statPeak;
static unsigned int statNonZero;

/* Cada pieza son cuatro compases de 16 semicorcheas: bajo, arpegio del acorde y charles.
   Las mascaras dicen en que semicorcheas del compas suena cada instrumento. */
typedef struct {
    int bpm;
    signed char bass[4];            /* nota del bajo de cada compas (MIDI) */
    signed char tones[4][4];        /* notas del acorde de cada compas */
    signed char arp[8];             /* orden en que se tocan las notas del acorde */
    unsigned short bassMask, arpMask, hatMask;
    unsigned char arpWave;
    float bassVol, bassLen, arpVol, arpLen, hatVol;
} Track;

static const Track TRACKS[TRACK_COUNT] = {
    /* titulo: tranquila, acordes de septima (Rem7 - Sol7 - DoMaj7 - Lam7) */
    { 84, { 38, 43, 48, 45 },
      { { 62, 65, 69, 72 }, { 59, 62, 65, 67 }, { 60, 64, 67, 71 }, { 57, 60, 64, 67 } },
      { 0, 1, 2, 3, 2, 1, 2, 3 }, 0x0101, 0x5555, 0x0000, W_SINE,
      0.10f, 0.90f, 0.050f, 0.40f, 0.0f },
    /* partida: la de siempre (Lam - Fa - Do - Sol) */
    { 100, { 45, 41, 48, 43 },
      { { 57, 60, 64, 69 }, { 53, 57, 60, 65 }, { 60, 64, 67, 72 }, { 55, 59, 62, 67 } },
      { 0, 2, 1, 2, 3, 2, 1, 2 }, 0x4101, 0x5555, 0x0000, W_SINE,
      0.11f, 0.45f, 0.055f, 0.30f, 0.020f },
    /* rival: mas rapida y tensa, bajo constante (Mim - Do - Lam - Si) */
    { 120, { 40, 36, 45, 47 },
      { { 64, 67, 71, 76 }, { 60, 64, 67, 72 }, { 57, 60, 64, 69 }, { 59, 63, 66, 71 } },
      { 0, 2, 1, 3, 0, 2, 1, 3 }, 0x5555, 0x5555, 0xAAAA, W_TRI,
      0.13f, 0.16f, 0.052f, 0.18f, 0.014f },
    /* tienda: alegre y saltarina (Do - Lam - Rem - Sol) */
    { 112, { 48, 45, 50, 43 },
      { { 60, 64, 67, 72 }, { 57, 60, 64, 69 }, { 62, 65, 69, 74 }, { 55, 59, 62, 67 } },
      { 0, 1, 2, 1, 3, 1, 2, 1 }, 0x4141, 0x5555, 0x4444, W_TRI,
      0.13f, 0.24f, 0.058f, 0.20f, 0.016f },
};

static volatile int trackWant = TRACK_TITLE;
static int trackCur = -1;

void synth_init(void)
{
    int i;
    memset(voices, 0, sizeof(voices));
    qHead = qTail = 0;
    seqPos = 0;
    seqLeft = 0;
    trackCur = -1;
    trackWant = TRACK_TITLE;
    statPeak = 0;
    statNonZero = 0;
    /* La4 (nota 69) = 440 Hz; el resto, multiplicando por la raiz doceava de 2 */
    midiFreq[69] = 440.0f;
    for (i = 70; i < 128; i++) midiFreq[i] = midiFreq[i - 1] * 1.0594631f;
    for (i = 68; i >= 0; i--) midiFreq[i] = midiFreq[i + 1] / 1.0594631f;
}

static void note_bus(int wave, float freq, float vol, float secs, float slide, int bus);

/* las notas que lanza el secuenciador son de musica */
#define note_on(w, f, v, s, sl) note_bus(w, f, v, s, sl, 1)

static void note_bus(int wave, float freq, float vol, float secs, float slide, int bus)
{
    Voice *v = &voices[0];
    float n;
    int i;
    for (i = 1; i < VOICES; i++)
        if (voices[i].amp < v->amp) v = &voices[i];

    n = secs * SYNTH_RATE;
    if (n < 200.0f) n = 200.0f;
    v->wave = wave;
    v->bus = bus;
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

/* Una semicorchea de la pieza en curso */
static void music_step(void)
{
    const Track *t = &TRACKS[trackCur];
    int bar = (seqPos / 16) % 4, st = seqPos % 16;
    int hot = intensity;
    unsigned short bit = (unsigned short)(1u << st);

    if (t->bassMask & bit) {
        /* el bajo pesa mas en el primer y el tercer tiempo */
        float accent = (st % 8 == 0) ? 1.0f : 0.65f;
        note_on(W_TRI, midiFreq[t->bass[bar]], t->bassVol * accent, t->bassLen, 0.0f);
    }
    if (t->arpMask & bit) note_on(t->arpWave, midiFreq[t->tones[bar][t->arp[st / 2]]], t->arpVol, t->arpLen, 0.0f);
    if (t->hatVol > 0.0f && ((t->hatMask & bit) || (hot > 30 && (st & 3) == 2)))
        note_on(W_NOISE, 9000.0f, t->hatVol, 0.03f, 0.0f);
    /* cuando la partida se calienta, una segunda voz una octava mas arriba */
    if (hot > 60 && (st & 1) == 1)
        note_on(W_TRI, midiFreq[t->tones[bar][t->arp[(st / 2 + 2) % 8]] + 12], 0.035f, 0.12f, 0.0f);
    seqPos++;
}

static int step_samples(void)
{
    return SYNTH_RATE * 60 / TRACKS[trackCur].bpm / 4;
}

static void mix(float *buf, int n)
{
    int i, k;
    for (i = 0; i < n; i++) buf[i] = 0.0f;
    for (k = 0; k < VOICES; k++) {
        Voice *v = &voices[k];
        float gain = (float)volume[v->bus & 1] * 0.1f;
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
            buf[i] += s * a * gain;
        }
    }
}

void synth_render(short *out, int frames)
{
    static float buf[CHUNK];

    while (qTail != qHead) {
        Note *q = &queue[qTail];
        note_bus(q->wave, q->freq, q->vol, q->secs, q->slide, 0);   /* lo que pide el juego son efectos */
        qTail = (qTail + 1) % QUEUE;
    }

    while (frames > 0) {
        int n = frames < CHUNK ? frames : CHUNK, i;
        if (seqLeft <= 0) {
            /* el cambio de pieza se hace en una semicorchea y empieza desde su primer compas */
            if (trackWant != trackCur) {
                trackCur = trackWant;
                seqPos = 0;
            }
            if (musicOn) music_step();
            seqLeft = step_samples();
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

void synth_set_track(int track)
{
    if (track >= 0 && track < TRACK_COUNT) trackWant = track;
}

void synth_set_volume(int music, int sfx)
{
    volume[1] = music < 0 ? 0 : (music > 10 ? 10 : music);
    volume[0] = sfx < 0 ? 0 : (sfx > 10 ? 10 : sfx);
}

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
