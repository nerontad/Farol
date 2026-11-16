/* FAROL - salida de audio en la PSP: conecta el sintetizador con pspaudiolib */
#include <stddef.h>
#include <pspaudiolib.h>
#include "synth.h"
#include "audio.h"

static void audio_callback(void *buf, unsigned int length, void *userdata)
{
    (void)userdata;
    synth_render((short *)buf, (int)length);
}

void audio_init(void)
{
    synth_init();
    pspAudioInit();
    pspAudioSetChannelCallback(0, audio_callback, NULL);
}

void audio_shutdown(void)
{
    pspAudioEndPre();
    pspAudioEnd();
}
