#include <EFM8LB1.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>     
#include "nrf24.h"
#include "string.h"

#include "radioPinFunctions.c"
#include "nrf24.c"

#define SYSCLK    72000000L // SYSCLK frequency in Hz
#define BAUDRATE    115200L // Baud rate of UART in bps

// The exact address your STM32 is listening to
uint8_t tx_mac[5] = {0x10, 0x21, 0x32, 0x43, 0x54};
uint32_t sanity_counter = 0;
uint8_t check_ch;
uint8_t incoming_byte;


uint8_t w_key = 0;       // 1 = Pressed, 0 = Released
uint8_t a_key = 0;
uint8_t s_key = 0;
uint8_t d_key = 0;
uint8_t manual_mode = 0; // 1 = Manual, 0 = Auto

// The 32-byte text buffer we will blast over the radio
xdata char tx_buffer[32];


char _c51_external_startup (void)
{
	// Disable Watchdog with 2-byte key sequence
	SFRPAGE = 0x00;
	WDTCN = 0xDE; //First key
	WDTCN = 0xAD; //Second key

	#if (SYSCLK == 48000000L)	
		SFRPAGE = 0x10;
		PFE0CN  = 0x10; // SYSCLK < 50 MHz.
		SFRPAGE = 0x00;
	#elif (SYSCLK == 72000000L)
		SFRPAGE = 0x10;
		PFE0CN  = 0x20; // SYSCLK < 75 MHz.
		SFRPAGE = 0x00;
	#endif
	
	#if (SYSCLK == 12250000L)
		CLKSEL = 0x10;
		CLKSEL = 0x10;
		while ((CLKSEL & 0x80) == 0);
	#elif (SYSCLK == 24500000L)
		CLKSEL = 0x00;
		CLKSEL = 0x00;
		while ((CLKSEL & 0x80) == 0);
	#elif (SYSCLK == 48000000L)	
		// Before setting clock to 48 MHz, must transition to 24.5 MHz first
		CLKSEL = 0x00;
		CLKSEL = 0x00;
		while ((CLKSEL & 0x80) == 0);
		CLKSEL = 0x07;
		CLKSEL = 0x07;
		while ((CLKSEL & 0x80) == 0);
	#elif (SYSCLK == 72000000L)
		// Before setting clock to 72 MHz, must transition to 24.5 MHz first
		CLKSEL = 0x00;
		CLKSEL = 0x00;
		while ((CLKSEL & 0x80) == 0);
		CLKSEL = 0x03;
		CLKSEL = 0x03;
		while ((CLKSEL & 0x80) == 0);
	#else
		#error SYSCLK must be either 12250000L, 24500000L, 48000000L, or 72000000L
	#endif

	P0MDOUT |= 0x10; // Enable UART0 TX as push-pull output
	XBR0     = 0x01; // Enable UART0 on P0.4(TX) and P0.5(RX)                     
	XBR1     = 0X00;
	XBR2     = 0x40; // Enable crossbar and weak pull-ups

	#if (((SYSCLK/BAUDRATE)/(2L*12L))>0xFFL)
		#error Timer 0 reload value is incorrect because ((SYSCLK/BAUDRATE)/(2L*12L))>0xFF
	#endif
	// Configure Uart 0
	SCON0 = 0x10;
	CKCON0 |= 0b_0000_0000 ; // Timer 1 uses the system clock divided by 12.
	TH1 = 0x100-((SYSCLK/BAUDRATE)/(2L*12L));
	TL1 = TH1;      // Init Timer1
	TMOD &= ~0xf0;  // TMOD: timer 1 in 8-bit auto-reload
	TMOD |=  0x20;                       
	TR1 = 1; // START Timer1
	TI = 1;  // Indicate TX0 ready
	
	return 0;
}

void simple_delay_ms(unsigned int ms) 
{
    unsigned int i;
    volatile unsigned int j; // 'volatile' stops the compiler from deleting the loop
    
    for(i = 0; i < ms; i++) 
    {
        // At 24.5MHz, counting to 3000 takes roughly 1 millisecond
        for(j = 0; j < 3000; j++) {} 
    }
}

// ========================================================================
void main (void)
{
    // 1. Double Buffer Setup
    char catch_buffer[6]  = "00000"; // Silently catches incoming bits from Python
    char live_command[6]  = "00000"; // The official state the radio broadcasts
    
    uint8_t uart_idx = 0;

    simple_delay_ms(100);
    
    // Initialize the Radio Hardware
    nrf24_init(); 
    nrf24_config(78, 32); 
    nrf24_tx_address(tx_mac); 

    nrf24_readRegister(0x05, &check_ch, 1);
    printf("Diagnostic - Configured Channel: %d\r\n", check_ch);
    printf("EFM8 Ready: Continuously broadcasting...\r\n");

    while(1)
    {
        // =================================================================
        // TASK 1: SILENTLY CATCH PYTHON DATA (NON-BLOCKING)
        // We use a 'while' here instead of 'if' to instantly drain the serial 
        // buffer before the EFM8 has a chance to hit any delays.
        // =================================================================
        while (RI == 1) 
        {
            incoming_byte = SBUF; 
            RI = 0; // Clear the hardware flag immediately
            
            // Only catch 1s and 0s (Ignores keyboard noise)
            if (incoming_byte == '0' || incoming_byte == '1') 
            {
                if (uart_idx < 5) {
                    catch_buffer[uart_idx] = incoming_byte; 
                    uart_idx++;                             
                }

                // If we caught all 5 bits from Python, update the live command!
                if (uart_idx == 5) 
                {
                    catch_buffer[5] = '\0'; // Cap the text string safely
                    
                    // Copy the caught string over to the live broadcast string
                    strcpy(live_command, catch_buffer);
                    
                    // Reset the trap
                    uart_idx = 0; 
                }
            }
            // Resync the trap when Python sends its newline character
            else if (incoming_byte == '\n' || incoming_byte == '\r') 
            {
                uart_idx = 0; 
            }
        }

        // =================================================================
        // TASK 2: CONSTANTLY BLAST THE RADIO
        // This runs independently, ensuring the STM32 always has a heartbeat
        // =================================================================
        
        // Format the payload with the start marker (e.g., "*10001")
        memset(tx_buffer, 0, 32);
        sprintf(tx_buffer, "*%s", live_command); 

        // Blast it over the airwaves!
        nrf24_send((uint8_t*)tx_buffer);
        while(nrf24_isSending()); // Wait for hardware to physically finish

        // Print to the terminal (Your Python script will catch this and display it!)
        if(nrf24_lastMessageStatus() == NRF24_TRANSMISSON_OK) {
            printf("TX SUCCESS: %s\r\n", tx_buffer);
        } else {
            printf("TX DROPPED: %s\r\n", tx_buffer);
        }

        // A 50ms delay limits the radio to 20 transmissions a second. 
        // This prevents the EFM8 from jamming the airwaves, while still feeling instant.
        simple_delay_ms(50); 
    }
}