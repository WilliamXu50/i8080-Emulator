#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <SDL2/SDL.h>
#include "machine.h"
#include "input.h"
#include "graphics.h"

int main(){
    display_t* game_display=malloc(sizeof(display_t));
    init_SDL(game_display);

    machine_t* machine=init_machine();

    //Load Space Invader ROM files into memory
    load_game(machine);

    int time=SDL_GetTicks();

    while(machine->quit_status!=1){
        //Every 17 ms a new frame updates (very roughly 60 fps)
        if((SDL_GetTicks() - time) > (1.0f / FPS) * 1000){
            //Update elapsed time
            time=SDL_GetTicks();
            
            //Get user input
            keyboard_handler(machine);

            int total_cycles=0;   //Total number of instruction cycles

            int current_cycle=0;    /*machine->cpu->instruction_cycles-current_cycle
                                      would give the CPU cycles after executing an instruction*/

            while(total_cycles<=HALF_CYCLES_PER_FRAME){
                current_cycle=machine->cpu->instruction_cycles;
                machine_execute(machine);
                total_cycles+=machine->cpu->instruction_cycles-current_cycle;
            }
            //Generate mid-screen interrupt (interrupt number = 1)
            generate_interrupt(machine, 1);

            current_cycle=0;

            while(total_cycles<=CYCLES_PER_FRAME){
                current_cycle=machine->cpu->instruction_cycles;
                machine_execute(machine);
                total_cycles+=machine->cpu->instruction_cycles-current_cycle;
            }
            //Generate end-of-screen interrupt (interrupt number = 2)
            generate_interrupt(machine, 2);
            
            machine_update_screen(machine);
            render_graphics(game_display, machine);
        }
    }

    destroy_SDL(game_display);
    destroy_machine(machine);
    printf("emulation finished\n");

    return 0;
}
