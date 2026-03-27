#include "ir_r.h"
#include "main.h"
#include "ir_r.h"
#include <stdio.h>
#include "stm32l0xx_hal.h"

uint8_t tx_buffer[10] = "ho\n\r";
uint8_t second_buffer[8];
uint32_t final_buffer[40];
volatile int printnow = 0;
volatile int printnow2 = 0;
uint32_t timer21_value = 0;
volatile uint32_t overflow_timer21 = 0;
volatile int countup = -1;
volatile uint32_t ir_signal = 0;

void populate_data(void)
{
	if (overflow_timer21 == 0)
	{
		if (countup == 32 || (countup != -1 && timer21_value > 13000 && timer21_value < 14000)) { // either end of message or the proper header detected in wrong place
			countup = -1; // throw away the signal that has been building up and overwrite it later, wait for header again
			printnow2 = 1;
			//ir_signal = 0;

		} else if (countup != -1 && timer21_value > 1000 && timer21_value < 1200) { // good zero found
			ir_signal &= ~(1 << (31-countup));// commit number to data
			countup ++;

		} else if (countup != -1 && timer21_value > 2100 && timer21_value < 2300) { // good one found
			ir_signal |= (1 << (31-countup));// commit number to data
			countup ++;

		} else if (countup == -1 && timer21_value > 13000 && timer21_value < 14000) { // good header found (necessary)
			countup ++;

		} else {
			countup = -1; // makes us wait for header again
		}

	} else {
		overflow_timer21 = 0;
		ir_signal = 0;
	}

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
	  populate_data();

}
