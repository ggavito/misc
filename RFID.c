#include <hidef.h>      /* common defines and macros */
#include "derivative.h"      /* derivative-specific definitions */
#include <stdio.h>

/* RFID lock code
Looks for a passive RFID tag, then locks (moves servo) once it scans one. Only that card's unique RFID can then unlock.
HCS12 only has 1 hard UART, a 'soft' one is needed for the reader (receiving and interpreting raw serial data) as the hard one
is used to upload code and debug
Written by Garet Gavito
 */

void INIT_HCS12(void);

char getByte(void);
char key[12];
char id[12] = {0x0A, '0','1','0','0','3','2','D','E','2','E', 0x0D};   // start byte, 10-byte ID, stop byte

    
int match;  
int i;
int j;
int dir;
int forward = 1;
int back = 2;
int idSet = 0;


void main(void) {
  
  INIT_HCS12();
 
  for(;;) {
        if (idSet == 0){
          for(i = 0; i < 12; i++) {
        
            key[i] = getByte();    //sets the key ID, receive data from RFID reader
          }
          PTT_PTT7 = 1;    // RFID search off
           
          if(PWMDTY0 == 1){         
              dir = back;            
            }                        
          if (dir == back){          
              PWMDTY0 = 13;           // move servo to locked
            }
            
          for(i = 0; i < 8; i++){    //blink green LED 4 times in 1s
      
            TC0 = TCNT + 46875/2;
            TFLG1_C0F = TFLG1_C0F_MASK;
            while(TFLG1_C0F == 0);
            PTT_PTT1 ^= 1;
            
              
          } // end for
          idSet = 1;  // key ID has been set
        } // end if
        PTT_PTT7 = 0;    // RFID search on


        for(i = 0; i < 12; i++) {
        
          id[i] = getByte(); // receive RFID reader
        }                  
      
      match = 0; // reset match before each check
      
      for(j = 0; j < 12; j++){
        
        if(id[j] == key[j]) {
          match++;
        
        }
      }//end for
       
      if(match == 12){  // CORRECT RFID CASE
            
            PTT_PTT7 = 1;    // RFID search off

            if(PWMDTY0 == 13){
              dir = forward;
            }
            if (dir == forward) {
              PWMDTY0 = 1;          // move to unlocked
              idSet = 0;        // begin looking for a new key ID
              
            }
         
         PTT_PTT1 = 1;    // green LED on
         for(i = 0; i < 3; i++){      //about 1s delay
      
            TC0 = TCNT + 62000;
            TFLG1_C0F = TFLG1_C0F_MASK;
            while(TFLG1_C0F == 0);
         }
         PTT_PTT1 = 0;    // green LED off
         PTT_PTT7 = 0;    // RFID search on
  
      } else {    // INCORRECT RFID CASE
         PTT_PTT7 = 1;    // RFID search off
         PTT_PTT3 = 1;    // red LED on
         for (i = 0; i < 3; i++) {         //1s delay
            
            TC0 = TCNT + 62000;
            TFLG1_C0F = TFLG1_C0F_MASK;
            while(TFLG1_C0F == 0);
         }
         PTT_PTT3 = 0;    // red LED off
         PTT_PTT7 = 0;    // RFID search on
      }      
      

  }/* loop forever */
  _FEED_COP();  // feeds the dog
}//end main

void INIT_HCS12(void) {

   // Configure the Data Direction Registers    
   DDRT = 0xEF; //PT4 input, rest outputs

   TIOS_IOS0 = 1; // Enable the compare TIOS address = 0x40
   TSCR2_PR2 = 1; TSCR2_PR1 = 1; TSCR2_PR0 = 1; // 1:128 prescaler
   TSCR1_TEN = 1; // Fire up the timer TSCR address = 0x46  
   
   PTT_PTT7 = 0;  //RFID search on
   
   CLKSEL_PLLSEL = 0; // turn off PLL for source clock
   PLLCTL_PLLON = 1; // turn on PLL
   SYNR_SYN = 2; // set PLL multiplier 8 Mhz
   REFDV_REFDV = 0; // set PLL divider (divide by REFDV+1)
 
   CRGFLG_LOCKIF = 1; // clear the lock interrupt flag
   while(!CRGFLG_LOCK){} // wait for the PLL to lock
   CLKSEL_PLLSEL = 1; // select the PLL for source clock
   
   // Configure PWM subsystem clocks to generate the longest possible period         
   PWMPRCLK_PCKA = 0x03;     // Use a 1:2 prescalar      see page 356 of MC9S12GC Family Reference Manual 
   PWMSCLA = 150;            // Scale Bus Clock by 2*234 see page 362 of MC9S12GC Family Reference Manual    
            
   // Configure PWM channel 0  
   PWMCLK_PCLK0 = 1;         // Use (slow) A clock for channel 0           
   PWMPER0 = 0xFF;           // Define the period of the PWM waveform
   PWMDTY0 = 1;              // Start with near 0% duty cycle
   PWMPOL_PPOL0 = 1;         // Make PWMDTY0 the time high
   MODRR_MODRR0 = 1;         // Mux out the PWM channel, not the GPIO
   PWME_PWME0 = 1;           // Fire up the entire PWM subsystem
   EnableInterrupts;         // Enable interrupts for debugging
} // end INIT_HCS12

char getByte(void) {
   // soft UART to receive data on PT4
   int k; 
   char data = 0;   // byte of data
   
   TSCR2_PR2 = 0; 
   TSCR2_PR1 = 0; 
   TSCR2_PR0 = 0; // 1:1 prescaler for short timers in this function
   

   while(PTT_PTT4 == 1); //wait for start bit
   TC0 = TCNT + 4992; // 208 us delay to get to middle of bit
   TFLG1 = TFLG1_C0F_MASK;
   while(TFLG1_C0F == 0);
   
   for(k = 0; k < 8; k++){
       TC0 = TCNT + 9984; // 417 us delay to sample bits at center
       TFLG1 = TFLG1_C0F_MASK;
       while(TFLG1_C0F == 0);

       data = data | (PTT_PTT4 << k); // LSB first
   }
   TC0 = TCNT + 9984; // 417 us delay
   TFLG1 = TFLG1_C0F_MASK;
   while(TFLG1_C0F == 0);
   
   TSCR2_PR2 = 1; 
   TSCR2_PR1 = 1; 
   TSCR2_PR0 = 1; // 1:128 prescaler for long timers in main()
   
   return data;
}