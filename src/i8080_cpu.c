#include <stdio.h>
#include <stdlib.h>
#include <inttypes.h>
#include <math.h>
#include <string.h>
#include <stdbool.h>
#include <unistd.h>

#include "i8080_cpu.h"
#include "disassembler.h"

//Table of CPU cycles for each i8080 instruction opcode
int get_instruction_cycles[] = {
    4,  10, 7,  5,  5,  5,  7,  4,  4,  10, 7,  5,  5,  5,  7,  4,
    4,  10, 7,  5,  5,  5,  7,  4,  4,  10, 7,  5,  5,  5,  7,  4,
    4,  10, 16, 5,  5,  5,  7,  4,  4,  10, 16, 5,  5,  5,  7,  4,
    4,  10, 13, 5,  10, 10, 10, 4,  4,  10, 13, 5,  5,  5,  7,  4,
    5,  5,  5,  5,  5,  5,  7,  5,  5,  5,  5,  5,  5,  5,  7,  5,
    5,  5,  5,  5,  5,  5,  7,  5,  5,  5,  5,  5,  5,  5,  7,  5,
    5,  5,  5,  5,  5,  5,  7,  5,  5,  5,  5,  5,  5,  5,  7,  5,
    7,  7,  7,  7,  7,  7,  7,  7,  5,  5,  5,  5,  5,  5,  7,  5,
    4,  4,  4,  4,  4,  4,  7,  4,  4,  4,  4,  4,  4,  4,  7,  4,
    4,  4,  4,  4,  4,  4,  7,  4,  4,  4,  4,  4,  4,  4,  7,  4,
    4,  4,  4,  4,  4,  4,  7,  4,  4,  4,  4,  4,  4,  4,  7,  4,
    4,  4,  4,  4,  4,  4,  7,  4,  4,  4,  4,  4,  4,  4,  7,  4,
    5,  10, 10, 10, 11, 11, 7,  11, 5,  10, 10, 10, 11, 17, 7,  11,
    5,  10, 10, 10, 11, 11, 7,  11, 5,  10, 10, 10, 11, 17, 7,  11,
    5,  10, 10, 18, 11, 11, 7,  11, 5,  5,  10, 5,  11, 17, 7,  11,
    5,  10, 10, 4,  11, 11, 7,  11, 5,  5,  10, 4,  11, 17, 7,  11
};

/******************************************************************************/

/*                           	Helper Functions                              */

/******************************************************************************/
//Checks the parity of an n-bit unsigned int
//Returns 1 if even parity, else returns 0
static inline uint8_t parity_check(uint8_t val, int NumOfBits){
    int count=0;
    int b=1;

    for(int i=0; i<NumOfBits; i++){
        if(val & (b<<i)){
            count++;
        }
    }

    return((count % 2)==0);
}

//Sets the Z, S & P status flags
static inline void set_ZSP(i8080* cpu, uint8_t result){
    cpu->flags.Z=(result==0);
    cpu->flags.S=((result & 0x80)==0x80);
    cpu->flags.P=parity_check(result, 8);
}

//Performs addition of 2 bytes (and carry) and set the appropriate status flags
static inline uint8_t add_bytes_set_flag(i8080* cpu, uint8_t val1, uint8_t val2, bool cy){
    uint16_t result=val1+val2+cy;

    set_ZSP(cpu, result & 0xFF);
    cpu->flags.C=(result & 0xFF00)>0;
    cpu->flags.AC=((val1 ^ result ^ val2) & 0x10)!=0;

    return (result & 0xFF);
}

//Performs subtraction of 2 bytes (and carry) and set the appropriate status flags
static inline uint8_t sub_bytes_set_flag(i8080* cpu, uint8_t val1, uint8_t val2, bool cy){
    uint16_t result=val1-val2-cy;

    set_ZSP(cpu, result & 0xFF);
    cpu->flags.C=(result & 0xFF00)>0;
    cpu->flags.AC=~((val1 ^ result ^ val2) & 0x10)!=0;

    return (result & 0xFF);
}

/*Concatenates 2 bytes into a single 16-bit value of the form byte1:byte2,
where byte1 is the higher byte and byte2 is the lower byte*/
static inline uint16_t byte_pair_concat(uint8_t byte1, uint8_t byte2){
    //Concatenate 2 bytes to form 1 word
    return (((uint16_t)byte1)<<8|byte2);
}

//Write a byte to memory location
static inline void write_mem(i8080* cpu, uint16_t addr, uint8_t data){
    cpu->memory[addr]=data;
}

//Read a byte from memory location and return it
uint8_t read_mem(i8080* cpu, uint16_t addr){
    return cpu->memory[addr];
}

/*Retrieves the 2-byte address from the operand of the instruction pointed to
by the program counter*/
static inline uint16_t get_immediate_addr(i8080* cpu, uint16_t pc){
    uint8_t byte1, byte2;
    //Read 2 bytes of the address immediate from memory
    byte1=read_mem(cpu, pc+1);
    byte2=read_mem(cpu, pc);

    //Concatenate the address bytes together to form a 16-bit address
    return byte_pair_concat(byte1, byte2);
}

/******************************************************************************/

/*                       16-Bit Load/Store/Move Operations                    */

/******************************************************************************/
static inline void PUSH(i8080* cpu, uint8_t reg1, uint8_t reg2){
    //Store the contents of reg1 and reg2 into the stack
    write_mem(cpu, (cpu->SP)-1, reg1);
    write_mem(cpu, (cpu->SP)-2, reg2);

    cpu->SP-=2;   //Update the stack pointer after pushing

    cpu->PC++;
}

static inline void POP(i8080* cpu, uint8_t* reg1, uint8_t* reg2){
    //Retrieve the contents of the stack, back to the designated registers
    *reg1=read_mem(cpu, (cpu->SP)+1);
    *reg2=read_mem(cpu, cpu->SP);

    //Update stack pointer after poping
    cpu->SP+=2;

    cpu->PC++;
}

static inline void PUSH_PSW(i8080* cpu){
    uint8_t saved_flags;
    saved_flags=(cpu->flags.C)|(0x02)|((cpu->flags.P)<<2)|
		((cpu->flags.AC<<4))|((cpu->flags.Z)<<6)|
		((cpu->flags.S)<<7);

    //Push flag register onto stack
    write_mem(cpu, (cpu->SP)-2, saved_flags);

    //Save the A register in the stack too
    write_mem(cpu, (cpu->SP)-1, cpu->A);

    cpu->SP-=2;   //Update stack pointer

    cpu->PC++;
}

static inline void POP_PSW(i8080* cpu){
    uint8_t PSW=read_mem(cpu, cpu->SP);

    //Pop the flags out of stack
    cpu->flags.C=((PSW & 0x01)!=0);
    cpu->flags.P=((PSW & 0x04)!=0);
    cpu->flags.AC=((PSW & 0x10)!=0);
    cpu->flags.Z=((PSW & 0x40)!=0);
    cpu->flags.S=((PSW & 0x80)!=0);

    cpu->A=read_mem(cpu, (cpu->SP)+1);

    cpu->SP+=2;		//Update the stack pointer

    cpu->PC++;
}

void XCHG(i8080* cpu){
    uint8_t D, E, H, L;

    D=cpu->D;
    E=cpu->E;
    H=cpu->H;
    L=cpu->L;

    cpu->D=H;
    cpu->E=L;
    cpu->H=D;
    cpu->L=E;

    cpu->PC++;
}

void XTHL(i8080* cpu){
    uint8_t L, H, SP, SP1;

    L=cpu->L;
    H=cpu->H;
    SP=read_mem(cpu, cpu->SP);
    SP1=read_mem(cpu, (cpu->SP)+1);

    cpu->L=SP;
    cpu->H=SP1;
    write_mem(cpu, cpu->SP, L);
    write_mem(cpu, (cpu->SP)+1, H);

    cpu->PC++;
}

void LXI(i8080* cpu, uint8_t *reg1, uint8_t* reg2, uint16_t pc){
    *reg1=read_mem(cpu, pc+1);
    *reg2=read_mem(cpu, pc);

    cpu->PC+=3;
}

void LHLD(i8080* cpu){
    uint16_t mem_addr=get_immediate_addr(cpu, (cpu->PC)+1);
    cpu->H=read_mem(cpu, mem_addr+1);
    cpu->L=read_mem(cpu, mem_addr);

    cpu->PC+=3;
}

void SHLD(i8080* cpu){
    uint16_t mem_addr=get_immediate_addr(cpu, (cpu->PC)+1);
    write_mem(cpu, mem_addr, cpu->L);
    write_mem(cpu, mem_addr+1, cpu->H);

    cpu->PC+=3;
}

/******************************************************************************/

/*                           Jump/Calls instructions                          */

/******************************************************************************/
static inline void JMP(i8080* cpu, uint16_t addr){
    //Set the program counter to the specifed address
    cpu->PC=addr;
}

static inline void CALL(i8080* cpu, uint16_t addr){
    //First store (PUSH) the return address into the stack
    /*The return address is simply the address of the next
    instruction in the memory after the CALL instruction. Thus the return
    address to the next instruction is PC+3 (+3 to skip the current instruction
    opcode and the 2 intermediate data bytes that makes up the 16-bit call address
    operand of the CALL instruction)*/
    uint16_t ret_addr;
    ret_addr=(cpu->PC)+3;

    PUSH(cpu, (ret_addr & 0xFF00)>>8, (ret_addr & 0x00FF));

    /*Then set the program counter to the call address (basically Jump)*/

    JMP(cpu, addr);
}

static inline void RET(i8080* cpu){
    //Read the return address from the stack
    uint8_t high_byte, low_byte;
    high_byte=read_mem(cpu, (cpu->SP)+1);
    low_byte=read_mem(cpu, cpu->SP);

    //Set the program counter to the return address
    cpu->PC=byte_pair_concat(high_byte, low_byte);

    //Update the stack pointer after popping
    cpu->SP+=2;
}

static inline void conditional_jmp(i8080* cpu, bool condition){
    if(condition){			//If conditional jump takes place
				uint16_t addr=get_immediate_addr(cpu, (cpu->PC)+1);
				JMP(cpu, addr);
    }
    else{
				cpu->PC+=3;
    }
}

static inline void conditional_call(i8080* cpu, bool condition){
    if(condition){		//If conditional call takes place
				uint16_t addr=get_immediate_addr(cpu, (cpu->PC)+1);
				cpu->instruction_cycles+=6;
				CALL(cpu, addr);
    }
    else{
				cpu->PC+=3;
    }
}

static inline void conditional_ret(i8080* cpu, bool condition){
    if(condition){		//If conditional return takes place
				cpu->instruction_cycles+=6;
				RET(cpu);
    }
    else{
				cpu->PC++;
    }
}

/******************************************************************************/

/*                        8-Bit Load/Store/Move Operations                    */

/******************************************************************************/
/*Retrieve data from memory address reg1:reg2 and store it in register A*/
static inline void LDAX(i8080* cpu, uint8_t reg1, uint8_t reg2){
    uint16_t addr;
    addr=byte_pair_concat(reg1, reg2);

    cpu->A=read_mem(cpu, addr);

    cpu->PC++;
}

static inline void LDA(i8080* cpu, uint16_t pc){
    uint16_t mem_addr=get_immediate_addr(cpu, pc);
    cpu->A=read_mem(cpu, mem_addr);

    cpu->PC+=3;
}

/*Store the contents of register A into memory address reg1:reg2*/
static inline void STAX(i8080* cpu, uint8_t reg1, uint8_t reg2){
    uint16_t addr;
    addr=byte_pair_concat(reg1, reg2);

    write_mem(cpu, addr, cpu->A);

    cpu->PC++;
}

static inline void MVI(i8080* cpu, uint8_t* dest, uint16_t pc){
    *dest=read_mem(cpu, pc);

    cpu->PC+=2;
}

static inline void STA(i8080* cpu, uint16_t pc){
    uint16_t mem_addr=get_immediate_addr(cpu, pc);
    write_mem(cpu, mem_addr, cpu->A);

    cpu->PC+=3;
}

//Generate an interrupt
void generate_interrupt(i8080* cpu, uint16_t addr){
    //First save the PC onto the stack
    write_mem(cpu, (cpu->SP)-1, cpu->PC >> 8);
    write_mem(cpu, (cpu->SP)-2, cpu->PC & 0xFF);
    cpu->SP-=2;

    cpu->PC=addr;
}

void RST(i8080* cpu, uint8_t int_num){
    switch(int_num){
				case 0: generate_interrupt(cpu, 0x0000); break;
				case 1: generate_interrupt(cpu, 0x0008); break;
				case 2: generate_interrupt(cpu, 0x0010); break;
				case 3: generate_interrupt(cpu, 0x0018); break;
				case 4: generate_interrupt(cpu, 0x0020); break;
				case 5: generate_interrupt(cpu, 0x0028); break;
				case 6: generate_interrupt(cpu, 0x0030); break;
				case 7: generate_interrupt(cpu, 0x0038); break;
    }
}

/******************************************************************************/

/*                      8-Bit Arithmetic/Logic Operations                     */

/******************************************************************************/
static inline void INR(i8080* cpu, uint8_t* reg){
    uint8_t result;
    result=(*reg)+1;

    set_ZSP(cpu, result);
    cpu->flags.AC=(result & 0x0F)==0x00;

    *reg=result;   //Store the new value back to the register

    cpu->PC++;
}

static inline void DCR(i8080* cpu, uint8_t* reg){
    uint8_t result;
    result=(*reg)-1;

    set_ZSP(cpu, result);
    cpu->flags.AC=!((result & 0x0F)==0x0F);

    *reg=result;

    cpu->PC++;
}

static inline void RLC(i8080* cpu){
    bool cy=(cpu->A & 0x80)!=0;

    cpu->A=(cpu->A <<1)|cy;

    cpu->flags.C=cy;

    cpu->PC++;
}

static inline void RAL(i8080* cpu){
    bool cy=(cpu->A & 0x80)!=0;

    cpu->A=(cpu->A <<1)|cpu->flags.C;

    cpu->flags.C=cy;

    cpu->PC++;
}

static inline void RRC(i8080* cpu){
    bool l_bit=(cpu->A & 0x01)!=0;

    cpu->A=(l_bit <<7)|(cpu->A >>1);

    cpu->flags.C=l_bit;

    cpu->PC++;
}

static inline void RAR(i8080* cpu){
    bool l_bit=(cpu->A & 0x01)!=0;

    cpu->A=(cpu->flags.C <<7)|(cpu->A >>1);

    cpu->flags.C=l_bit;

    cpu->PC++;
}

static inline void DAA(i8080* cpu){
    bool cy=cpu->flags.C;
    uint8_t addend=0;

    uint8_t lower_nibble;
    lower_nibble=(cpu->A) & 0x0F;

    if((lower_nibble>0x09)||(cpu->flags.AC==1)){
				addend+=0x06;
    }

    uint8_t higher_nibble;
    higher_nibble=(cpu->A)>>4;

    if((higher_nibble>0x09)||(cpu->flags.C==1)||(lower_nibble>0x09 && higher_nibble>=0x09)){
				addend+=0x60;
				cy=1;
    }

    cpu->A=add_bytes_set_flag(cpu, cpu->A, addend, 0);
    cpu->flags.C=cy;
    cpu->PC++;
}

static inline void ADD(i8080* cpu, uint8_t val, bool carry){
    cpu->A=add_bytes_set_flag(cpu, cpu->A, val, carry);

    cpu->PC++;
}

static inline void ADD_immediate(i8080* cpu, uint16_t pc, bool carry){
    uint8_t byte=read_mem(cpu, pc);

    cpu->A=add_bytes_set_flag(cpu, cpu->A, byte, carry);
    cpu->PC+=2;			//For the additional data byte
}

static inline void SUB(i8080* cpu, uint8_t val, uint8_t carry){
    cpu->A=sub_bytes_set_flag(cpu, cpu->A, val, carry);

    cpu->PC++;
}

static inline void SUB_immediate(i8080* cpu, uint16_t pc, uint8_t carry){
    uint8_t byte=read_mem(cpu, pc);

    cpu->A=sub_bytes_set_flag(cpu, cpu->A, byte, carry);
    cpu->PC+=2;			//For the additional data byte
}

static inline void ANA(i8080* cpu, uint8_t val){
    uint16_t result=(cpu->A) & val;

    set_ZSP(cpu, result);
    cpu->flags.C=0;
    cpu->flags.AC=(((cpu->A)|val) & 0x08)!=0;

    cpu->A=result & 0xFF;

    cpu->PC++;
}

static inline void ANI(i8080* cpu, uint16_t pc){
    uint8_t byte=read_mem(cpu, pc);
    ANA(cpu, byte);
    cpu->PC++;			//For the additional data byte
}

static inline void XRA(i8080* cpu, uint8_t val){
    uint16_t result=(cpu->A) ^ val;

    set_ZSP(cpu, result);
    cpu->flags.C=0;
    cpu->flags.AC=0;

    cpu->A=result & 0xFF;

    cpu->PC++;
}

static inline void XRI(i8080* cpu, uint16_t pc){
    uint8_t byte=read_mem(cpu, pc);
    XRA(cpu, byte);
    cpu->PC++;			//For the additional data byte
}

static inline void ORA(i8080* cpu, uint8_t val){
    uint16_t result=(cpu->A)|val;

    set_ZSP(cpu, result);
    cpu->flags.C=0;
    cpu->flags.AC=0;

    cpu->A=result & 0xFF;

    cpu->PC++;
}

static inline void ORI(i8080* cpu, uint16_t pc){
    uint8_t byte=read_mem(cpu, pc);
    ORA(cpu, byte);
    cpu->PC++;			//For the additional data byte
}

static inline void CMP(i8080* cpu, uint8_t val){
    sub_bytes_set_flag(cpu, cpu->A, val, 0);

    cpu->PC++;
}

static inline void CPI(i8080* cpu, uint16_t pc){
    uint8_t byte=read_mem(cpu, pc);
    CMP(cpu, byte);
    cpu->PC++;			//For the additional data byte
}

/******************************************************************************/

/*                     16-Bit Arithmetic/Logic Operations                     */

/******************************************************************************/
static inline void INX(i8080* cpu, uint8_t* higher_reg, uint8_t* lower_reg){
    (*lower_reg)++;

    if((*lower_reg)==0){
        (*higher_reg)++;
    }

    cpu->PC++;
}

static inline void DCX(i8080* cpu, uint8_t* higher_reg, uint8_t* lower_reg){
    (*lower_reg)--;

    if((*lower_reg)==0xFF){
        (*higher_reg)--;
    }

    cpu->PC++;
}

static inline void DAD(i8080* cpu, uint8_t reg1, uint8_t reg2){
    uint32_t large_reg;
    large_reg=(uint32_t)byte_pair_concat(reg1, reg2);

    uint32_t HL_pair;
    HL_pair=(uint32_t)byte_pair_concat(cpu->H, cpu->L);

    uint32_t result;
    result=HL_pair+large_reg;

    //Store the new values back into H and L registers
    cpu->H=((result & 0xFF00))>>8;
    cpu->L=result & 0xFF;

    //Set carry flag
    cpu->flags.C=((result & 0xFFFF0000)>0);

    cpu->PC++;
}

/*Executes one instruction from the memory (as pointed by the program counter)
and updates the program counter*/
void i8080_emulator(i8080* cpu){
    //Fetch the instruction opcode from the memory pointed to by the PC
    uint8_t opcode=read_mem(cpu, cpu->PC);

    #if DISASSEMBLY_ON_FLAG
        i8080_disassembler(cpu->memory, cpu->PC);
    #endif

    /*Converts H and L register pair into an 16-bit address (H:L)
    This address will be used by instructions using register-
    indirect addressing mode*/
    uint16_t HL_addr=byte_pair_concat(cpu->H, cpu->L);

    cpu->instruction_cycles+=get_instruction_cycles[opcode];

    uint16_t mem_addr;
    uint8_t byte1, byte2;

    //Execute instruction
    switch(opcode){
        //0x00 ... 0x0F
        case 0x00: cpu->PC++; break;			//NOP
        case 0x01: LXI(cpu, &cpu->B, &cpu->C, (cpu->PC)+1); break;			//LXI		B, d16
        case 0x02: STAX(cpu, cpu->B, cpu->C); break;			//STAX	B
        case 0x03: INX(cpu, &cpu->B, &cpu->C); break;			//INX		B
        case 0x04: INR(cpu, &cpu->B); break;			//INR		B
        case 0x05: DCR(cpu, &cpu->B); break;			//DCR		B
        case 0x06: MVI(cpu, &cpu->B, (cpu->PC)+1); break;			//MVI		B, d8
        case 0x07: RLC(cpu); break;			//RLC
        case 0x08: cpu->PC++; break;		//Undocumented opcode
        case 0x09: DAD(cpu, cpu->B, cpu->C); break;			//DAD		B
        case 0x0A: LDAX(cpu, cpu->B, cpu->C); break;			//LDAX	 B
        case 0x0B: DCX(cpu, &cpu->B, &cpu->C); break;			//DCX		B
        case 0x0C: INR(cpu, &cpu->C); break;				//INR		C
        case 0x0D: DCR(cpu, &cpu->C); break;				//DCR		C
        case 0x0E: MVI(cpu, &cpu->C, (cpu->PC)+1); break;			//MVI		C, d8
        case 0x0F: RRC(cpu); break;			//RRC

        //0x10 ... 0x1F
        case 0x10: cpu->PC++; break;		//Undocumented opcode
        case 0x11: LXI(cpu, &cpu->D, &cpu->E, (cpu->PC)+1); break;			//LXI		D, d16
        case 0x12: STAX(cpu, cpu->D, cpu->E); break;			//STAX	 D
        case 0x13: INX(cpu, &cpu->D, &cpu->E); break;			//INX		D
        case 0x14: INR(cpu, &cpu->D); break;			//INR		D
        case 0x15: DCR(cpu, &cpu->D); break;			//DCR		D
        case 0x16: MVI(cpu, &cpu->D, (cpu->PC)+1); break;			//MVI		D, d8
        case 0x17: RAL(cpu); break;			//RAL
        case 0x18: cpu->PC++; break;			//Undocumented opcode
        case 0x19: DAD(cpu, cpu->D, cpu->E); break;			//DAD		D
        case 0x1A: LDAX(cpu, cpu->D, cpu->E); break;			//LDAX		D
        case 0x1B: DCX(cpu, &cpu->D, &cpu->E); break;			//DCX		D
        case 0x1C: INR(cpu, &cpu->E); break;			//INR		E
        case 0x1D: DCR(cpu, &cpu->E); break;			//DCR		E
        case 0x1E: MVI(cpu, &cpu->E, (cpu->PC)+1); break;			//MVI		E, d8
        case 0x1F: RAR(cpu); break;			//RAR

        //0x20 ... 0x2F
        case 0x20: cpu->PC++; break;			//Undocumented opcode
        case 0x21: LXI(cpu, &cpu->H, &cpu->L,(cpu->PC)+1); break;			//LXI		H, d16
        case 0x22: SHLD(cpu); break;			//SHLD
        case 0x23: INX(cpu, &cpu->H, &cpu->L); break;			//INX		H
        case 0x24: INR(cpu, &cpu->H); break;			//INR		H
        case 0x25: DCR(cpu, &cpu->H); break;			//DCR		H
        case 0x26: MVI(cpu, &cpu->H, (cpu->PC)+1); break;			//MVI		H, d8
        case 0x27: DAA(cpu); break;			//DAA
        case 0x28: cpu->PC++; break;		//Undocumented opcode
        case 0x29: DAD(cpu, cpu->H, cpu->L); break;			//DAD		H
        case 0x2A: LHLD(cpu); break;			//LHLD
        case 0x2B: DCX(cpu, &cpu->H, &cpu->L); break;			//DCX		H
        case 0x2C: INR(cpu, &cpu->L); break;			//INR		L
        case 0x2D: DCR(cpu, &cpu->L); break;			//DCR		L
        case 0x2E: MVI(cpu, &cpu->L, (cpu->PC)+1); break;			//MVI		L, d8
        case 0x2F: cpu->A=~(cpu->A); cpu->PC++; break;			//CMA

        //0x30 ... 0x3F
        case 0x30: cpu->PC++; break;			//Undocumented opcode
        case 0x31:			//LXI		SP, d16
            byte1=read_mem(cpu, (cpu->PC)+2);
            byte2=read_mem(cpu, (cpu->PC)+1);
            cpu->SP=byte_pair_concat(byte1, byte2);
            cpu->PC+=3;
            break;
        case 0x32: STA(cpu, (cpu->PC)+1); break;			//STA		d16
        case 0x33: cpu->SP++; cpu->PC++; break;			//INX		SP
        case 0x34: INR(cpu, &cpu->memory[HL_addr]); break;			//INR		M
        case 0x35: DCR(cpu, &cpu->memory[HL_addr]); break;			//DCR		M
        case 0x36: MVI(cpu, &cpu->memory[HL_addr], (cpu->PC)+1); break;			//MVI		M, d8
        case 0x37: cpu->flags.C=1; cpu->PC++; break;			//STC
        case 0x38: cpu->PC++; break;			//Undocumented opcode
        case 0x39: DAD(cpu, (cpu->SP)>>8, (cpu->SP) & 0xFF); break;			//DAD		SP
        case 0x3A: LDA(cpu, (cpu->PC)+1); break;			//LDA		d16
        case 0x3B: cpu->SP--; cpu->PC++; break;			//DCX		SP
        case 0x3C: INR(cpu, &cpu->A); break;			//INR		A
        case 0x3D: DCR(cpu, &cpu->A); break;			//DCR		A
        case 0x3E: MVI(cpu, &cpu->A, (cpu->PC)+1); break;			//MVI		A, d8
        case 0x3F: cpu->flags.C=~(cpu->flags.C); cpu->PC++; break;			//CMC

				//0x40 ... 0x4F
				case 0x40: cpu->B=cpu->B; cpu->PC++; break;						//MOV		B, B
				case 0x41: cpu->B=cpu->C; cpu->PC++; break;						//MOV		B, C
				case 0x42: cpu->B=cpu->D; cpu->PC++; break;						//MOV		B, D
				case 0x43: cpu->B=cpu->E; cpu->PC++; break;						//MOV		B, E
				case 0x44: cpu->B=cpu->H; cpu->PC++; break;						//MOV		B, H
				case 0x45: cpu->B=cpu->L; cpu->PC++; break;						//MOV		B, L
				case 0x46: cpu->B=read_mem(cpu, HL_addr); cpu->PC++; break;		//MOV		B, M
				case 0x47: cpu->B=cpu->A; cpu->PC++; break;						//MOV		B, A
				case 0x48: cpu->C=cpu->B; cpu->PC++; break;						//MOV		C, B
				case 0x49: cpu->C=cpu->C; cpu->PC++; break;						//MOV		C, C
				case 0x4A: cpu->C=cpu->D; cpu->PC++; break;						//MOV		C, D
				case 0x4B: cpu->C=cpu->E; cpu->PC++; break;						//MOV		C, E
				case 0x4C: cpu->C=cpu->H; cpu->PC++; break;						//MOV		C, H
				case 0x4D: cpu->C=cpu->L; cpu->PC++; break;						//MOV		C, L
				case 0x4E: cpu->C=read_mem(cpu, HL_addr); cpu->PC++; break;		//MOV		C, M
				case 0x4F: cpu->C=cpu->A; cpu->PC++; break;						//MOV		C, A

				//0x50 ... 0x5F
				case 0x50: cpu->D=cpu->B; cpu->PC++; break;						//MOV		D, B
				case 0x51: cpu->D=cpu->C; cpu->PC++; break;						//MOV		D, C
				case 0x52: cpu->D=cpu->D; cpu->PC++; break;						//MOV		D, D
				case 0x53: cpu->D=cpu->E; cpu->PC++; break;						//MOV		D, E
				case 0x54: cpu->D=cpu->H; cpu->PC++; break;						//MOV		D, H
				case 0x55: cpu->D=cpu->L; cpu->PC++; break;						//MOV		D, L
				case 0x56: cpu->D=read_mem(cpu, HL_addr); cpu->PC++; break;		//MOV		D, M
				case 0x57: cpu->D=cpu->A; cpu->PC++; break;						//MOV		D, A
				case 0x58: cpu->E=cpu->B; cpu->PC++; break;						//MOV		E, B
				case 0x59: cpu->E=cpu->C; cpu->PC++; break;						//MOV		E, C
				case 0x5A: cpu->E=cpu->D; cpu->PC++; break;						//MOV		E, D
				case 0x5B: cpu->E=cpu->E; cpu->PC++; break;						//MOV		E, E
				case 0x5C: cpu->E=cpu->H; cpu->PC++; break;						//MOV		E, H
				case 0x5D: cpu->E=cpu->L; cpu->PC++; break;						//MOV		E, L
				case 0x5E: cpu->E=read_mem(cpu, HL_addr); cpu->PC++; break;		//MOV		E, M
				case 0x5F: cpu->E=cpu->A; cpu->PC++; break;						//MOV		E, A

				//0x60 ... 0x6F
				case 0x60: cpu->H=cpu->B; cpu->PC++; break;						//MOV		H, B
				case 0x61: cpu->H=cpu->C; cpu->PC++; break;						//MOV		H, C
				case 0x62: cpu->H=cpu->D; cpu->PC++; break;						//MOV		H, D
				case 0x63: cpu->H=cpu->E; cpu->PC++; break;						//MOV		H, E
				case 0x64: cpu->H=cpu->H; cpu->PC++; break;						//MOV		H, H
				case 0x65: cpu->H=cpu->L; cpu->PC++; break;						//MOV		H, L
				case 0x66: cpu->H=read_mem(cpu, HL_addr); cpu->PC++; break;		//MOV		H, M
				case 0x67: cpu->H=cpu->A; cpu->PC++; break;						//MOV		H, A
				case 0x68: cpu->L=cpu->B; cpu->PC++; break;						//MOV		L, B
				case 0x69: cpu->L=cpu->C; cpu->PC++; break;						//MOV		L, C
				case 0x6A: cpu->L=cpu->D; cpu->PC++; break;						//MOV		L, D
				case 0x6B: cpu->L=cpu->E; cpu->PC++; break;						//MOV		L, E
				case 0x6C: cpu->L=cpu->H; cpu->PC++; break;						//MOV		L, H
				case 0x6D: cpu->L=cpu->L; cpu->PC++; break;						//MOV		L, L
				case 0x6E: cpu->L=read_mem(cpu, HL_addr); cpu->PC++; break;		//MOV		L, M
				case 0x6F: cpu->L=cpu->A; cpu->PC++; break;						//MOV		L, A

				//0x70 ... 0x7F
				case 0x70: write_mem(cpu, HL_addr, cpu->B); cpu->PC++; break;		//MOV		M, B
				case 0x71: write_mem(cpu, HL_addr, cpu->C); cpu->PC++; break;		//MOV		M, C
				case 0x72: write_mem(cpu, HL_addr, cpu->D); cpu->PC++; break;		//MOV		M, D
				case 0x73: write_mem(cpu, HL_addr, cpu->E); cpu->PC++; break;		//MOV		M, E
				case 0x74: write_mem(cpu, HL_addr, cpu->H); cpu->PC++; break;		//MOV		M, H
				case 0x75: write_mem(cpu, HL_addr, cpu->L); cpu->PC++; break;		//MOV		M, L
				case 0x76: cpu->PC--; break;			//HLT
				case 0x77: write_mem(cpu, HL_addr, cpu->A); cpu->PC++; break;		//MOV		M, A
				case 0x78: cpu->A=cpu->B; cpu->PC++; break;						//MOV		A, B
				case 0x79: cpu->A=cpu->C; cpu->PC++; break;						//MOV		A, C
				case 0x7A: cpu->A=cpu->D; cpu->PC++; break;						//MOV		A, D
				case 0x7B: cpu->A=cpu->E; cpu->PC++; break;						//MOV		A, E
				case 0x7C: cpu->A=cpu->H; cpu->PC++; break;						//MOV		A, H
				case 0x7D: cpu->A=cpu->L; cpu->PC++; break;						//MOV		A, L
				case 0x7E: cpu->A=read_mem(cpu, HL_addr); cpu->PC++; break;		//MOV		A, M
				case 0x7F: cpu->A=cpu->A; cpu->PC++; break;						//MOV		A, A

				//0x80 ... 0x8F
				case 0x80: ADD(cpu, cpu->B, 0); break;			//ADD		B
				case 0x81: ADD(cpu, cpu->C, 0); break;			//ADD		C
				case 0x82: ADD(cpu, cpu->D, 0); break;			//ADD		D
				case 0x83: ADD(cpu, cpu->E, 0); break;			//ADD		E
				case 0x84: ADD(cpu, cpu->H, 0); break;			//ADD		H
				case 0x85: ADD(cpu, cpu->L, 0); break;			//ADD		L
				case 0x86: ADD(cpu, cpu->memory[HL_addr], 0); break;		//ADD		M
				case 0x87: ADD(cpu, cpu->A, 0); break;						//ADD		A
				case 0x88: ADD(cpu, cpu->B, cpu->flags.C); break;		//ADC		B
				case 0x89: ADD(cpu, cpu->C, cpu->flags.C); break;		//ADC		C
        case 0x8A: ADD(cpu, cpu->D, cpu->flags.C); break;		//ADC		D
				case 0x8B: ADD(cpu, cpu->E, cpu->flags.C); break;		//ADC		E
				case 0x8C: ADD(cpu, cpu->H, cpu->flags.C); break;		//ADC		H
				case 0x8D: ADD(cpu, cpu->L, cpu->flags.C); break;		//ADC		L
				case 0x8E: ADD(cpu, cpu->memory[HL_addr], cpu->flags.C); break;	//ADC		M
				case 0x8F: ADD(cpu, cpu->A, cpu->flags.C); break;		//ADC		A

				//0x90 ... 0x9F
				case 0x90: SUB(cpu, cpu->B, 0); break;			//SUB		B
				case 0x91: SUB(cpu, cpu->C, 0); break;			//SUB		C
				case 0x92: SUB(cpu, cpu->D, 0); break;			//SUB		D
				case 0x93: SUB(cpu, cpu->E, 0); break;			//SUB		E
				case 0x94: SUB(cpu, cpu->H, 0); break;			//SUB		H
				case 0x95: SUB(cpu, cpu->L, 0); break;			//SUB		L
				case 0x96: SUB(cpu, cpu->memory[HL_addr], 0); break;		//SUB		M
				case 0x97: SUB(cpu, cpu->A, 0); break;			//SUB		A
				case 0x98: SUB(cpu, cpu->B, cpu->flags.C); break;			//SBB		B
				case 0x99: SUB(cpu, cpu->C, cpu->flags.C); break;			//SBB		C
				case 0x9A: SUB(cpu, cpu->D, cpu->flags.C); break;			//SBB		D
				case 0x9B: SUB(cpu, cpu->E, cpu->flags.C); break;			//SBB		E
				case 0x9C: SUB(cpu, cpu->H, cpu->flags.C); break;			//SBB		H
				case 0x9D: SUB(cpu, cpu->L, cpu->flags.C); break;			//SBB		L
				case 0x9E: SUB(cpu, cpu->memory[HL_addr], cpu->flags.C); break;		//SBB		M
				case 0x9F: SUB(cpu, cpu->A, cpu->flags.C); break;			//SBB		A

				//0xA0 ... 0xAF
				case 0xA0: ANA(cpu, cpu->B); break;			//ANA		B
				case 0xA1: ANA(cpu, cpu->C); break;			//ANA		C
				case 0xA2: ANA(cpu, cpu->D); break;			//ANA		D
				case 0xA3: ANA(cpu, cpu->E); break;			//ANA		E
				case 0xA4: ANA(cpu, cpu->H); break;			//ANA		H
				case 0xA5: ANA(cpu, cpu->L); break;			//ANA		L
				case 0xA6: ANA(cpu, cpu->memory[HL_addr]); break;			//ANA		M
				case 0xA7: ANA(cpu, cpu->A); break;			//ANA		A
				case 0xA8: XRA(cpu, cpu->B); break;			//XRA		B
				case 0xA9: XRA(cpu, cpu->C); break;			//XRA		C
				case 0xAA: XRA(cpu, cpu->D); break;			//XRA		D
				case 0xAB: XRA(cpu, cpu->E); break;			//XRA		E
				case 0xAC: XRA(cpu, cpu->H); break;			//XRA		H
				case 0xAD: XRA(cpu, cpu->L); break;			//XRA		L
				case 0xAE: XRA(cpu, cpu->memory[HL_addr]); break;			//XRA		M
				case 0xAF: XRA(cpu, cpu->A); break;			//XRA		A

				//0xB0 ... 0xBF
				case 0xB0: ORA(cpu, cpu->B); break;			//ORA		B
				case 0xB1: ORA(cpu, cpu->C); break;			//ORA		C
				case 0xB2: ORA(cpu, cpu->D); break;			//ORA		D
				case 0xB3: ORA(cpu, cpu->E); break;			//ORA		E
				case 0xB4: ORA(cpu, cpu->H); break;			//ORA		H
				case 0xB5: ORA(cpu, cpu->L); break;			//ORA		L
				case 0xB6: ORA(cpu, cpu->memory[HL_addr]); break;			//ORA		M
				case 0xB7: ORA(cpu, cpu->A); break;			//ORA		A
				case 0xB8: CMP(cpu, cpu->B); break;			//CMP		B
				case 0xB9: CMP(cpu, cpu->C); break;			//CMP		C
				case 0xBA: CMP(cpu, cpu->D); break;			//CMP		D
				case 0xBB: CMP(cpu, cpu->E); break;			//CMP		E
				case 0xBC: CMP(cpu, cpu->H); break;			//CMP		H
				case 0xBD: CMP(cpu, cpu->L); break;			//CMP		L
				case 0xBE: CMP(cpu, cpu->memory[HL_addr]); break;			//CMP		M
				case 0xBF: CMP(cpu, cpu->A); break;			//CMP		A

				//0xC0 ... 0xCF
				case 0xC0: conditional_ret(cpu, cpu->flags.Z==0); break;			//RNZ
				case 0xC1: POP(cpu, &cpu->B, &cpu->C); break;		//POP		B
				case 0xC2: conditional_jmp(cpu, cpu->flags.Z==0); break;		//JNZ
				case 0xC3:			//JMP		a16
            mem_addr=get_immediate_addr(cpu, (cpu->PC)+1);
	    			JMP(cpu, mem_addr);
	    			break;
				case 0xC4: conditional_call(cpu, cpu->flags.Z==0); break;		//CNZ
				case 0xC5: PUSH(cpu, cpu->B, cpu->C); break;		//PUSH		B
				case 0xC6: ADD_immediate(cpu, (cpu->PC)+1, 0); break;			//ADI		d8
				case 0xC7: RST(cpu, 0); break;		//RST		0
				case 0xC8: conditional_ret(cpu, cpu->flags.Z==1); break;			//RZ
				case 0xC9: RET(cpu); break;		//RET
				case 0xCA: conditional_jmp(cpu, cpu->flags.Z==1); break;		//JZ
				case 0xCB: cpu->PC++; break;				//Undocumented Opcode
				case 0xCC: conditional_call(cpu, cpu->flags.Z==1); break;		//CZ
				case 0xCD:				//CALL		a16
	    			mem_addr=get_immediate_addr(cpu, (cpu->PC)+1);
	    			CALL(cpu, mem_addr);
	    			break;
				case 0xCE: ADD_immediate(cpu, (cpu->PC)+1, cpu->flags.C); break;		//ACI		d8
				case 0xCF: RST(cpu, 1); break;			//RST		1

				//0xD0 ... 0xDF
				case 0xD0: conditional_ret(cpu, cpu->flags.C==0); break;				//RNC
				case 0xD1: POP(cpu, &cpu->D, &cpu->E); break;		//POP		D
				case 0xD2: conditional_jmp(cpu, cpu->flags.C==0); break;		//JNC
				case 0xD3: break;			//OUT		d8
				case 0xD4: conditional_call(cpu, cpu->flags.C==0); break;		//CNC
				case 0xD5: PUSH(cpu, cpu->D, cpu->E); break;		//PUSH		D
				case 0xD6: SUB_immediate(cpu, (cpu->PC)+1, 0); break;			//SUI		d8
				case 0xD7: RST(cpu, 2); break;		//RST		2
				case 0xD8: conditional_ret(cpu, cpu->flags.C==1); break;				//RC
				case 0xD9: cpu->PC++; break;				//Undocumented Opcode
				case 0xDA: conditional_jmp(cpu, cpu->flags.C==1); break;			//JC
				case 0xDB: break;		//IN		d8
				case 0xDC: conditional_call(cpu, cpu->flags.C==1); break;		//CC
				case 0xDD: cpu->PC++; break;		//Undocumented Opcode
				case 0xDE: SUB_immediate(cpu, (cpu->PC)+1, cpu->flags.C); break;		//SBI		d8
				case 0xDF: RST(cpu, 3); break;		//RST		3

				//0xE0 ... 0xEF
				case 0xE0: conditional_ret(cpu, cpu->flags.P==0); break;			//RPO
				case 0xE1: POP(cpu, &cpu->H, &cpu->L); break;			//POP		H
				case 0xE2: conditional_jmp(cpu, cpu->flags.P==0); break;			//JPO
				case 0xE3: XTHL(cpu); break;		//XTHL
				case 0xE4: conditional_call(cpu, cpu->flags.P==0); break;		//CPO
				case 0xE5: PUSH(cpu, cpu->H, cpu->L); break;		//PUSH		H
				case 0xE6: ANI(cpu, (cpu->PC)+1); break;		//ANI		d8
				case 0xE7: RST(cpu, 4); break;
				case 0xE8: conditional_ret(cpu, cpu->flags.P==1);	break;			//RPE
				case 0xE9: cpu->PC=HL_addr; break;			//PCHL
				case 0xEA: conditional_jmp(cpu, cpu->flags.P==1); break;			//JPE
				case 0xEB: XCHG(cpu); break;			//XCHG
				case 0xEC: conditional_call(cpu, cpu->flags.P==1); break;		//CPE
				case 0xED: cpu->PC++; break;			//Undocumented Opcode
				case 0xEE: XRI(cpu, (cpu->PC)+1); break;		//XRI		d8
				case 0xEF: RST(cpu, 5); break;

				//0xF0 ... 0xFF
				case 0xF0: conditional_ret(cpu, cpu->flags.S==0); break;			//RP
				case 0xF1: POP_PSW(cpu); break;			//POP		PSW
				case 0xF2: conditional_jmp(cpu, cpu->flags.S==0); break;		//JP
				case 0xF3: cpu->interrupt_enable=0; cpu->PC++; break;		//DI
				case 0xF4: conditional_call(cpu, cpu->flags.S==0); break;		//CP
				case 0xF5: PUSH_PSW(cpu); break;			//PUSH		PSW
				case 0xF6: ORI(cpu, (cpu->PC)+1); break;		//ORI		d8
				case 0xF7: RST(cpu, 6); break;
				case 0xF8: conditional_ret(cpu, cpu->flags.S==1); break;			//RM
				case 0xF9: cpu->SP=HL_addr; cpu->PC++; break;			//SPHL
				case 0xFA: conditional_jmp(cpu, cpu->flags.S==1); break;			//JM
				case 0xFB: cpu->interrupt_enable=1; cpu->PC++; break;		//EI
				case 0xFC: conditional_call(cpu, cpu->flags.S==1); break;		//CM
				case 0xFD: cpu->PC++; break;				//Undocumented Opcode
				case 0xFE: CPI(cpu, (cpu->PC)+1); break;			//CPI		d8
				case 0xFF: RST(cpu, 7); break;

				default: printf("Invalid opcode!\n"); break;
    }

    #if PRINT_VALUES_FLAG
        print_values(cpu);
    #endif
}

//Initialize an i8080 cpu
i8080* i8080_init(){
    //Allocate a i8080 struct
    i8080* cpu=malloc(sizeof(i8080));

    //Initialize memory pointer to NULL
    cpu->memory=NULL;

    //Initialize PC and SP to 0
    cpu->PC=0;
    cpu->SP=0;

    //Initialize all status flags to 0
    cpu->flags.Z=0;
    cpu->flags.S=0;
    cpu->flags.P=0;
    cpu->flags.C=0;
    cpu->flags.AC=0;

    //Initialize all registers to 0
    cpu->A=0;
    cpu->B=0;
    cpu->C=0;
    cpu->D=0;
    cpu->E=0;
    cpu->H=0;
    cpu->L=0;

    cpu->interrupt_enable=0;		//Disable interrupt on startup
    cpu->instruction_cycles=0;

    return cpu;
}

//Prints the value of i8080's registers, flags, PC and SP pointers
//For debugging purposes
void print_values(i8080* cpu){
    printf("\tA=$%02x, B=$%02x, C=$%02x, D=$%02x, E=$%02x, H=$%02x, L=$%02x, SP=$%04x\n",
					cpu->A, cpu->B, cpu->C, cpu->D, cpu->E, cpu->H, cpu->L, cpu->SP);

    printf("\tZ=%d, S=%d, P=%d, CY=%d, AC=%d\n",
					cpu->flags.Z, cpu->flags.S, cpu->flags.P, cpu->flags.C, cpu->flags.AC);
}
