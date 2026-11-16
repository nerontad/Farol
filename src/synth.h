/* FAROL - sintetizador de efectos y musica (independiente de la plataforma) */
#ifndef SYNTH_H
#define SYNTH_H

#define SYNTH_RATE 44100

enum { W_SQUARE, W_TRI, W_SINE, W_NOISE };

void synth_init(void);

/* Lanza una nota. Se puede llamar desde el hilo del juego.
   secs: lo que tarda en apagarse; slide: semitonos por segundo que sube (o baja) el tono. */
void synth_note(int wave, float freq, float vol, float secs, float slide);

/* Rellena `frames` muestras estereo de 16 bits. La llama el hilo de audio. */
void synth_render(short *out, int frames);

void synth_set_music(int on);
int  synth_get_music(void);
void synth_set_intensity(float v);   /* 0..1: la musica se anima cuando sube */

/* Para las pruebas: pico absoluto y numero de muestras no nulas generadas */
void synth_stats(int *peak, unsigned int *nonzero);

#endif
