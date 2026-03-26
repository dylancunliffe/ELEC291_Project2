#include <stdio.h>
#include <stdlib.h>
#include <EFM8LB1.h>
#include <string.h>

// ~C51~  

#define SYSCLK 72000000L
#define BAUDRATE 115200L
#define VDD 3.3035 // The measured value of VDD in volts
#define  SMB_FREQUENCY  400000L   // I2C SCL clock rate

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

float Volts_at_Pin(unsigned char pin)
{
	return ((ADC_at_Pin(pin) * VDD) / 0b_0011_1111_1111_1111);
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

unsigned char Arrows(float input, int offset, char* string) {
	unsigned char degree = 0x03;
	if (input < VDD * 0.429) {
		string[offset - 1] = '<';
		degree = 0x02;
		if (input < VDD * 0.286) {
			string[offset - 2] = '<';
			degree = 0x01;
			if (input < VDD * 0.143) {
				string[offset - 3] = '<';
				degree = 0x00;
			}
			else
				string[offset - 3] = ' ';
		}
		else
			string[offset - 2] = ' ';
	}
	else
		string[offset - 1] = ' ';

	if (input > VDD * 0.571) {
		string[offset + 1] = '>';
		degree = 0x04;
		if ( input > VDD * 0.714) {
			string[offset + 2] = '>';
			degree = 0x05;
			if (input > VDD * 0.857) {
				string[offset + 3] = '>';
				degree = 0x06;
			}
			else
				string[offset + 3] = ' ';
		}
		else
			string[offset + 2] = ' ';
	}
	else
		string[offset + 1] = ' ';

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
	bit eco_dbc = 0;
	bit eco = 0;
	bit aut_dbc = 0;
	bit aut = 0;
	bit rot_dbc = 0;
	bit rot = 0;
	bit stop_dbc = 0;
	bit stop = 0;

	float direction = 0;
	float rotation = 0;
	char screen2_ln1[CHARS_PER_LINE + 1] = "   D        R   ";
	char screen2_ln2[CHARS_PER_LINE + 1] = "Speed: X.XXX m/s";
	volatile unsigned int i2c_buff;
	const unsigned char radar_addr = 0x00; //temporary until if get the real address
	unsigned int refresh_acc = 0;

	// Configure P3.0 (GREEN) and P3.1 (RED) as outputs
	SFRPAGE = 0x20;
	P3MDOUT |= 0x03; 			// 0000_0011 
	SFRPAGE = 0x00;

	// Configure pins 2.4, 2.5, and 2.6 for inputs 
	SFRPAGE = 0x20;
	P2MDIN |= 0x70;				// 0111_0000
	SFRPAGE = 0x00;

	LCD_4BIT(1);
	LCD_4BIT(2);
	InitPinADC(2, 2);
	InitPinADC(2, 3);

	waitms(500);

	printf("\x1b[2J"); // Clear screen using ANSI escape sequence.

	printf("ADC test program\n"
		"File: %s\n"
		"Compiled: %s, %s\n\n",
		__FILE__, __DATE__, __TIME__);

	while (1) {

		if (eco_dbc == 1 && !ECO_BUT)
			eco = eco ? 0 : 1;
		eco_dbc = ECO_BUT;

		if (aut_dbc == 1 && !AUTO_BUT)
			aut = aut ? 0 : 1;
		aut_dbc = AUTO_BUT;

		if (rot_dbc == 1 && !FLIP_BUT)
			rot = rot ? 0 : 1;
		rot_dbc = FLIP_BUT;

		if (stop_dbc == 1 && !STOP_BUT)
			stop = stop ? 0 : 1;
		stop_dbc = STOP_BUT;

		IR_data = (IR_data & ~(1 << 8))  | (eco  <<  8);
		IR_data = (IR_data & ~(1 << 9))  | (aut  <<  9);
		IR_data = (IR_data & ~(1 << 10)) | (rot  << 10);
		IR_data = (IR_data & ~(1 << 11)) | (stop << 11);

		direction = Volts_at_Pin(QFP32_MUX_P2_2);
		rotation = Volts_at_Pin(QFP32_MUX_P2_3);
		
		i2c_read_addr8_data16(radar_addr, &i2c_buff);

		if (i2c_buff < 10000) {
			screen2_ln2[7]  = (char)((i2c_buff / 1000) + '0');
			screen2_ln2[9]  = (char)(((i2c_buff / 100) % 10) + '0');
			screen2_ln2[10] = (char)(((i2c_buff / 10) % 10) + '0');
			screen2_ln2[11] = (char)((i2c_buff) % 10 + '0');
		}

		//Screen 2 GUI
		IR_data = (IR_data & ~0x0007) | Arrows(direction, 3, screen2_ln1);
		IR_data = (IR_data & ~0x0038) | (Arrows(rotation, 12, screen2_ln2) << 3);

		//Screen 1 GUI
		switch (path) {
		case PATH1:
			if (refresh_acc > 10) {
				LCDprint(">Path1  Path2", 1, 1, 1);
				LCDprint(" Path3  Path4", 2, 1, 1);
			}
			if (DPAD_DOWN || DPAD_UP)
				path = PATH3;
			else if (DPAD_LEFT || DPAD_RIGHT)
				path = PATH2;
			break;
		case PATH2:
			if (refresh_acc > 10) {
				LCDprint(" Path1 >Path2", 1, 1, 1);
				LCDprint(" Path3  Path4", 2, 1, 1);
			}
			if (DPAD_DOWN || DPAD_UP)
				path = PATH4;
			else if (DPAD_LEFT || DPAD_RIGHT)
				path = PATH1;
			break;
		case PATH3:
			if (refresh_acc > 10) {
				LCDprint(" Path1  Path2", 1, 1, 1);
				LCDprint(">Path3  Path4", 2, 1, 1);
			}
			if (DPAD_DOWN || DPAD_UP)
				path = PATH1;
			else if (DPAD_LEFT || DPAD_RIGHT)
				path = PATH4;
			break;
		case PATH4:
			if (refresh_acc > 10) {
				LCDprint(" Path1  Path2", 1, 1, 1);
				LCDprint(" Path3 >Path4", 2, 1, 1);
			}
			if (DPAD_DOWN || DPAD_UP)
				path = PATH3;
			else if (DPAD_LEFT || DPAD_RIGHT)
				path = PATH2;
			break;
		default:
			if (refresh_acc > 10) {
				LCDprint("ERROR", 1, 1, 1);
				LCDprint("ERROR", 2, 1, 1);
			}
		}
		IR_data = (IR_data & ~(3 << 6)) | (path << 6);
		
		//bottlenecked by IR transmission so loop which runs 30-50ms, therefore wait 300-500ms before writing the LCD
		if (refresh_acc > 10) {
			LCDprint(screen2_ln1, 1, 1, 2);
			LCDprint(screen2_ln2, 2, 1, 2);
			refresh_acc = 0;
		}
		else refresh_acc++;

		Send_16bit(IR_data);
	}
}

