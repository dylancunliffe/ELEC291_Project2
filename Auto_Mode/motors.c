// writes whatever it needs to write to the needed registers
#include "motors.h"
#include "main.h"
#include <stdio.h>

// 0% Duty Cycle is CCR = 0
// 100% Duty Cycle is CCR = 999

void Motors_SetSpeed(int32_t left, int32_t right){

	// gets values and saves them to whatever
	TIM2->CCR1 = left;
	TIM2->CCR2 = 0;
	TIM22->CCR1 = right;
	TIM22->CCR2 = 0;
}

void Motors_Stop(void){
	// sets both values to zero to stop the motors from moving
	TIM2->CCR1 = 0;
	TIM2->CCR2 = 0;
	TIM22->CCR1 = 0;
	TIM22->CCR2 = 0;
}
