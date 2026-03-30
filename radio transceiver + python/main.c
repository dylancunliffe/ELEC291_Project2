#include <EFM8LB1.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>     
#include "nrf24.h"
#include "string.h"

#include "radioPinFunctions.c"
#include "nrf24.c"

#define SYSCLK    72000000L 
#define BAUDRATE    115200L 

uint8_t tx_mac[5] = {0x10, 0x21, 0x32, 0x43, 0x54};
uint8_t check_ch;
xdata char tx_buffer[32];

// ========================================================================
// VOLATILE GLOBALS (Shared between background Interrupt and Main Loop)
// ========================================================================
volatile char catch_buffer[6]  = "00000"; 
volatile char live_command[6]  = "00000"; // Defaults to all 0s!
volatile uint8_t uart_idx = 0;

// NEW: A hardware flag to safely manage printf and the Interrupt
volatile bit tx_ready = 1; 

// ========================================================================
// OVERRIDE PRINTF: This teaches printf how to work WITH the interrupt
// ========================================================================
void putchar(char c) 
{
    while (!tx_ready); // Wait until the hardware finishes the last letter
    tx_ready = 0;      // Lock the port
    SBUF = c;          // Send the new letter
}

// ========================================================================
// BACKGROUND UART INTERRUPT (Runs automatically)
// ========================================================================
void UART0_ISR (void) interrupt 4
{
    uint8_t incoming_byte;

    // 1. Did we RECEIVE a byte from Python?
    if (RI == 1) 
    {
        incoming_byte = SBUF; 
        RI = 0; 
        
        // Catch only 1s and 0s
        if (incoming_byte == '0' || incoming_byte == '1') 
        {
            if (uart_idx < 5) {
                catch_buffer[uart_idx] = incoming_byte; 
                uart_idx++;                             
            }

            // If we caught all 5 bits (e.g., "11000"), update instantly!
            if (uart_idx == 5) 
            {
                catch_buffer[5] = '\0'; 
                strcpy((char*)live_command, (char*)catch_buffer); 
                uart_idx = 0; 
            }
        }
        else if (incoming_byte == '\n' || incoming_byte == '\r') 
        {
            uart_idx = 0; 
        }
    }

    // 2. Did we finish TRANSMITTING a printf character?
    if (TI == 1) 
    {
        TI = 0; 
        tx_ready = 1; // Unlock the port so putchar() can send the next letter!
    }
}

// ========================================================================
// STARTUP CODE
// ========================================================================
char _c51_external_startup (void)
{
    SFRPAGE = 0x00;
    WDTCN = 0xDE; 
    WDTCN = 0xAD; 

    #if (SYSCLK == 72000000L)
        SFRPAGE = 0x10;
        PFE0CN  = 0x20; 
        SFRPAGE = 0x00;
    #endif
    
    CLKSEL = 0x00;
    CLKSEL = 0x00;
    while ((CLKSEL & 0x80) == 0);
    CLKSEL = 0x03;
    CLKSEL = 0x03;
    while ((CLKSEL & 0x80) == 0);

    P0MDOUT |= 0x10; 
    XBR0     = 0x01;                    
    XBR1     = 0X00;
    XBR2     = 0x40; 
    
    SCON0 = 0x10;
    CKCON0 |= 0b_0000_0000 ; 
    TH1 = 0x100-((SYSCLK/BAUDRATE)/(2L*12L));
    TL1 = TH1;      
    TMOD &= ~0xf0;  
    TMOD |=  0x20;                       
    TR1 = 1; 
    TI = 1;  
    
    return 0;
}

void simple_delay_ms(unsigned int ms) 
{
    unsigned int i;
    volatile unsigned int j; 
    for(i = 0; i < ms; i++) {
        for(j = 0; j < 3000; j++) {} 
    }
}

// ========================================================================
// MAIN LOOP
// ========================================================================
void main (void)
{
    char safe_copy[6]; 

    simple_delay_ms(100);
    
    nrf24_init(); 
    nrf24_config(78, 32); 
    nrf24_tx_address(tx_mac); 

    // ENABLE GLOBAL AND UART INTERRUPTS
    EA = 1;   
    ES0 = 1;  

    // YES, THIS WILL PRINT NOW!
    nrf24_readRegister(0x05, &check_ch, 1);
    printf("Diagnostic - Configured Channel: %d\r\n", check_ch);
    printf("EFM8 Ready: Interrupts ACTIVE. Continuously broadcasting...\r\n");

    while(1)
    {
        // 1. SAFELY GRAB THE LATEST BITS FROM THE BACKGROUND INTERRUPT
        EA = 0; 
        strcpy(safe_copy, (char*)live_command);
        EA = 1; 

        // 2. CONSTANTLY BLAST THE RADIO
        memset(tx_buffer, 0, 32);
        sprintf(tx_buffer, "*%s", safe_copy); // Starts as "*00000"

        nrf24_send((uint8_t*)tx_buffer);
        while(nrf24_isSending()); 

        // 3. TERMINAL FEEDBACK
        if(nrf24_lastMessageStatus() == NRF24_TRANSMISSON_OK) {
            printf("TX SUCCESS: %s\r\n", tx_buffer);
        } else {
            printf("TX DROPPED: %s\r\n", tx_buffer);
        }

        simple_delay_ms(50); 
    }
}