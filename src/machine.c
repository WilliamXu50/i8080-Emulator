#include "machine.h"
#include "i8080_cpu.h"

machine_t* init_machine(){
    machine_t* machine=calloc(1, sizeof(machine_t));

    //Allocate space for the machine memory
    machine->machine_mem=malloc(sizeof(uint8_t) * 0x4000);

    //Initiate an i8080 CPU
    machine->cpu=i8080_init();

    //Set the cpu's memory reference to the allocated memory space of the machine
    machine->cpu->memory=machine->machine_mem;

    machine->int_num=1;     //Interrupt number is resetted to 1
    machine->port_in1=(1<<3);     //Bit 3 always set
    machine->port_in2=0;
    machine->shift0=0;
    machine->shift1=0;
    machine->shift_offset=0;
    machine->quit_status=0;       //Just started, so no quit yet

    //Clear the screen buffer upon reset
    memset(machine->screen_buffer, 0, sizeof(machine->screen_buffer));

    return machine;
}

void destroy_machine(machine_t* machine){
    free(machine->cpu);
    free(machine->machine_mem);
    free(machine);
}

void load_file_into_mem(machine_t* machine, char* file_path, uint16_t offset){
    //Open the file in binary read mode
    FILE* fp=fopen(file_path, "rb");

    if(!fp){      //Test if file exists
        printf("File doesn't exist!\n");
        exit(1);
    }

    //Get file size
    fseek(fp, 0, SEEK_END);
    int f_size=ftell(fp);
    fseek(fp, 0, SEEK_SET);

    //Read file into machine memory
    fread(&machine->machine_mem[offset], f_size, 1, fp);

    fclose(fp);
}

void load_game(machine_t* machine){
    //Loads all the ROM files of space invaders
    load_file_into_mem(machine, "ROM/invaders.h", 0x0000);
    load_file_into_mem(machine, "ROM/invaders.g", 0x0800);
    load_file_into_mem(machine, "ROM/invaders.f", 0x1000);
    load_file_into_mem(machine, "ROM/invaders.e", 0x1800);
}

void machine_execute(machine_t* machine){
    uint8_t opcode=read_mem(machine->cpu, machine->cpu->PC);

    if(opcode==0xD3){     //OUT   d8
        uint8_t port=read_mem(machine->cpu, (machine->cpu->PC)+1);

        switch(port){
            case 2: machine->shift_offset=(machine->cpu->A) & 0x07; break;
            case 4:
                machine->shift0=machine->shift1;
                machine->shift1=machine->cpu->A;
                break;
            }
            machine->cpu->instruction_cycles+=10;
            machine->cpu->PC+=2;
        }

    else if(opcode==0xDB){    //IN    d8
        uint8_t port=read_mem(machine->cpu, (machine->cpu->PC)+1);

        switch(port){
            uint16_t val;
            case 1: machine->cpu->A=machine->port_in1; break;
            case 2: machine->cpu->A=machine->port_in2; break;
            case 3:
                val=(machine->shift1<<8)|(machine->shift0);
                machine->cpu->A=(val>>(8-machine->shift_offset)) & 0xFF;
                break;
            }
            machine->cpu->instruction_cycles+=10;
            machine->cpu->PC+=2;
        }
    else{   //Any other instructions the emulator can execute
        i8080_emulator(machine->cpu);
    }
}

void machine_update_screen(machine_t* machine){
    for(int x=0; x<SCREEN_WIDTH; x++){

        uint16_t offset=0x241F+(x * 0x20);

        for(int y=0; y<SCREEN_HEIGHT; y+=8){

            uint8_t data_byte=machine->machine_mem[offset];

            for(int bit=0; bit<8; bit++){
                
                if(y>0 && y<=30){       //Scoreboard is in black and white
                    if((data_byte<<bit) & 0x80){
                        machine->screen_buffer[y+bit][x][R]=255;
                        machine->screen_buffer[y+bit][x][G]=255;
                        machine->screen_buffer[y+bit][x][B]=255;
                    }
                    else{
                        machine->screen_buffer[y+bit][x][R]=0;
                        machine->screen_buffer[y+bit][x][G]=0;
                        machine->screen_buffer[y+bit][x][B]=0;
                    }
                }
                else if(y>31 && y<60){      //Right underneath the scoreboard, pixels landed there would be in red
                    if((data_byte<<bit) & 0x80){
                        machine->screen_buffer[y+bit][x][R]=204;
                        machine->screen_buffer[y+bit][x][G]=0;
                        machine->screen_buffer[y+bit][x][B]=0;
                    }
                    else{
                        machine->screen_buffer[y+bit][x][R]=0;
                        machine->screen_buffer[y+bit][x][G]=0;
                        machine->screen_buffer[y+bit][x][B]=0;
                    }
                }
                else if(y>=192){       //Bottom portion of the screen is in green
                    if((data_byte<<bit) & 0x80){
                        machine->screen_buffer[y+bit][x][R]=0;
                        machine->screen_buffer[y+bit][x][G]=204;
                        machine->screen_buffer[y+bit][x][B]=0;
                    }
                    else{
                        machine->screen_buffer[y+bit][x][R]=0;
                        machine->screen_buffer[y+bit][x][G]=0;
                        machine->screen_buffer[y+bit][x][B]=0;
                    }
                }
                else{       //Everything else is black and white
                    if((data_byte<<bit) & 0x80){
                        machine->screen_buffer[y+bit][x][R]=255;
                        machine->screen_buffer[y+bit][x][G]=255;
                        machine->screen_buffer[y+bit][x][B]=255;
                    }
                    else{
                        machine->screen_buffer[y+bit][x][R]=0;
                        machine->screen_buffer[y+bit][x][G]=0;
                        machine->screen_buffer[y+bit][x][B]=0;
                    }
                }
            }
            offset--;
        }
    }
}

void generate_interrupt(machine_t* machine, uint8_t int_num){
    //Only generate interrupt if interrupt-enable is set
    if(machine->cpu->interrupt_enable==1){
        //Disable interrupt enable
        machine->cpu->interrupt_enable=0;
        //Generate the interrupt with the appropriate interrupt number
        RST(machine->cpu, int_num);
        //11 cycles taken for interrupts
        machine->cpu->instruction_cycles+=11;
    }
}
