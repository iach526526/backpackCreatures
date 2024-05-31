#include <msp430.h>
#include "LiquidCrystal_I2C.h"
#include <string.h>

unsigned int TXBUF; // 要傳輸的數據

void I2C_Init(int addr) {
    UCB0CTLW0 |= UCSWRST;       // 將 eUSCI_B0 置於軟體復位

    //-- 設置 eUSCI_B0 -------------------------------------------
    UCB0CTLW0 |= UCSSEL_3;      // 選擇 BRCLK = SMCLK = 1MHz
    UCB0BRW = 10;               // 將 BRCLK 除以 10 獲得 SCL = 100kHz

    UCB0CTLW0 |= UCMODE_3;      // 設置為 I2C 模式
    UCB0CTLW0 |= UCMST;         // 設置為主模式
    UCB0CTLW0 |= UCTR;          // 設置為發送模式
    UCB0I2CSA = addr;           // 從屬地址 = 0x27

    UCB0CTLW1 |= UCASTP_2;      // 當 UCB0TBCNT 達到時自動停止
    UCB0TBCNT = 0x01;           // 發送 1 byte 的數據

    //-- 設置端口 ----------------------------------------------
    P1SEL1 &= ~BIT3;            // P1.3 = SCL
    P1SEL0 |=  BIT3;

    P1SEL1 &= ~BIT2;            // P1.2 = SDA
    P1SEL0 |=  BIT2;

    PM5CTL0 &= ~LOCKLPM5;       // 開啟 GPIO

    //-- 解除 eUSCI_B0 軟體復位 ---------------------------------
    UCB0CTLW0 &= ~UCSWRST;      // 解除 eUSCI_B0 的軟體復位

    UCB0IE |= UCTXIE0;          // 啟用 I2C_B0 發送中斷
    __enable_interrupt();
}

void I2C_Send(int value) {
    UCB0CTLW0 |= UCTXSTT; // 產生 START 條件
    TXBUF = value;        // 將要傳輸的數據放入發送緩衝區
}

void pulseEnable(int value) {
    I2C_Send(value | En);  // En 高
    __delay_cycles(150);   // 使能脈衝必須大於 450ns
    I2C_Send(value & ~En); // En 低
    __delay_cycles(1500);  // 指令需要超過 37us 來穩定
}

void write4bits(int value) {
    I2C_Send(value);
    __delay_cycles(50);
    pulseEnable(value);
}

void LCD_Send(int value, int mode) {
    int high_b = value & 0xF0;
    int low_b = (value << 4) & 0xF0;
    write4bits(high_b | mode);
    write4bits(low_b | mode);
}

void LCD_Write(char *text) {
    unsigned int i;
    for (i = 0; i < strlen(text); i++) {
        LCD_Send((int)text[i], Rs | LCD_BACKLIGHT);
    }
}

void DisplayBlack() {
    unsigned int i;
    for (i = 0; i < 8; i++) {
        LCD_Send((int)(0x1F), Rs | LCD_BACKLIGHT);
    }
}

void LCD_WriteNum(unsigned int num) {
    unsigned int reverseNum = 0;
    unsigned int digits = 0;
    int i;

    if (num == 0) {
        LCD_Send(0 | 0x30, Rs | LCD_BACKLIGHT);
    } else {
        while (num > 0) {
            reverseNum = reverseNum * 10 + (num % 10);
            num /= 10;
            digits++;
        }

        for (i = 0; i < digits; ++i) {
            LCD_Send((reverseNum % 10) | 0x30, Rs | LCD_BACKLIGHT);
            reverseNum /= 10;
        }
    }
}

void LCD_SetCursor(int col, int row) {
    int row_offsets[] = {0x00, 0x40, 0x14, 0x54};
    LCD_Send(LCD_SETDDRAMADDR | (col + row_offsets[row]), LCD_BACKLIGHT);
}
void LCD_CreateChar(int chId, char charmap[]) {//chId0~7
    chId &= 0x7;  // Ensure location is between 0 and 7
    LCD_Send(LCD_SETCGRAMADDR | (chId << 3), LCD_BACKLIGHT);
    for (int i = 0; i < 8; i++) {
        LCD_Send(charmap[i], Rs | LCD_BACKLIGHT);
    }
}

void LCD_ShowCustomChar(int chId, int col, int row) {
    LCD_SetCursor(col, row);
    LCD_Send(chId, Rs | LCD_BACKLIGHT);
}
void LCD_ClearDisplay(void) {
    LCD_Send(LCD_CLEARDISPLAY, LCD_BACKLIGHT);
    __delay_cycles(50);
}

void LCD_leftToRight(void) {
    LCD_Send(LCD_ENTRYMODESET | LCD_ENTRYLEFT | LCD_ENTRYSHIFTDECREMENT, LCD_BACKLIGHT);
}

void LCD_rightToLeft(void) {
    LCD_Send(LCD_ENTRYMODESET | LCD_ENTRYRIGHT | LCD_ENTRYSHIFTDECREMENT, LCD_BACKLIGHT);
}

void LCD_Setup(void) {
    int _init[] = {LCD_init, LCD_init, LCD_init, LCD_4_BIT};
    int _setup[5];
    int mode = LCD_BACKLIGHT;
    _setup[0] = LCD_FUNCTIONSET | LCD_4BITMODE | LCD_2LINE | LCD_5x8DOTS;
    _setup[1] = LCD_CLEARDISPLAY;
    _setup[2] = LCD_RETURNHOME;
    _setup[3] = LCD_ENTRYMODESET | LCD_ENTRYLEFT | LCD_ENTRYSHIFTDECREMENT;
    _setup[4] = LCD_DISPLAYCONTROL | LCD_DISPLAYON | LCD_CURSOROFF | LCD_BLINKOFF;

    write4bits(_init[0]);
    __delay_cycles(108000);
    write4bits(_init[1]);
    __delay_cycles(108000);
    write4bits(_init[2]);
    __delay_cycles(3600);
    write4bits(_init[3]);

    LCD_Send(_setup[0], mode);
    LCD_Send(_setup[1], mode);
    __delay_cycles(50);
    LCD_Send(_setup[2], mode);
    LCD_Send(_setup[3], mode);
    LCD_Send(_setup[4], mode);
}

#pragma vector = EUSCI_B0_VECTOR
__interrupt void EUSCI_B0_I2C_ISR(void) {
    UCB0TXBUF = TXBUF; // 將我們要傳輸的值放入緩衝區
}
