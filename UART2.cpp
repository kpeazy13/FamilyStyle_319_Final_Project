/* UART2.cpp
 * Your name
 * Data:
 * PA22 UART2 Rx from other microcontroller PA8 IR output<br>
 */


#include <ti/devices/msp/msp.h>
#include "UART2.h"
#include "../inc/Clock.h"
#include "../inc/LaunchPad.h"
#include "../inc/FIFO2.h"

uint32_t LostData;
Queue FIFO2;

// power Domain PD0
// for 80MHz bus clock, UART2 clock is ULPCLK 40MHz
// initialize UART2 for 2375 baud rate
// no transmit, interrupt on receive timeout
void UART2_Init(void){
    LostData = 0;
    UART2->GPRCM.RSTCTL = 0xB1000003;
    UART2->GPRCM.PWREN  = 0x26000001;
    Clock_Delay(24);
    IOMUX->SECCFG.PINCM[PA22INDEX] = 0x00040082;
    UART2->CLKSEL = 0x08;
    UART2->CLKDIV = 0x00;
    UART2->CTL0 &= ~0x01;             
    UART2->CTL0 = 0x00020008;       
    UART2->IBRD = 1052;               
    UART2->FBRD = 40;
    UART2->LCRH = 0x00000030;      
    UART2->IFLS = (4 << 8);           
    UART2->CPU_INT.IMASK = 0x00000001;     
    NVIC->ISER[0] = (1 << UART2_INT_IRQn);
    UART2->CTL0 |= 0x01;      
}
//------------UART2_InChar------------
// Get new serial port receive data from FIFO2
// Input: none
// Output: Return 0 if the FIFO2 is empty
//         Return nonzero data from the FIFO1 if available
char UART2_InChar(void){
  char out;
// write this
  while(FIFO2.IsEmpty()){}
  FIFO2.Get(&out);
  return out;
}

extern "C" void UART2_IRQHandler(void);
void UART2_IRQHandler(void){ uint32_t status; char letter;
  status = UART2->CPU_INT.IIDX; // reading clears bit in RTOUT
  if(status == 0x01){   // 0x01 receive timeout
    GPIOB->DOUTTGL31_0 = BLUE; // toggle PB22 (minimally intrusive debugging)
    GPIOB->DOUTTGL31_0 = BLUE; // toggle PB22 (minimally intrusive debugging)
    // read all data, putting in FIFO
    // finish writing this
    while((UART2->STAT & 0x04) == 0){
      letter = UART2->RXDATA;
      if(!FIFO2.Put(letter)){
        LostData++;
      }
    }
    UART2->CPU_INT.ICLR = 1;
    GPIOB->DOUTTGL31_0 = BLUE; // toggle PB22 (minimally intrusive debugging)
  }
}
