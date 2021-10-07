# Main Code
This project will emulate the intel 8080 CPU as well as the hardware and graphics that Space Invaders runs on.

# Main Loop of the Game
The main loop of the game does the following: 
As long as the player doesn't press exit on the console window or press 'Q', the game will run. Space Invaders runs on 60 fps 
and with a 2 MHz CPU. This means that every second, 60 frames will be updated and displayed on the screen, which would mean that 
every 1/60 secs (or ~17 ms) a new frame will be updated. 
So the main loop will look as the follows:

`while(not_quit){
     //run the game
}`

Next we will need a timing system to track the interval of 17 ms. I used the built in `SDL_GetTicks()` built-in function from the SDL library, 
but it can also be done using `gettimeofday()` from `sys/time.h` (assuming you're using an POSIX-interfaced OS).

`while(not_quit){
      //get user keyboard input
      //Get elapsed time
      //...
}`

Since 60 frames needs to be updated and displayed in 1 sec (e.g. updated & displayed every 17 ms), we need a conditional that ill check 
if 17 ms has elapsed:

`while(not_quit){`
    //get user keyboard input
     
    if(//elapsed time >= 17 ms){
         `//...`
    }
`}`

Now to understand what happens after every 17 ms interval, we will examine how the actual game itself is displayed on the screen.
The game updates the screen twice during each frame; once when the video rendering reaches the middle of the screen (during which a 
mid-screen interrupt would trigger), and another one when the rendering reaches the end of the screen (during which an end-of-screen
interrupt would trigger). The game will update and draw the top half of the screen when the mid-screen interrupt triggers, and then draws
the bottom half of the screen when the end-of-screen interrupt triggers. Thus for each frame, the game first draws top then the bottom.

The timing of the mid-screen interrupt and end-of-screen interrupt can be emulated with respect to the number of CPU cycles taken. The total
CPU cycles is just the accumulation of cycles taken by executing instructions. 

Since the 8080 CPU has a clock rate of 2 MHz and the game runs on 60 fps, we can derive the number of CPU cycles taken for each frame:
`CLOCK_RATE/FPS => 2000000 (1/sec) / 60 (frames/sec)`, thus each frame will take `CLOCK_RATE/FPS` CPU cycles. Therefore the mid-screen
interrupt will trigger when the total CPU cycles _for that particular frame_ reaches around ~ half of `CLOCK_RATE/FPS`. Likewise, the
end-of-screen interrupt will trigger after a full `CLOCK_RATE/FPS` cycles has completed:

Below is a code snippet of the main driver loop:

`while(not_quit){
      //get user keyboard input
     
      if(//elapsed time >= 17 ms){
            //Begin a new frame
            //Update the current time first
            
            int cpu_cycles=0;     //This will keep track of the total number of CPU cycles
            
            while(cpu_cycles <= CLOCK_RATE/FPS){
            
[1]               int current_cycle=machine->cpu->instruction_cycles;

                  machine_execute(machine);

                  cpu_cycles+=machine->cpu->instruction_cycles-current_cycle;
 -------------------------------------------------------------------------------------------
[2]               if(machine->cpu->instruction_cycles>=HALF_CYCLES_PER_FRAME){
                        //Trigger interrupts
                        
                        machine->cpu->instruction_cycles-=HALF_CYCLES_PER_FRAME;

                        machine->int_num=(machine->int_num==1 ? 2 : 1);
                  }
            }
            //Update screen buffer
            //Update screen buffer to display (render graphics)
      }
}`

[1] Execute instructions from the program memory (using the 8080 emulator), and then updates the `cpu_cycles` count. 
This effectively keeps track of the total number of CPU cycles taken as the result of executing program instructions. This will ensure 
that the total number of CPU cycles won't exceed `CLOCK_RATE/FPS` cycles for any frame.

[2] Once the CPU cycles reaches around  ~`1/2 CLOCK_RATE/FPS` cycles, the mid-screen interrupt will trigger. Note here that for [2], I used the 
struct member variable `machine->cpu->instruction_cycles` instead of `cpu_cycles` to compare the total number of CPU cycles. This is because I 
designed it so that `machine->cpu->instruction_cycles` will be decremented by `1/2 CLOCK_RATE/FPS` cycles after the mid-screen interrupt triggers, 
while `cpu_cycles` will keep on incrementing after executing instructions.

This way, `machine->cpu->instruction_cycles` will keep track of when to trigger mid-screen interrupt as well as the end-of-screen interrupt 
(since both interrupts are `1/2 CLOCK_RATE/FPS` cycles apart). And `cpu_cycles` will ensure that we don't exceed the appropriate number of cycles 
per frame. Also note that the interrupt number alternates after every interrupt trigger, thus the interrupts will alternate between 
mid-screen and end-of-screen, starting with the mid-screen interrupt for every frame.

So, the code snippet above does the following: 
assuming the player doesn't quit or exit, the user keyboard input is first polled, then:

After every 17 ms, a new frame needs to be displayed: this is accomplished by continuously executing the game program instructions (as pointed
to by the program counter) until the total number of CPU cycles for that frame reaches about ~ `1/2 CLOCK_RATE/FPS` cycles. Then the 
mid-screen interrupt would trigger. Specific ISR for mid-screen interrupt would execute, then returns to the normal execution. The game 
program instructions will continued to be executed until the total number of CPU cycles for that frame reaches reaches ~ `CLOCK_RATE/FPS` cycles, 
at which the end-of-screen interrupt would trigger. After both interrupts are serviced, the code will exit the executing instructions loop,
update the screen buffer and lastly will update the console screen itself.

This cycle of execution -> mid-screen interrupt -> execution -> end-of-screen interrupt -> update buffer -> update screen will be implemented 
for every new frame to be displayed. 

# How the Screen is Displayed


