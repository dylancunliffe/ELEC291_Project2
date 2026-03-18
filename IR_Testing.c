#include <stdio.h>
#include <stdlib.h>
#include <EFM8LB1.h>

// ~C51~  

#define SYSCLK 72000000L
#define BAUDRATE 115200L
#define SARCLK 18000000L
#define IR_RATE 38000L
#define VDD 3.3035 // The measured value of VDD in volts

#define PULSEWAVE_PIN 0

// LCD Pins LCD1 Enable: 2.0 LCD2 Enable: 1.6
#define LCD_RS P1_7
#define LCD_E1 P2_0
#define LCD_E2 P1_6
#define LCD_D4 P1_3
#define LCD_D5 P1_2
#define LCD_D6 P1_1
#define LCD_D7 P1_0
#define CHARS_PER_LINE 16

#define FWD_LED		P2_1
#define BKW_LED		P2_2
#define ECO_LED		P2_3
#define TURN_LED	P2_4
#define AUTO_LED	P2_5

#define DPAD_UP		P0_1
#define DPAD_DOWN	P0_2
#define DPAD_LEFT	P0_3
#define DPAD_RIGHT	P0_4

#define ECO_BUT		P0_5
#define AUTO_BUT	P0_6
#define STOP_BUT	P0_7
#define FLIP_BUT	P3_1

#define IR_PIN		P3_0

char _c51_external_startup (void)
{
	// Disable Watchdog with key sequence
	SFRPAGE = 0x00;
	WDTCN = 0xDE; //First key
	WDTCN = 0xAD; //Second key
  
	VDM0CN=0x80;       // enable VDD monitor
	RSTSRC=0x02|0x04;  // Enable reset on missing clock detector and VDD

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

	// Configure Uart 0
	#if (((SYSCLK/BAUDRATE)/(2L*12L))>0xFFL)
		#error Timer 0 reload value is incorrect because (SYSCLK/BAUDRATE)/(2L*12L) > 0xFF
	#endif
	SCON0 = 0x10;
	TH1 = 0x100-((SYSCLK/BAUDRATE)/(2L*12L));
	TL1 = TH1;      // Init Timer1
	TMOD &= ~0xf0;  // TMOD: timer 1 in 8-bit auto-reload
	TMOD |=  0x20;                       
	TR1 = 1; // START Timer1
	TI = 1;  // Indicate TX0 ready
  	
	return 0;
}

/****************************
 * DELAY(TIMER 3) SECTION	*
 ****************************/

// Uses Timer3 to delay <us> micro-seconds. 
void Timer3us(unsigned char us)
{
	unsigned char i;               // usec counter

	// The input for Timer 3 is selected as SYSCLK by setting T3ML (bit 6) of CKCON0:
	CKCON0 |= 0b_0100_0000;

	TMR3RL = (-(SYSCLK) / 1000000L); // Set Timer3 to overflow in 1us.
	TMR3 = TMR3RL;                 // Initialize Timer3 for first overflow

	TMR3CN0 = 0x04;                 // Sart Timer3 and clear overflow flag
	for (i = 0; i < us; i++)       // Count <us> overflows
	{
		while (!(TMR3CN0 & 0x80));  // Wait for overflow
		TMR3CN0 &= ~(0x80);         // Clear overflow indicator
	}
	TMR3CN0 = 0;                   // Stop Timer3 and clear overflow flag
}

void waitms(unsigned int ms)
{
	unsigned int j;
	unsigned char k;
	for (j = 0; j < ms; j++)
		for (k = 0; k < 4; k++) Timer3us(250);
}

/********************
 * IR SECTION		*
 ********************/

//SEND THIS BEFORE EVERY TRANSMISSION
void Send_Header() {
	IR_PIN = 0;
	waitms(9);  // 9ms Mark
	IR_PIN = 1;
	waitms(4);  // 4.5ms Space
	Timer3us(250);
	Timer3us(250);
}

//sends 8 bits of information over IR
//LSB transmitted first
void Send_byte(unsigned char data_out) {
	int i = 0;
	for (i = 0; i < 8; i++) {
		IR_PIN = 0;
		//wait 560us for mark
		Timer3us(250);
		Timer3us(250);
		Timer3us(60);
		IR_PIN = 1;
		if ((data_out >> i) & 1){
			//wait 1690us for a one
			waitms(1);
			Timer3us(250);
			Timer3us(250);
			Timer3us(190);
		}
		else{
			//wait 560us for a zero
			Timer3us(250);
			Timer3us(250);
			Timer3us(60);
		}
	}
	IR_PIN = 0;
	Timer3us(250);
	Timer3us(250);
	Timer3us(60);
	IR_PIN = 1;
}

//sends 16 bits of information over IR
//LSB transmitted first
void Send_Int(unsigned int data_out) {
	Send_byte((unsigned char)(data_out & 0xFF)); // send least significant byte
	Send_byte((unsigned char)((data_out >> 8) & 0xFF));
}


void main (void)
{
	unsigned int IR_data = 0;

	// Configure P3.0 (GREEN) and P3.1 (RED) as outputs
	SFRPAGE = 0x20;
	P3MDOUT |= 0x03; 			// 0000_0011 
	SFRPAGE = 0x00;

	// Configure pins 2.4, 2.5, and 2.6 for inputs 
	SFRPAGE = 0x20;
	P2MDIN |= 0x70;				// 0111_0000
	SFRPAGE = 0x00;
	
	waitms(500);

	printf("\x1b[2J"); // Clear screen using ANSI escape sequence.
	
	printf("ADC test program\n"
		"File: %s\n"
		"Compiled: %s, %s\n\n",
		__FILE__, __DATE__, __TIME__);

	while(1){
		Send_Header();
		Send_byte(0xAA);
	}  
}	

