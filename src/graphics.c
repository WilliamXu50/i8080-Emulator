#include "graphics.h"

void init_SDL(display_t* display){
    SDL_Init(SDL_INIT_EVERYTHING);

    display->window=SDL_CreateWindow("Space Invaders", SDL_WINDOWPOS_UNDEFINED,
		                                 SDL_WINDOWPOS_UNDEFINED, WINDOW_WIDTH,
		                                 WINDOW_HEIGHT, SDL_WINDOW_RESIZABLE);

    if(!display->window){
        printf("Cannot create a window\n");
        exit(1);
    }

    display->renderer=SDL_CreateRenderer(display->window, -1,
                          SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);

    if(!display->renderer){
        printf("Cannot create renderer\n");
        exit(1);
    }

    display->texture=SDL_CreateTexture(display->renderer, SDL_PIXELFORMAT_RGB24,
		                  SDL_TEXTUREACCESS_STREAMING, SCREEN_WIDTH,
		                  SCREEN_HEIGHT);

    if(!display->texture){
        printf("Cannot create texture\n");
        exit(1);
    }
}

void destroy_SDL(display_t* display){
    SDL_DestroyTexture(display->texture);
    SDL_DestroyRenderer(display->renderer);
    SDL_DestroyWindow(display->window);
    SDL_Quit();
    free(display);
}

void render_graphics(display_t* display, machine_t* machine){
    uint32_t pitch=sizeof(uint8_t) * 3 * SCREEN_WIDTH;
    SDL_UpdateTexture(display->texture, NULL, &machine->screen_buffer, pitch);

    SDL_RenderClear(display->renderer);
    SDL_RenderCopy(display->renderer, display->texture, NULL, NULL);
    SDL_RenderPresent(display->renderer);
}
