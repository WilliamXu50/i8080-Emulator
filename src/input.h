#ifndef input_H
#define input_H

#include <SDL2/SDL.h>
#include "machine.h"

void key_pressed(SDL_Keycode key, machine_t* machine);

void key_released(SDL_Keycode key, machine_t* machine);

void keyboard_handler(machine_t* machine);

#endif
