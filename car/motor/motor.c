#include "main.h"
#include "motor.h"

static void TurnMTR(int8_t horz, int8_t vert, uint8_t eco_flag) {

	// horz, vert both -128 - 127
	// eco will be used as a flag
	uint8_t eco_off;
	if(eco_flag) eco_off = 1;
	else eco_off = 2;

	int16_t mtr1_val = abs(((int16_t)vert + (int16_t)horz)*4*eco_off);	// Left Motor (car turns right if this is larger)
	int16_t mtr2_val = abs(((int16_t)vert - (int16_t)horz)*4*eco_off);	// Right Motor (car turns left if this is larger)
	if(mtr1_val > 999) mtr1_val = 999;
	if(mtr2_val > 999) mtr2_val = 999;

	if(mtr1_val == 0 && mtr2_val == 0) {	// if no movement
		TIM2->CCR1 = 0;
		TIM2->CCR2 = 0;
		TIM22->CCR1 = 0;
		TIM22->CCR2 = 0;
	}

	if(vert > 0) {
			TIM2->CCR1 = mtr1_val;
			TIM2->CCR2 = 0;
		}
	else {
		TIM2->CCR1 = 0;
		TIM2->CCR2 = mtr1_val;
	}

	if(vert > 0) {
		TIM22->CCR1 = mtr2_val;
		TIM22->CCR2 = 0;
	}
	else{
		TIM22->CCR1 = 0;
		TIM22->CCR2 = mtr2_val;
	}

}
