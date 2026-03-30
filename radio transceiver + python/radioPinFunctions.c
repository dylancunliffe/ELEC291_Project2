/*
* ----------------------------------------------------------------------------
* THE COFFEEWARE LICENSE� (Revision 1
* <ihsan@kehribar.me> wrote this file. As long as you retain this notice you
* can do whatever you want with this stuff. If we meet some day, and you think
* this stuff is worth it, you can buy me a coffee in return.
* -----------------------------------------------------------------------------
* Please define your platform specific functions in this file ...
* -----------------------------------------------------------------------------
*/

#include <EFM8LB1.h>
#include <stdint.h>

// // Pins used by the SPI interface on EFM8LB1:
// #define NRF24_MISO  P1_0 //input
// #define NRF24_SCK   P1_2 //output STAY ---
// #define NRF24_MOSI  P1_3 //output STAY ----
// #define NRF24_CE    P1_4 //output
// #define NRF24_CSN   P1_5 //output 


// Pins used by the SPI interface on EFM8LB1:
 #define NRF24_MISO  P3_2 //input GOOD!
 #define NRF24_SCK   P3_3 //output BAD! ---
 #define NRF24_MOSI  P3_0 //output BAD! ----
 #define NRF24_CE    P3_7 //output
 #define NRF24_CSN   P3_1 //output GOOD!
//#define NRF24_IRQ   P2_6 //input GOOD!


/* ------------------------------------------------------------------------- */
void nrf24_setupPins()
{
    SFRPAGE = 0x20;
    P3MDIN  |= 0b_0000_1111;
    P3MDOUT |= 0b_1000_1011;

    SFRPAGE = 0x00;
    P1MDOUT |= 0b_0001_1100;
    P2MDOUT |= 0b_0000_0000;

    // Must initialize pins before setting push-pull
    NRF24_SCK = 0;   // SCK must idle LOW
    NRF24_MOSI = 0;  // MOSI idles LOW
    NRF24_CE = 0;    // Antenna OFF
    NRF24_CSN = 1;   // Chip Unselected (HIGH)

}
/* ------------------------------------------------------------------------- */
void nrf24_ce_digitalWrite(uint8_t state)
{
    if(state) NRF24_CE=1; else NRF24_CE=0;
}
/* ------------------------------------------------------------------------- */
void nrf24_csn_digitalWrite(uint8_t state)
{
    // The 'volatile' keyword stops the compiler from deleting our delay
    volatile int pause; 
    
    if(state) 
    {
        NRF24_CSN=1; 
        
        // Count to 10 to give the radio >50ns to breathe 
        // between high-speed SPI commands
        for(pause = 0; pause < 10; pause++) {} 
    } 
    else 
    {
        NRF24_CSN=0; 
    }
}
/* ------------------------------------------------------------------------- */
void nrf24_sck_digitalWrite(uint8_t state)
{
    if(state) NRF24_SCK=1; else NRF24_SCK=0;
}
/* ------------------------------------------------------------------------- */
void nrf24_mosi_digitalWrite(uint8_t state)
{
    if(state) NRF24_MOSI=1; else NRF24_MOSI=0;
}
/* ------------------------------------------------------------------------- */
uint8_t nrf24_miso_digitalRead()
{
    return NRF24_MISO;
}
/* ------------------------------------------------------------------------- */
