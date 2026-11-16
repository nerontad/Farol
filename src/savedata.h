/* FAROL - almacenamiento en la PSP (datos guardados del sistema, con archivo de respaldo) */
#ifndef SAVEDATA_H
#define SAVEDATA_H

void savedata_init(void);       /* despues de gfx_init y antes de ui_init */
void savedata_flush(void);      /* espera a que termine de escribirse lo pendiente */
int  savedata_official(void);   /* 1 si se esta usando el guardado del sistema, 0 si el archivo suelto */

#endif
