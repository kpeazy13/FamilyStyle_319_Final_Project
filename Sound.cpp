#include <stdint.h>
#include <ti/devices/msp/msp.h>
#include "Sound.h"
#include "sounds/sounds.h"
#include "../inc/DAC5.h"
#include "../inc/Timer.h"

const uint8_t* soundPt    = 0;
uint32_t       soundCount = 0;
uint32_t       soundIndex = 0;
uint32_t       numPlays = 3;

void SysTick_IntArm(uint32_t period, uint32_t priority){
  SysTick->CTRL = 0x00;
  SysTick->LOAD = period - 1;
  SysTick->VAL  = 0;
  SCB->SHP[1]   = (SCB->SHP[1] & (~0xC0000000)) | (priority << 30);
  SysTick->CTRL = 0x07;
}

void Sound_Init(void){
  DAC5_Init();
  soundPt    = 0;
  soundCount = 0;
  soundIndex = 0;
  SysTick_IntArm(7256, 0);
}

extern "C" void SysTick_Handler(void);
void SysTick_Handler(void){
  if(soundCount == 0) return;
  DAC5_Out(soundPt[soundIndex]<<3);
  soundIndex++;
  if(soundIndex >= soundCount){
    soundIndex = 0;
    numPlays--;
  }
  if(numPlays == 0){
    soundCount = 0;
  }
}

void Sound_Start(const uint8_t *pt, uint32_t count){
  soundPt    = pt;
  soundCount = count;
  soundIndex = 0;
  if (pt == ding){
    numPlays = 3;
  } else {
    numPlays = 1;
  }
}

void Sound_Ding(void){
  Sound_Start(ding, 2000);
}

void Sound_GameOver(void){
  Sound_Start(gameover, 8796);
}