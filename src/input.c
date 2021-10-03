#include "input.h"

void key_pressed(SDL_Keycode key, machine_t* machine){
    switch(key){
        case SDLK_q: machine->quit_status=1; break;
        case SDLK_c: machine->port_in1|=(1<<0); break;
        case SDLK_2: machine->port_in1|=(1<<1); break;
        case SDLK_RETURN: machine->port_in1=(1<<2); break;
        case SDLK_SPACE:
            machine->port_in1|=(1<<4);
            machine->port_in2|=(1<<4);
            break;
        case SDLK_LEFT:
            machine->port_in1|=(1<<5);
            machine->port_in2|=(1<<5);
            break;
        case SDLK_RIGHT:
            machine->port_in1|=(1<<6);
            machine->port_in2|=(1<<6);
            break;
    }
}

void key_released(SDL_Keycode key, machine_t* machine){
    switch(key){
        case SDLK_c: machine->port_in1 &=~(1<<0); break;
        case SDLK_2: machine->port_in1 &=~(1<<1); break;
        case SDLK_RETURN: machine->port_in1 &=~(1<<2); break;
        case SDLK_SPACE:
            machine->port_in1 &=~(1<<4);
            machine->port_in2 &=~(1<<4);
            break;
        case SDLK_LEFT:
            machine->port_in1 &=~(1<<5);
            machine->port_in2 &=~(1<<5);
            break;
        case SDLK_RIGHT:
            machine->port_in1 &=~(1<<6);
            machine->port_in2 &=~(1<<6);
            break;
    }
}

void keyboard_handler(machine_t* machine){
    SDL_Event event;

    while(SDL_PollEvent(&event)){
        switch(event.type){
          case SDL_QUIT: machine->quit_status=1; break;
          case SDL_KEYDOWN: key_pressed(event.key.keysym.sym, machine); break;
          case SDL_KEYUP:
                key_released(event.key.keysym.sym, machine);
                machine->port_in1|=(1<<3);
                break;
        }
    }
}
