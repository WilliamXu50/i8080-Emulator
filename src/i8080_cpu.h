#ifndef i8080_emu_H
#define i8080_emu_H

#include <inttypes.h>
#include <string.h>

typedef struct {
    uint8_t S:1;    //Sign flag
    uint8_t Z:1;    //Zero flag
    uint8_t P:1;    //Polarity flag
    uint8_t C:1;    //Carry flag
    uint8_t AC:1;   //Auxiliary carry flag
} status_flags;

typedef struct{
    //Accumulator register A
    uint8_t A;

    //Other data registers:
    uint8_t B;
    uint8_t C;
    uint8_t D;
    uint8_t E;
    uint8_t H;
    uint8_t L;
    uint16_t SP;    //Stack Pointer

    uint16_t PC;    //Program Counter

    uint8_t *memory;      //Pointer to a memory space,

    status_flags flags;   //Status register flags

    int interrupt_enable;
    int instruction_cycles;

} i8080;

uint8_t read_mem(i8080* cpu, uint16_t addr);

//Emaulates one instruction and updates the program counter
void i8080_emulator(i8080* cpu);

//Initialize an i8080 cpu
i8080* i8080_init();

//Generates an interrupt with a specific interrupt number (int_num)
void RST(i8080* cpu, uint8_t int_num);

//Prints the value of i8080's registers, flags, PC and SP pointers
void print_values(i8080* cpu);

#endif
