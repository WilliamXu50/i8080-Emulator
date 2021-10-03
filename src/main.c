#include <stdio.h>
#include <stdlib.h>
#include <string.h>
//#include <pthread.h>

#include <SDL2/SDL.h>
#include "machine.h"
#include "input.h"
#include "graphics.h"

/*void* get_key_input(void* arg){
    machine_t* machine=(machine_t*)arg;

    while(machine->quit_status!=1){
        keyboard_handler(machine);
    }

    pthread_detach(pthread_self());
    return NULL;
}*/

int main(){
    display_t* game_display=malloc(sizeof(display_t));
    init_SDL(game_display);

    machine_t* machine=init_machine();

    //Load Space Invader Rom files into memory
    load_game(machine);

    int time=SDL_GetTicks();

    while(machine->quit_status!=1){
        //Get user input
        keyboard_handler(machine);

        //Every 17 ms (60 fps)
        if((SDL_GetTicks() - time) > (1.0f / FPS) * 1000){
            time=SDL_GetTicks();

            int cycles=0;   //Total number of instruction cycles

            while(cycles<=CYCLES_PER_FRAME){
                int current_cycle=machine->cpu->instruction_cycles;

                machine_execute(machine);

                cycles+=machine->cpu->instruction_cycles-current_cycle;

                if(machine->cpu->instruction_cycles>=HALF_CYCLES_PER_FRAME){
                    //Only generate interrupt if interrupt-enable is set
                    if(machine->cpu->interrupt_enable==1){
                        //Disable interrupt enable
                        machine->cpu->interrupt_enable=0;
                        //Generate the interrupt with the appropriate interrupt number
                        RST(machine->cpu, machine->int_num);
                        //11 cycles taken for interrupts
                        machine->cpu->instruction_cycles+=11;
                    }
                    machine->cpu->instruction_cycles-=HALF_CYCLES_PER_FRAME;

                    machine->int_num=(machine->int_num==1 ? 2 : 1);
                }
            }
            machine_update_screen(machine);
            render_graphics(game_display, machine);
        }
    }

    destroy_SDL(game_display);
    destroy_machine(machine);
    printf("emulation finished\n");

    return 0;
}
