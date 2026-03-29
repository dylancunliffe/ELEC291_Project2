#include "ir_r.h"
#include "main.h"
#include "ir_r.h"
#include <stdio.h>
#include "stm32l0xx_hal.h"

#define GET_BITS(val, start, len) (((val) >> (start)) & ((1U << (len)) - 1)) // start at 0
#define ALL_ONES(len) ((1U << (len)) - 1)


uint8_t tx_buffer[10] = "ho\n\r";
uint8_t flag_buffer[10];
uint8_t second_buffer[8];
uint32_t final_buffer[40];
volatile int printnow = 0;
volatile int printnow2 = 0;
uint32_t timer21_value = 0;
volatile uint32_t overflow_timer21 = 0;
volatile int countup = -1;
volatile uint32_t ir_signal = 0;
volatile uint32_t ir_signal_copy = 0;
volatile uint16_t direction_data = 0;
volatile uint16_t rotation_data = 0;
volatile uint16_t preset_data = 0;


void populate_data(void)
{
	/*
	if (overflow_timer21 == 0)
	{
		if ((countup != -1 && timer21_value > 13000 && timer21_value < 14000)) { // either end of message or the proper header detected in wrong place
			countup = -1; // throw away the signal that has been building up and overwrite it later, wait for header again
			//printnow2 = 1;
			//ir_signal_copy = ir_signal;
			//decode_into_flags(ir_signal_copy);
			//ir_signal = 0;

		} else if (countup != -1 && timer21_value > 1000 && timer21_value < 1200) { // good zero found
			ir_signal &= ~(1 << (31-countup));// commit number to data
			countup ++;
			if (countup == 32) {
				countup = -1; // throw away the signal that has been building up and overwrite it later, wait for header again
				printnow2 = 1;
				ir_signal_copy = ir_signal;
				decode_into_flags(ir_signal_copy);
			}

		} else if (countup != -1 && timer21_value > 2100 && timer21_value < 2300) { // good one found
			ir_signal |= (1 << (31-countup));// commit number to data
			countup ++;
			if (countup == 32) {
							countup = -1; // throw away the signal that has been building up and overwrite it later, wait for header again
							printnow2 = 1;
							ir_signal_copy = ir_signal;
							decode_into_flags(ir_signal_copy);
						}

		} else if (countup == -1 && timer21_value > 13000 && timer21_value < 14000) { // good header found (necessary)
			countup ++;

		} else {
			countup = -1; // makes us wait for header again
		}

	} else {
		overflow_timer21 = 0;
		ir_signal = 0;
	}
	*/

}

void get_tim21_value(void)
{
	timer21_value = TIM21->CNT;
}

void do_on_negedge (void)
{
	  get_tim21_value();
	  TIM21->CNT = 0;
	  printnow = 1;
	//  populate_data();

}


void decode_into_flags (uint32_t ir) { // only working with luke's signal, temu lights aren't mirrored therefore always 0
	//getting fed ir_signal_copy
	//start by sending header 1000, continue to stop (11), rotate 180(10)... THEN, start AGAIN at the header but INVERTED -> 0111, etc and go up to bit 0.
	//must pass a snapshot of ir_signal, not the actual because it might be written to while this function is decoding
	// bitwise xor , bit by bit xor operation, bitwise &, logical &&
	// have extra variables or refetch these values for commiting ot flags?

	uint8_t dir_data  = ((GET_BITS(ir, 0, 3) ^ GET_BITS(ir, 16, 3)) == ALL_ONES(3)); // resolution of 8, only use 7 //
	uint8_t rot_data  = ((GET_BITS(ir, 3, 3) ^ GET_BITS(ir, 19, 3)) == ALL_ONES(3)); // resolution of 8, only use 7 //
	uint8_t path  =((GET_BITS(ir, 6, 2) ^ GET_BITS(ir, 22, 2)) == ALL_ONES(2)); // direction turning presets // yasna
	uint8_t eco  = ((GET_BITS(ir, 8, 1) ^ GET_BITS(ir, 24, 1)) == ALL_ONES(1)); //eco mode // ethan
	uint8_t auton= ((GET_BITS(ir, 9, 1) ^ GET_BITS(ir, 25, 1)) == ALL_ONES(1)); //autonomous
	uint8_t rotate = ((GET_BITS(ir, 10, 1) ^ GET_BITS(ir, 26, 1)) == ALL_ONES(1)); //rotate 180
	uint8_t stop = ((GET_BITS(ir, 11, 1) ^ GET_BITS(ir, 27, 1)) == ALL_ONES(1));
	uint8_t address = ((GET_BITS(ir, 12, 4) ^ GET_BITS(ir, 28, 4)) == ALL_ONES(4));
	uint8_t address_correct = (GET_BITS(ir, 28, 4) == 0x8);

	if (dir_data && rot_data && path && eco && auton && rotate && stop && address && address_correct) {
		// can finally commit all values to main after ensuring that we have a correct signal
		flags.valid_config = 1;
		flags.eco_mode = GET_BITS(ir, 24, 1);
		flags.auto_mode = GET_BITS(ir, 25, 1);
		flags.rotate_button = GET_BITS(ir, 26, 1);
		flags.stop = GET_BITS(ir, 27, 1);

		// not single bit flags, but data values (I need to map numbers for motor doc)
		//[( 0-6 ) -3] *40

		direction_data = ((int)GET_BITS(ir, 16, 3) - 3) * 40; // [0-6] -> [-120, -80, -40, 0, 40, 80, 120]
		rotation_data = ((int)GET_BITS(ir, 19, 3) - 3) * 40;

		//declare path presets elsewhere (in main)
		preset_data = GET_BITS(ir, 22, 2);

	}

}


