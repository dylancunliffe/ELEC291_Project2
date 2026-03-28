#include <stdio.h>
#include <stdlib.h>
#include <EFM8LB1.h>
#include <string.h>

// ~C51~  

#define SYSCLK 72000000L
#define BAUDRATE 115200L
#define SARCLK 18000000L
#define ADC_MAX 14100
#define VDD 3.3035 // The measured value of VDD in volts
#define SMB_FREQUENCY  400000L   // I2C SCL clock rate

// LCD Pins LCD1 Enable: 2.0 LCD2 Enable: 1.6
#define LCD_RS P1_1
#define LCD_E1 P1_0
#define LCD_E2 P0_7
#define LCD_D4 P0_6
#define LCD_D5 P0_3
#define LCD_D6 P0_2
#define LCD_D7 P0_1
#define CHARS_PER_LINE 16

#define FWD_LED		P3_2
#define BKW_LED		P3_3
#define ECO_LED		P3_7
#define FLIP_LED	P3_1
#define AUTO_LED	P2_5

#define DPAD_UP		P1_5
#define DPAD_DOWN	P1_6
#define DPAD_LEFT	P1_7
#define DPAD_RIGHT	P2_0

#define ECO_BUT		P3_0
#define AUTO_BUT	P1_3
#define STOP_BUT	P1_2
#define FLIP_BUT	P1_4

#define IR_PIN		P0_0

unsigned char key1, key2;
volatile int offset;

typedef enum {
	PATH1 = 0,
	PATH2 = 1,
	PATH3 = 2,
	PATH4 = 3
} path_select;

char _c51_external_startup(void)
{
	// Disable Watchdog with key sequence
	SFRPAGE = 0x00;
	WDTCN = 0xDE; //First key
	WDTCN = 0xAD; //Second key

	VDM0CN = 0x80;       // enable VDD monitor
	RSTSRC = 0x02 | 0x04;  // Enable reset on missing clock detector and VDD

#if (SYSCLK == 48000000L)	
	SFRPAGE = 0x10;
	PFE0CN = 0x10; // SYSCLK < 50 MHz.
	SFRPAGE = 0x00;
#elif (SYSCLK == 72000000L)
	SFRPAGE = 0x10;
	PFE0CN = 0x20; // SYSCLK < 75 MHz.
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
	XBR0 = 0x01; // Enable UART0 on P0.4(TX) and P0.5(RX)                     
	XBR1 = 0X00;
	XBR2 = 0x40; // Enable crossbar and weak pull-ups

	// Configure Uart 0
#if (((SYSCLK/BAUDRATE)/(2L*12L))>0xFFL)
#error Timer 0 reload value is incorrect because (SYSCLK/BAUDRATE)/(2L*12L) > 0xFF
#endif
	SCON0 = 0x10;
	TH1 = 0x100 - ((SYSCLK / BAUDRATE) / (2L * 12L));
	TL1 = TH1;      // Init Timer1
	TMOD &= ~0xf0;  // TMOD: timer 1 in 8-bit auto-reload
	TMOD |= 0x20;
	TR1 = 1; // START Timer1
	TI = 1;  // Indicate TX0 ready

	// Configure Timer 0 as the I2C clock source
	CKCON0 |= 0b_0000_0100; // Timer0 clock source = SYSCLK
	TMOD &= 0xf0;  // Mask out timer 1 bits
	TMOD |= 0x02;  // Timer0 in 8-bit auto-reload mode
	// Timer 0 configured to overflow at 1/3 the rate defined by SMB_FREQUENCY
	TL0 = TH0 = 256 - (SYSCLK / SMB_FREQUENCY / 3);
	TR0 = 1; // Enable timer 0

	// Configure and enable SMBus
	SMB0CF = 0b_0101_1100; //INH | EXTHOLD | SMBTOE | SMBFTE ;
	//SMB0CF = 0b_0100_0100; //INH | EXTHOLD | SMBTOE | SMBFTE ;
	SMB0CF |= 0b_1000_0000;  // Enable SMBus

	SFRPAGE = 0x10;
	TMR3CN1 |= 0b_0110_0000; // Timer 3 will only reload on overflow events
	SFRPAGE = 0x00;

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

void Send_Footer() {
	IR_PIN = 0;
	Timer3us(250);
	Timer3us(250);
	Timer3us(60);
	IR_PIN = 1;
}

//sends 8 bits of information over IR
//MSB transmitted first
void Send_byte(unsigned char data_out) {
	int i = 7;
	for (i = 7; i >= 0; i--) {
		IR_PIN = 0;
		//wait 560us for mark
		Timer3us(250);
		Timer3us(250);
		Timer3us(60);
		IR_PIN = 1;
		if ((data_out >> i) & 1) {
			//wait 1690us for a one
			waitms(1);
			Timer3us(250);
			Timer3us(250);
			Timer3us(190);
		}
		else {
			//wait 560us for a zero
			Timer3us(250);
			Timer3us(250);
			Timer3us(60);
		}
	}
}

//sends 16 bits of information over IR
//MSB transmitted first
void Send_16bit(unsigned int data_out) {
	Send_Header();
	Send_byte((unsigned char)((data_out >> 8) & 0xFF));
	Send_byte((unsigned char)((data_out >> 0) & 0xFF));
	//send inverted data to check for errors
	Send_byte((unsigned char)(~(data_out >> 8) & 0xFF));
	Send_byte((unsigned char)(~(data_out >> 0) & 0xFF));
	Send_Footer();
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

void InitPinADC(unsigned char portno, unsigned char pinno)
{
	unsigned char mask;

	mask = 1 << pinno;

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

unsigned char Arrows(unsigned int input_adc, int offset, char* string) {
	unsigned char degree = 0x03;
	unsigned int frac = ADC_MAX / 5;

	//clear all arrows
	string[offset - 1] = ' ';
	string[offset - 2] = ' ';
	string[offset - 3] = ' ';
	string[offset + 1] = ' ';
	string[offset + 2] = ' ';
	string[offset + 3] = ' ';

	if (input_adc > frac * 3) {
		string[offset + 1] = '>';
		degree = 0x04;
		if (input_adc > frac * 4) {
			string[offset + 2] = '>';
			degree = 0x04;
			if (input_adc >= ADC_MAX) {
				string[offset + 3] = '>';
				degree = 0x04;
			}
		}
	}

	if (input_adc < frac * 2) {
		string[offset - 1] = '<';
		degree = 0x02;
		if (input_adc < frac) {
			string[offset - 2] = '<';
			degree = 0x01;
			if (input_adc <= 0) {
				string[offset - 3] = '<';
				degree = 0x00;
			}
		}
	}

	return degree;
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
 * I2C SECTION		*
 ********************/

void Wait_SI(void)
{
	unsigned int I2C_t = 5000;
	while ((!SI) && (I2C_t > 0)) I2C_t--;
}

void Wait_STO(void)
{
	unsigned int I2C_t = 5000;
	while ((STO) && (I2C_t > 0)) I2C_t--;
}

void I2C_write(unsigned char output_data)
{
	SMB0DAT = output_data; // Put data into buffer
	SI = 0;  // Proceed with write
	Wait_SI(); // Wait until done with send
}

unsigned char I2C_read(bit ack)
{
	ACK = ack;
	SI = 0; // Proceed with read
	Wait_SI(); // Wait until done with read
	return SMB0DAT;
}

void I2C_start(void)
{
	ACK = 0;
	STO = 0;
	STA = 1; // Send I2C start
	SI = 0; // Proceed with start
	Wait_SI(); // Wait until done with start
}

void I2C_stop(void)
{
	ACK = 0;
	STA = 0;
	STO = 1; // Perform I2C stop
	SI = 0; // Proceed with stop
	Wait_STO(); // Wait until done with stop
	STO = 0;
}

bit i2c_read_addr8_data8(unsigned char address, unsigned char* value)
{
	I2C_start();
	I2C_write(0x52); // Write address
	I2C_write(address);
	I2C_stop();

	I2C_start();
	I2C_write(0x53); // Read address
	*value = I2C_read(1);
	I2C_stop();

	return 1;
}

bit i2c_read_addr8_data16(unsigned char address, unsigned int* value)
{
	I2C_start();
	I2C_write(0x52); // Write address
	I2C_write(address);
	I2C_stop();

	I2C_start();
	I2C_write(0x53); // Read address
	*value = I2C_read(0) * 256;
	*value += I2C_read(1);
	I2C_stop();

	return 1;
}

bit i2c_write_addr8_data8(unsigned char address, unsigned char value)
{
	I2C_start();
	I2C_write(0x52); // Write address
	I2C_write(address);
	I2C_write(value);
	I2C_stop();

	return 1;
}

void main(void){

	unsigned int IR_data = 0;
	/*************************************************************************************
	* bits | Description
	* ------------------------------------------------------------------------------------
	*  0-2 | direction data, 0x00 = reverse max, 0x03 = nothing, 0x06 = forward max
	*  3-5 | rotation data,  0x00 = CCW max, 0x03 = nothing, 0x06 = CW max
	*  6-7 | path data (path 0,1,2,3)
	*   8  | eco
	*   9  | auto
	*   10 | rotate 180
	*   11 | stop
	* 10-15| unassigned
	**************************************************************************************/

	path_select path = PATH1;
	bit dpad_up_dbc = 0;
	bit dpad_up_edge = 0;
	bit dpad_down_dbc = 0;
	bit dpad_down_edge = 0;
	bit dpad_right_dbc = 0;
	bit dpad_right_edge = 0;
	bit dpad_left_dbc = 0;
	bit dpad_left_edge = 0;

	bit eco_dbc = 0;
	bit eco = 0;
	bit aut_dbc = 0;
	bit aut = 0;
	bit flip_dbc = 0;
	bit flip = 0;
	bit stop_dbc = 0;
	bit stop = 0;

	unsigned int direction = 0; //14 bit adc reading
	unsigned int rotation = 0;  //14 bit adc reading
	char screen2_ln1[CHARS_PER_LINE + 1] = "   D        R   ";
	char screen2_ln2[CHARS_PER_LINE + 1] = "Speed: X.XXX m/s";
	volatile unsigned int i2c_buff;
	const unsigned char radar_addr = 0x00; //temporary until if get the real address

	/* Configure I/0 for P1 register:
	* 0.0 - IR Transmit	| out
	* 0.1 - LCD D7		| out
	* 0.2 - LCD D6		| out
	* 0.3 - LCD D5		| out
	* 0.4 - TX			| N/A
	* 0.5 - RX			| N/A
	* 0.6 - LCD D4		| out
	* 0.7 - LCD Enable2 | out
	*/
	SFRPAGE = 0x20;
	P0MDOUT |= 0xCF; 			// 1100_1111 
	SFRPAGE = 0x00;

	/* Configure I/0 for P1 register:
	* 1.0 - LCD Enable2 | out
	* 1.1 - LDC RS		| out
	* 1.2 - Stop button | in
	* 1.3 - Auto button | in
	* 1.4 - Flip button | in
	* 1.5 - D-pad up	| in
	* 1.6 - D-pad down	| in
	* 1.7 - D-pad left	| in
	*/
	SFRPAGE = 0x20;
	P1MDOUT |= 0x03; 			// 0000_0011 
	SFRPAGE = 0x00;
	SFRPAGE = 0x20;
	P1MDIN  |= 0xFC; 			// 1111_1100 
	SFRPAGE = 0x00;

	/* Configure I/0 for P1 register:
	* 2.0 - D-pad right	| in
	* 2.1 - Hatpic		| out
	* 2.2 - Analog		| N/A
	* 2.3 - IR Recieve	| in
	* 2.4 - Stop LED	| out
	* 2.5 - Auto LED	| out
	* 2.6 - Analog		| N/A
	*/
	SFRPAGE = 0x20;
	P2MDOUT |= 0x32; 			// 0011_0010 
	SFRPAGE = 0x00;
	SFRPAGE = 0x20;
	P2MDIN  |= 0x09; 			// 0000_1001 
	SFRPAGE = 0x00;

	/* Configure I/0 for P3 register:
	* 3.0 - Eco button		| in
	* 3.1 - Turn LED		| out
	* 3.2 - Forward LED		| out
	* 3.3 - Reverse LED		| out
	* 3.7 - Eco LED			| out
	*/
	SFRPAGE = 0x20;
	P3MDOUT |= 0x8E; 			// 1000_1110 
	SFRPAGE = 0x00;
	SFRPAGE = 0x20;
	P3MDIN  |= 0x01; 			// 0000_0001 
	SFRPAGE = 0x00;

	LCD_4BIT(1);
	LCD_4BIT(2);
	InitADC();
	waitms(500);
	InitPinADC(2, 6);
	InitPinADC(2, 2);

	LCDprint("LCD2", 1, 1, 1);
	LCDprint("LCD1", 1, 1, 2);

	while (1) {
		//function buttons detection
		if (eco_dbc == 1 && !ECO_BUT)
			eco = eco ? 0 : 1;
		eco_dbc = ECO_BUT;

		if (aut_dbc == 1 && !AUTO_BUT)
			aut = aut ? 0 : 1;
		aut_dbc = AUTO_BUT;

		if (flip_dbc == 1 && !FLIP_BUT)
			flip = flip ? 0 : 1;
		flip_dbc = FLIP_BUT;

		if (stop_dbc == 1 && !STOP_BUT)
			stop = stop ? 0 : 1;
		stop_dbc = STOP_BUT;

		//dpad edge detection
		if (dpad_up_dbc == 1 && !DPAD_UP)
			dpad_up_edge = 1;
		else
			dpad_up_edge = 0;
		dpad_up_dbc = DPAD_UP;

		if (dpad_down_dbc == 1 && !DPAD_DOWN)
			dpad_down_edge = 1;
		else
			dpad_down_edge = 0;
		dpad_down_dbc = DPAD_DOWN;

		if (dpad_right_dbc == 1 && !DPAD_RIGHT)
			dpad_right_edge = 1;
		else
			dpad_right_edge = 0;
		dpad_right_dbc = DPAD_RIGHT;

		if (dpad_left_dbc == 1 && !DPAD_LEFT)
			dpad_left_edge = 1;
		else
			dpad_left_edge = 0;
		dpad_left_dbc = DPAD_LEFT;

		ECO_LED = eco;
		AUTO_LED = aut;
		FLIP_LED = flip;
		BKW_LED = stop;

		IR_data = (IR_data & ~(1 << 8))  | (eco  <<  8);
		IR_data = (IR_data & ~(1 << 9))  | (aut  <<  9);
		IR_data = (IR_data & ~(1 << 10)) | (flip  << 10);
		IR_data = (IR_data & ~(1 << 11)) | (stop << 11);

		direction = ADC_at_Pin(QFP32_MUX_P2_6);
		rotation = ADC_at_Pin(QFP32_MUX_P2_2);
		printf("Direction: %u, Rotation: %u\n", direction, rotation);
		i2c_read_addr8_data16(radar_addr, &i2c_buff);

		if (i2c_buff < 10000) {
			screen2_ln2[7]  = (char)((i2c_buff / 1000) + '0');
			screen2_ln2[9]  = (char)(((i2c_buff / 100) % 10) + '0');
			screen2_ln2[10] = (char)(((i2c_buff / 10) % 10) + '0');
			screen2_ln2[11] = (char)((i2c_buff) % 10 + '0');
		}

		//Screen 2 GUI
		IR_data = (IR_data & ~0x0007) | Arrows(direction, 3, screen2_ln1);
		IR_data = (IR_data & ~0x0038) | (Arrows(rotation, 12, screen2_ln1) << 3);

		//Screen 1 GUI
		switch (path) {
		case PATH1:
			LCDprint(">Path1  Path2", 1, 1, 1);
			LCDprint(" Path3  Path4", 2, 1, 1);
			if (dpad_down_edge || dpad_up_edge)
				path = PATH3;
			else if (dpad_left_edge || dpad_right_edge)
				path = PATH2;
			break;
		case PATH2:
			LCDprint(" Path1 >Path2", 1, 1, 1);
			LCDprint(" Path3  Path4", 2, 1, 1);
			if (dpad_down_edge || dpad_up_edge)
				path = PATH4;
			else if (dpad_left_edge || dpad_right_edge)
				path = PATH1;
			break;
		case PATH3:
			LCDprint(" Path1  Path2", 1, 1, 1);
			LCDprint(">Path3  Path4", 2, 1, 1);
			if (dpad_down_edge || dpad_up_edge)
				path = PATH1;
			else if (dpad_left_edge || dpad_right_edge)
				path = PATH4;
			break;
		case PATH4:
			LCDprint(" Path1  Path2", 1, 1, 1);
			LCDprint(" Path3 >Path4", 2, 1, 1);

			if (dpad_down_edge || dpad_up_edge)
				path = PATH2;
			else if (dpad_left_edge || dpad_right_edge)
				path = PATH3;
			break;
		default:
			LCDprint("ERROR", 1, 1, 1);
			LCDprint("ERROR", 2, 1, 1);
		}
		IR_data = (IR_data & ~(3 << 6)) | (path << 6);
		
		LCDprint(screen2_ln1, 1, 1, 2);
		LCDprint(screen2_ln2, 2, 1, 2);

		Send_16bit(IR_data);
	}
}

