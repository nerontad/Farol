/* FAROL - almacenamiento en la PSP.
 *
 * Usa la utilidad de datos guardados del sistema, asi la partida aparece en el menu de la
 * consola (Juego > Datos guardados) con su titulo e icono. Guarda en segundo plano: el juego
 * sigue dibujando mientras la utilidad escribe. Si la utilidad fallara, se usa un archivo
 * suelto en ms0:/PSP/COMMON como respaldo.
 */
#include <pspkernel.h>
#include <psputility.h>
#include <pspiofilemgr.h>
#include <string.h>
#include "gfx.h"
#include "save.h"
#include "savedata.h"

#define SD_GAME   "FARL00001"
#define SD_NAME   "0000"
#define SD_FILE   "DATA.BIN"
#define SD_BUF    8192
#define FILE_DIR  "ms0:/PSP/COMMON"
#define FILE_PATH "ms0:/PSP/COMMON/FAROL.SAV"

static SceUtilitySavedataParam param;
static unsigned char __attribute__((aligned(64))) dataBuf[SD_BUF];
static unsigned char pendingBuf[SD_BUF], lastBuf[SD_BUF];
static int pendingSize, lastSize;
static unsigned char icon[8192];
static int iconSize;
static char detail[256] = "FAROL";
static int busy, busyFrames, pending;
static int lastResult;
static int official = -1;      /* -1 sin probar, 1 funciona, 0 no funciona (se usa el archivo) */

/* --------------------------------------------------------- archivo suelto */

static int file_read(void *buf, int size)
{
    SceUID fd = sceIoOpen(FILE_PATH, PSP_O_RDONLY, 0777);
    int n;
    if (fd < 0) return 0;
    n = sceIoRead(fd, buf, size);
    sceIoClose(fd);
    return n;
}

static int file_write(const void *buf, int size)
{
    SceUID fd;
    int n;
    sceIoMkdir(FILE_DIR, 0777);      /* si ya existe, el error se ignora */
    fd = sceIoOpen(FILE_PATH, PSP_O_WRONLY | PSP_O_CREAT | PSP_O_TRUNC, 0777);
    if (fd < 0) return -1;
    n = sceIoWrite(fd, buf, size);
    sceIoClose(fd);
    return n;
}

/* ------------------------------------------------------- utilidad oficial */

static void fill_param(int mode, int size)
{
    memset(&param, 0, sizeof(param));
    param.base.size = sizeof(param);
    param.base.language = PSP_SYSTEMPARAM_LANGUAGE_SPANISH;
    param.base.buttonSwap = PSP_UTILITY_ACCEPT_CROSS;
    param.base.graphicsThread = 0x11;
    param.base.accessThread = 0x13;
    param.base.fontThread = 0x12;
    param.base.soundThread = 0x10;
    param.mode = mode;
    param.overwrite = 1;
    param.focus = PSP_UTILITY_SAVEDATA_FOCUS_LATEST;
    strcpy(param.gameName, SD_GAME);
    strcpy(param.saveName, SD_NAME);
    strcpy(param.fileName, SD_FILE);
    param.dataBuf = dataBuf;
    param.dataBufSize = SD_BUF;
    param.dataSize = (SceSize)size;

    if (mode == PSP_UTILITY_SAVEDATA_AUTOSAVE) {
        strcpy(param.sfoParam.title, "FAROL");
        strcpy(param.sfoParam.savedataTitle, "Partida y récords");
        strncpy(param.sfoParam.detail, detail, sizeof(param.sfoParam.detail) - 1);
        param.sfoParam.parentalLevel = 1;
        if (iconSize > 0) {
            param.icon0FileData.buf = icon;
            param.icon0FileData.bufSize = (SceSize)iconSize;
            param.icon0FileData.size = (SceSize)iconSize;
        }
    }
}

static int start(void)
{
    int r = sceUtilitySavedataInitStart(&param);
    if (r < 0) {
        lastResult = r;
        return 0;
    }
    busy = 1;
    busyFrames = 0;
    return 1;
}

static void start_pending_save(void)
{
    int size = (pendingSize + 15) & ~15;
    pending = 0;
    memcpy(lastBuf, pendingBuf, sizeof(lastBuf));
    lastSize = pendingSize;
    memset(dataBuf, 0, sizeof(dataBuf));
    memcpy(dataBuf, pendingBuf, (size_t)pendingSize);
    fill_param(PSP_UTILITY_SAVEDATA_AUTOSAVE, size);
    if (!start()) {
        official = 0;
        file_write(lastBuf, lastSize);
    }
}

/* Se llama una vez por fotograma, entre el final del dibujo y el cambio de buffer */
static void tick(void)
{
    if (!busy) {
        if (pending && official != 0) start_pending_save();
        return;
    }
    switch (sceUtilitySavedataGetStatus()) {
    case PSP_UTILITY_DIALOG_VISIBLE:
        sceUtilitySavedataUpdate(1);
        break;
    case PSP_UTILITY_DIALOG_QUIT:
        sceUtilitySavedataShutdownStart();
        break;
    case PSP_UTILITY_DIALOG_NONE:
        busy = 0;
        lastResult = param.base.result;
        break;
    default:
        break;
    }
    if (busy && ++busyFrames > 1500) {      /* nunca deberia pasar: no dejar el juego colgado */
        busy = 0;
        lastResult = -1;
    }
    if (!busy && param.mode == PSP_UTILITY_SAVEDATA_AUTOSAVE) {
        if (lastResult == 0) {
            official = 1;
        } else {
            official = 0;
            file_write(lastBuf, lastSize);
        }
    }
}

static void wait_idle(void)
{
    int guard = 0;
    while ((busy || (pending && official != 0)) && guard++ < 3000) {
        gfx_begin(RGB(16, 8, 14));
        gfx_end();
    }
}

/* ---------------------------------------------------------------- publico */

void savedata_init(void)
{
    /* el icono del guardado es el del propio juego, si se arranca desde el ISO */
    SceUID fd = sceIoOpen("disc0:/PSP_GAME/ICON0.PNG", PSP_O_RDONLY, 0777);
    iconSize = 0;
    if (fd >= 0) {
        int n = sceIoRead(fd, icon, sizeof(icon));
        sceIoClose(fd);
        if (n > 0 && n < (int)sizeof(icon)) iconSize = n;
    }
    gfx_set_frame_hook(tick);
}

void savedata_flush(void)
{
    wait_idle();
}

int savedata_official(void)
{
    return official;
}

void plat_storage_label(const char *text)
{
    strncpy(detail, text, sizeof(detail) - 1);
    detail[sizeof(detail) - 1] = 0;
}

int plat_storage_read(void *buf, int size)
{
#if defined(AUTOTEST) && AUTOTEST != 3
    (void)buf; (void)size;
    return 0;       /* estas autopruebas parten siempre de un perfil vacio */
#else
    if (size > SD_BUF) return 0;
    wait_idle();
    memset(dataBuf, 0, sizeof(dataBuf));
    fill_param(PSP_UTILITY_SAVEDATA_AUTOLOAD, 0);
    if (start()) {
        wait_idle();
        if (lastResult == 0) {
            official = 1;
            memcpy(buf, dataBuf, (size_t)size);
            return size;
        }
    }
    return file_read(buf, size);
#endif
}

int plat_storage_write(const void *buf, int size)
{
    if (size > SD_BUF) return -1;
    if (official == 0) return file_write(buf, size);
    memset(pendingBuf, 0, sizeof(pendingBuf));
    memcpy(pendingBuf, buf, (size_t)size);
    pendingSize = size;
    pending = 1;                /* lo recoge tick() en cuanto la utilidad este libre */
    return size;
}
