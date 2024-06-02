#include <msp430.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "LiquidCrystal_I2C.h"
#include <time.h>
#include <vector>

#define backpackSize 16
#define DEBOUNCE_TIME 500 // 0.3 seconds debounce time in milliseconds

char monsterTemplates[8][8] = {
   {0x00,0x0A,0x0E,0x0E,0x1F,0x0E,0x0A,0x00},
   {0x00,0x02,0x1E,0x16,0x1E,0x06,0x06,0x06},
   {0x00,0x02,0x0E,0x06,0x06,0x06,0x06,0x06},
   {0x00,0x04,0x0E,0x04,0x04,0x1F,0x04,0x04},
   {0x00,0x00,0x0A,0x04,0x0A,0x0A,0x04,0x00},
   {0x00,0x1F,0x11,0x15,0x0A,0x0A,0x04,0x00},
   {0x00,0x1F,0x15,0x1F,0x0E,0x0A,0x04,0x00},
   {0x1F,0x1F,0x1F,0x1F,0x1F,0x1F,0x1F,0x1F}
};
unsigned int NumRand();
struct monster{
    char iconId = 0;
    char atk= 0;
    char hp = 0;
    bool occupied = false;
    void random() {
        iconId = rand();
        atk = NumRand();
        hp = NumRand();
        occupied = true;
    }
};
struct monster monsters[backpackSize];


unsigned int monsterCount = 1;
unsigned int selectIndex = 0;
volatile unsigned int debounceTimer = 0;
unsigned short tick = 0; //this tick is used for actions that involve the random seed


int flashing = 0;


// Setup timer
void setupTimer() {
    TB0CTL = TBSSEL__SMCLK | MC__UP | TBCLR; // SMCLK, Up mode, clear TBR
    TB0CCR0 = 1000 - 1;                      // 1 MHz SMCLK, 1000 cycles = 1 ms
    TB0CCTL0 = CCIE;                         // Enable Timer_B interrupt
}

int nowChar;
char blank[] = " ";
unsigned int NumRand(){
    static long int seed = (time(NULL));;
    seed = (seed * 12345 + 54321) & 0x7fffffff;
    return seed % 99+1;
}


int rand() {
  static long int seed = (time(NULL));;
  seed = (seed * 110351 + 12345) & 0x7fffffff;
  if (tick%1 == 0){
      return seed % 7;
  }else{
      nowChar = 33+seed%222;
      if (nowChar == 254 || nowChar == 160) nowChar = seed%7;
      return nowChar;
  }
}

void delay(unsigned int ms) {
  unsigned int i;
  for (i = 0; i < ms; i++) {
    __delay_cycles(1000); // Delay for 1 millisecond
  }
}

void displayAllMonsters(){
    __enable_interrupt();
    for(int j=backpackSize-1;j>=0;j--)
    {
        LCD_ShowCustomChar(char(254),j,0);
    }
    int i=0;
    while(monsters[i].occupied)
    {
       LCD_ShowCustomChar(monsters[i].iconId,i,0);
        i++;
    }
//    for (int i = 0; i <monsterCount-1-1; i++) {
//    }
}

void removeMonsterAtSelection() {
    if (monsterCount == 1) {
        //due to the fact that if there is only one monster, deleting it would cause multiple issues, such as the fact that the selection pointer has to be visible at all times.
        return;
    }
    // Shift elements to the left
    int i;
    for (i = selectIndex; i < monsterCount - 1; i++) {
        monsters[i] = monsters[i + 1];
    }
    monsters[monsterCount-1].occupied = false;
//    LCD_ShowCustomChar(124,i+2,0);
    //__enable_interrupt();
    if (selectIndex == monsterCount-1){
        selectIndex -= 1;
    }

    // Update size
    (monsterCount)--;
    displayAllMonsters();
}


void addElement() {
  if (monsterCount < backpackSize) {
    monsterCount++;
    monsters[monsterCount - 1].random();
    //LCD_ShowCustomChar(monsters[monsterCount - 1].iconId, monsterCount - 1, 0); //show the new one
  }
}


void setupPorts() {
    WDTCTL = WDTPW | WDTHOLD;   // Stop watchdog timer

    // Configure buttons
    P2OUT |= BIT0 | BIT1 | BIT2 | BIT3;                          // P2.0/2.1/2.2 pull-up
    P2REN |= BIT0 | BIT1 | BIT2 | BIT3;                          // P2.0/2.1/2.2 pull-up resistor enable
    P2IES |= BIT0 | BIT1 | BIT2 | BIT3;                          // P2.0/2.1/2.2 Hi/Low edge

    P1OUT |= BIT2;                                 // P1.2 pull-up
    P1REN |= BIT2;                                 // P1.2 pull-up resistor enable
    P1IES &= ~BIT2;                                // P1.2 Low/Hi edge

    P2IE |= BIT0 | BIT1 | BIT2 | BIT3;                           // P2.0/2.1/2.2 interrupt enabled
    PM5CTL0 &= ~LOCKLPM5;                          // Disable GPIO power-on default high-impedance mode

    P2IFG &= ~(BIT0 | BIT1 | BIT2 | BIT3);                       // Clear P2.0/2.1/2.2 interrupt flags
}


// Setup LCD
void setupLCD() {
    I2C_Init(0x27);             // Initialize I2C with LCD address 0x27
    LCD_Setup();
    LCD_ClearDisplay();

    // Create custom characters
    for (int i = 0; i < 8; i++) {
        LCD_CreateChar(i, monsterTemplates[i]);
    }
}

void fixedNum(int x, int num){
    LCD_SetCursor(x, 1);
    LCD_WriteNum(0);
    LCD_SetCursor((num>=10)?x:x+1, 1);
    LCD_WriteNum(num);
}
char MSG_atk[] = {'A','T','K',':','\0'};
char MSG_hp[] = {'H','P',':','\0'};
char buffer[32];
bool removeEvent=false;
void main(void) {
    setupPorts();
    setupTimer();
    //__enable_interrupt(); //Enable maskable interrupts

  // LCD function call and setups:
  setupLCD();
  //create first monster
  monsters[0].random();
  unsigned int loopCount = 0;
  while(1){
      loopCount += 1;
      if (loopCount % 3 == 0) {flashing = ~flashing;}
      for (int i = 0; i < monsterCount; i++) {
          if (monsters[i].occupied == true ){
              LCD_ShowCustomChar((flashing & i == selectIndex) ? 255 : monsters[i].iconId,i,0);//255 是全黑編碼，這裡在全黑和腳色圖形做選擇
          }else{
              LCD_ShowCustomChar(254,i,0);
          }
      }
      LCD_SetCursor(0,1);
      sprintf(buffer, "%s%d%s%d", MSG_atk,  monsters[selectIndex].atk, MSG_hp,  monsters[selectIndex].hp);
      LCD_Write(buffer);

      //第二航最後兩個文字放怪物總數
      fixedNum(14, monsterCount);
      delay(200);
      if(removeEvent)
      {
          removeMonsterAtSelection();
          removeEvent=false;
      }
  }

}

#if defined(__TI_COMPILER_VERSION__) || defined(__IAR_SYSTEMS_ICC__)
#pragma vector=PORT2_VECTOR
__interrupt void Port_2(void)
#elif defined(__GNUC__)
void __attribute__ ((interrupt(PORT1_VECTOR))) Port_2 (void)
#else
#error Compiler not supported!
#endif
{
    if(P2IFG&BIT0)//P2.0 trap
    {
       //select the next element (mosnter)
        P2IFG &= ~BIT0;
        if (debounceTimer > 0) return; // Check debounce time
        debounceTimer = DEBOUNCE_TIME;
        // Button press detected

        LCD_ShowCustomChar(monsters[selectIndex].iconId,selectIndex,0);

        selectIndex = (selectIndex + 1) % monsterCount;

        LCD_ShowCustomChar(255,selectIndex,0);


    }
    else if(P2IFG&BIT1){//P2.1 trap
       //adding element
        P2IFG &= ~BIT1;
        if (debounceTimer > 0) return; // Check debounce time
        debounceTimer = DEBOUNCE_TIME;
        // Button press detected
        addElement();
    }else if (P2IFG&BIT2){//P2.2 trap
       //remove element
        P2IFG &= ~BIT2;
        if (debounceTimer > 0) return; // Check debounce time
        debounceTimer = DEBOUNCE_TIME;
        removeEvent=true; //due to the fact that removing has to also be inside the loop, we use a boolean to call the particular function.

    }else if (P2IFG&BIT3){//P2.3 trap
        //refresh
        P2IFG &= ~BIT3;
       if (debounceTimer > 0) return; // Check debounce time
       debounceTimer = DEBOUNCE_TIME;
       __enable_interrupt();
       LCD_ClearDisplay();
       LCD_Setup();
       I2C_Init(0x27);             // Initialize I2C with LCD address 0x27
       for (int i = 0; i < monsterCount-1; i++) {
           LCD_ShowCustomChar(monsters[i].iconId,i,0);
       }
    }


}


#if defined(__TI_COMPILER_VERSION__) || defined(__IAR_SYSTEMS_ICC__)
#pragma vector=TIMER0_B0_VECTOR
__interrupt void Timer_B(void)
#elif defined(__GNUC__)
void __attribute__ ((interrupt(TIMER0_B0_VECTOR))) Timer_B(void)
#else
#error Compiler not supported!
#endif
{
    if (debounceTimer > 0) {
        debounceTimer--;
    }
    tick++;
}

