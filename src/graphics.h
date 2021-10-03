#ifndef graphics_H
#define graphics_H

#include <SDL2/SDL.h>
#include "machine.h"

#define WINDOW_WIDTH 	SCREEN_WIDTH * 3
#define WINDOW_HEIGHT	SCREEN_HEIGHT * 3

typedef struct{
    SDL_Window* window;
    SDL_Renderer* renderer;
    SDL_Texture* texture;
} display_t;

void init_SDL(display_t* display);

void destroy_SDL(display_t* display);

void render_graphics(display_t* display, machine_t* machine);

#endif
