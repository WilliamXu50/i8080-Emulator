#ifndef machine_H
#define machine_H

#include "i8080_cpu.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define SCREEN_WIDTH 			224
#define SCREEN_HEIGHT			256
#define FPS				60		//60 frames/sec
#define CLOCK_RATE			2000000			//2MHz
#define CYCLES_PER_FRAME		CLOCK_RATE / FPS	//2x10^6 cpu per second.
#define HALF_CYCLES_PER_FRAME	 	CYCLES_PER_FRAME / 2		//Used to trigger interrupts

enum colors{R, G, B};

typedef struct{
    i8080* cpu;     //Pointer to i8080 cpu
    uint8_t port_in1, port_in2;
    uint8_t shift0, shift1, shift_offset;

    uint8_t screen_buffer[SCREEN_HEIGHT][SCREEN_WIDTH][3];
    uint8_t* machine_mem;	//Pointer to allocated memory

    uint8_t int_num;

    int quit_status;
} machine_t;

machine_t* init_machine();

void destroy_machine(machine_t* machine);

void load_file_into_mem(machine_t* machine, char* file_path, uint16_t offset);

void load_game(machine_t* machine);

void machine_execute(machine_t* machine);

void machine_update_screen(machine_t* machine);

#endif
