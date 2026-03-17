#include <stdio.h>
#include <stdlib.h>
#include <EFM8LB1.h>

// ~C51~  

#define SYSCLK 72000000L
#define BAUDRATE 115200L
#define SARCLK 18000000L
#define IR_RATE 38000L
#define VDD 3.3035 // The measured value of VDD in volts
#define TIC_MARK 560
#define TIC_ZERO 560
#define TIC_ONE 1690

#define PULSEWAVE_PIN 0

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

/********************************
 * 38kHz WAVE(TIMER2) SECTION	*
 ********************************/

void InitPCA_38kHz(void)
{
	//Setup PCA Clock (SYSCLK / 12)
	PCA0MD = 0x00;   // CPS2:CPS0 = 000 (SYSCLK / 12)

	// Configure Module 0 for Frequency Output Mode
	// TOG0 = 1 (Toggle), PWM0 = 1 (Enable Frequency Output)
	PCA0CPM0 = 0x46;
	PCA0CPH0 = SYSCLK/(24L * IR_RATE); // Set the frequency value

	// Enable Crossbar to route PCA0 to a physical pin
	XBR1 |= 0x01 << PULSEWAVE_PIN;    // Route CEX0 to pin LOGIC NOT FINALIZED (P0.0 for now)
	XBR2 |= 0x40;    // Enable Crossbar

	// Start the PCA timer
	PCA0CN0 |= 0x40; // CR = 1
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
 * LCD SECTION		*
 ********************/
void LCD_pulse(int display)
{
	switch (display)
	{
	case 1:
		LCD_E1 = 1;
		Timer3us(40);
		LCD_E1 = 0;
		break;
	default:
		LCD_E2 = 1;
		Timer3us(40);
		LCD_E2 = 0;
		break;
	}
}

void LCD_byte(unsigned char x, int display)
{
	// The accumulator in the C8051Fxxx is bit addressable!
	ACC = x; //Send high nible
	LCD_D7 = ACC_7;
	LCD_D6 = ACC_6;
	LCD_D5 = ACC_5;
	LCD_D4 = ACC_4;
	LCD_pulse(display);
	Timer3us(40);
	ACC = x; //Send low nible
	LCD_D7 = ACC_3;
	LCD_D6 = ACC_2;
	LCD_D5 = ACC_1;
	LCD_D4 = ACC_0;
	LCD_pulse(display);
}

void WriteData(unsigned char x, int display)
{
	LCD_RS = 1;
	LCD_byte(x, display);
	waitms(2);
}

void WriteCommand(unsigned char x, int display)
{
	LCD_RS = 0;
	LCD_byte(x, display);
	waitms(5);
}

void LCD_4BIT(int display)
{
	// Resting state of LCDs' enable is zero
	LCD_E1 = 0;
	LCD_E2 = 0;
	// LCD_RW=0; // We are only writing to the LCD in this program
	waitms(20);
	// First make sure the LCD is in 8-bit mode and then change to 4-bit mode
	WriteCommand(0x33, display);
	WriteCommand(0x33, display);
	WriteCommand(0x32, display); // Change to 4-bit mode

	// Configure the LCD
	WriteCommand(0x28, display);
	WriteCommand(0x0c, display);
	WriteCommand(0x01, display); // Clear screen command (takes some time)
	waitms(20); // Wait for clear screen command to finsih.
}

void LCDprint(char* string, unsigned char line, bit clear, int display)
{
	int j;

	WriteCommand(line == 2 ? 0xc0 : 0x80, display);
	waitms(5);
	for (j = 0; string[j] != 0; j++)	WriteData(string[j], display);// Write the message
	if (clear) for (; j < CHARS_PER_LINE; j++) WriteData(' ', display); // Clear the rest of the line
}

int getsn(char* buff, int len)
{
	int j;
	char c;

	for (j = 0; j < (len - 1); j++)
	{
		c = getchar();
		if ((c == '\n') || (c == '\r'))
		{
			buff[j] = 0;
			return j;
		}
		else
		{
			buff[j] = c;
		}
	}
	buff[j] = 0;
	return len;
}

/********************
 * ADC SECTION		*
 ********************/
void InitADC(void)
{
	SFRPAGE = 0x00;
	ADEN = 0; // Disable ADC

	ADC0CN1 =
		(0x2 << 6) | // 0x0: 10-bit, 0x1: 12-bit, 0x2: 14-bit
		(0x0 << 3) | // 0x0: No shift. 0x1: Shift right 1 bit. 0x2: Shift right 2 bits. 0x3: Shift right 3 bits.		
		(0x0 << 0); // Accumulate n conversions: 0x0: 1, 0x1:4, 0x2:8, 0x3:16, 0x4:32

	ADC0CF0 =
		((SYSCLK / SARCLK) << 3) | // SAR Clock Divider. Max is 18MHz. Fsarclk = (Fadcclk) / (ADSC + 1)
		(0x0 << 2); // 0:SYSCLK ADCCLK = SYSCLK. 1:HFOSC0 ADCCLK = HFOSC0.

	ADC0CF1 =
		(0 << 7) | // 0: Disable low power mode. 1: Enable low power mode.
		(0x1E << 0); // Conversion Tracking Time. Tadtk = ADTK / (Fsarclk)

	ADC0CN0 =
		(0x0 << 7) | // ADEN. 0: Disable ADC0. 1: Enable ADC0.
		(0x0 << 6) | // IPOEN. 0: Keep ADC powered on when ADEN is 1. 1: Power down when ADC is idle.
		(0x0 << 5) | // ADINT. Set by hardware upon completion of a data conversion. Must be cleared by firmware.
		(0x0 << 4) | // ADBUSY. Writing 1 to this bit initiates an ADC conversion when ADCM = 000. This bit should not be polled to indicate when a conversion is complete. Instead, the ADINT bit should be used when polling for conversion completion.
		(0x0 << 3) | // ADWINT. Set by hardware when the contents of ADC0H:ADC0L fall within the window specified by ADC0GTH:ADC0GTL and ADC0LTH:ADC0LTL. Can trigger an interrupt. Must be cleared by firmware.
		(0x0 << 2) | // ADGN (Gain Control). 0x0: PGA gain=1. 0x1: PGA gain=0.75. 0x2: PGA gain=0.5. 0x3: PGA gain=0.25.
		(0x0 << 0); // TEMPE. 0: Disable the Temperature Sensor. 1: Enable the Temperature Sensor.

	ADC0CF2 =
		(0x0 << 7) | // GNDSL. 0: reference is the GND pin. 1: reference is the AGND pin.
		(0x1 << 5) | // REFSL. 0x0: VREF pin (external or on-chip). 0x1: VDD pin. 0x2: 1.8V. 0x3: internal voltage reference.
		(0x1F << 0); // ADPWR. Power Up Delay Time. Tpwrtime = ((4 * (ADPWR + 1)) + 2) / (Fadcclk)

	ADC0CN2 =
		(0x0 << 7) | // PACEN. 0x0: The ADC accumulator is over-written.  0x1: The ADC accumulator adds to results.
		(0x0 << 0); // ADCM. 0x0: ADBUSY, 0x1: TIMER0, 0x2: TIMER2, 0x3: TIMER3, 0x4: CNVSTR, 0x5: CEX5, 0x6: TIMER4, 0x7: TIMER5, 0x8: CLU0, 0x9: CLU1, 0xA: CLU2, 0xB: CLU3

	ADEN = 1; // Enable ADC
}

void InitPinADC (unsigned char portno, unsigned char pinno)
{
	unsigned char mask;
	
	mask=1<<pinno;

	SFRPAGE = 0x20;
	switch (portno)
	{
		case 0:
			P0MDIN &= (~mask); // Set pin as analog input
			P0SKIP |= mask; // Skip Crossbar decoding for this pin
		break;
		case 1:
			P1MDIN &= (~mask); // Set pin as analog input
			P1SKIP |= mask; // Skip Crossbar decoding for this pin
		break;
		case 2:
			P2MDIN &= (~mask); // Set pin as analog input
			P2SKIP |= mask; // Skip Crossbar decoding for this pin
		break;
		default:
		break;
	}
	SFRPAGE = 0x00;
}

unsigned int ADC_at_Pin(unsigned char pin)
{
	ADC0MX = pin;   // Select input from pin
	ADINT = 0;
	ADBUSY = 1;     // Convert voltage at the pin
	while (!ADINT); // Wait for conversion to complete
	return (ADC0);
}

float Volts_at_Pin(unsigned char pin)
{
	 return ((ADC_at_Pin(pin)*VDD)/0b_0011_1111_1111_1111);
}
/********************
 * IR SECTION		*
 ********************/

//SEND THIS BEFORE EVERY TRANSMISSION
void Send_Header() {
	PCA0CPM0 |= 0x42;
	Timer3us(9000);  // 9ms Mark
	PCA0CPM0 &= ~0x42;
	Timer3us(4500);  // 4.5ms Space
}

//sends 8 bits of information over IR
//LSB transmitted first
void Send_byte(unsigned char data) {
	int i = 0;
	for (i = 0; i < 8; i++) {
		PCA0CPM0 |= 0x42;
		Timer3us(TIC_MARK);
		PCA0CPM0 &= ~0x42;
		if ((data >> i) & 1)
			Timer3us(TIC_ONE);
		else
			Timer3us(TIC_ZERO);
	}
	PCA0CPM0 |= 0x42;
	Timer3us(TIC_MARK);
	PCA0CPM0 &= ~0x42;
}

//sends 16 bits of information over IR
//LSB transmitted first
void Send_Int(unsigned int data) {
	Send_byte((unsigned char)(data & 0xFF)); // send least significant byte
	Send_byte((unsigned char)((data >> 8) & 0xFF));
}

void main (void)
{
	printf("\x1b[2J"); // Clear screen using ANSI escape sequence.
	
	printf ("ADC test program\n"
	        "File: %s\n"
	        "Compiled: %s, %s\n\n",
	        __FILE__, __DATE__, __TIME__);

	LCD_4BIT(1);
	LCD_4BIT(2);
	
	InitPinADC(2, 2); // Configure P2.2 as analog input
	InitPinADC(2, 3); // Configure P2.3 as analog input
    InitADC();
	InitPCA_38kHz();

	LCDprint("LCD1", 1, 1, 1); // Header on LCD 1
	LCDprint("LCD2", 1, 1, 2); // Header on LCD 2

	while(1)
	{

	}  
}	

