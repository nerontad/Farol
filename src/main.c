/* FAROL - un roguelike de poker para PSP */
#include <pspkernel.h>
#include <pspdisplay.h>
#include <pspiofilemgr.h>
#include "gfx.h"
#include "input.h"
#include "ui.h"
#include "audio.h"
#include "savedata.h"

PSP_MODULE_INFO("FAROL", 0, 1, 0);
PSP_MAIN_THREAD_ATTR(PSP_THREAD_ATTR_USER | PSP_THREAD_ATTR_VFPU);
PSP_HEAP_SIZE_KB(4096);

static volatile int running = 1;

static int exit_callback(int arg1, int arg2, void *common)
{
    (void)arg1; (void)arg2; (void)common;
    running = 0;
    return 0;
}

static int callback_thread(SceSize args, void *argp)
{
    int cbid;
    (void)args; (void)argp;
    cbid = sceKernelCreateCallback("Exit Callback", exit_callback, NULL);
    sceKernelRegisterExitCallback(cbid);
    sceKernelSleepThreadCB();
    return 0;
}

static void setup_callbacks(void)
{
    int thid = sceKernelCreateThread("update_thread", callback_thread, 0x11, 0xFA0, 0, 0);
    if (thid >= 0) sceKernelStartThread(thid, 0, 0);
}

unsigned int plat_ticks(void)
{
    return sceKernelGetSystemTimeLow();
}

int main(void)
{
    setup_callbacks();
    gfx_init();
    input_init();
    audio_init();
    savedata_init();
    ui_init();

    while (running) {
        input_update();
        ui_update();
        gfx_begin(RGB(16, 8, 14));
        ui_draw();
        gfx_end();
#ifdef AUTOTEST
        {
            const char *shot = ui_autotest_shot();
            if (shot) gfx_dump(shot);
            if (ui_autotest_done()) {
                char b[400];
                int n;
                SceUID fd;
                savedata_flush();       /* el resumen dice si el guardado oficial funciono */
                n = ui_autotest_summary(b, sizeof(b));
                if (n > (int)sizeof(b) - 1) n = (int)sizeof(b) - 1;
                fd = sceIoOpen("ms0:/farol_done.txt", PSP_O_WRONLY | PSP_O_CREAT | PSP_O_TRUNC, 0777);
                if (fd >= 0) {
                    sceIoWrite(fd, b, n);
                    sceIoClose(fd);
                }
                running = 0;
            }
        }
#endif
    }

    savedata_flush();
    audio_shutdown();
    sceKernelExitGame();
    return 0;
}
