/* FAROL - entrada */
#ifndef INPUT_H
#define INPUT_H

enum {
    BTN_UP = 1, BTN_DOWN = 2, BTN_LEFT = 4, BTN_RIGHT = 8,
    BTN_CROSS = 16, BTN_CIRCLE = 32, BTN_SQUARE = 64, BTN_TRIANGLE = 128,
    BTN_L = 256, BTN_R = 512, BTN_START = 1024, BTN_SELECT = 2048
};

void input_init(void);
void input_update(void);
int  input_held(unsigned int b);
int  input_pressed(unsigned int b);
int  input_repeat(unsigned int b);   /* pulsado, con autorrepeticion en la cruceta */

#endif
