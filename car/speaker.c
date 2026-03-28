#include "main.h"
#include "speaker.h"

volatile uint8_t spkr_flag = 0;	// flag for the speaker output at PC15, 0 for OFF, 1 for ON

void HAL_TIM_PeriodElapsedCallback(TIM_HandleTypeDef *htim) {		// htim interrupt (i dont know if we have to merge this lol)

	if(htim->Instance == TIM2) {		// if the htim address is TIM2, toggle the pin HIGH/LOW

		if(spkr_flag) HAL_GPIO_TogglePin(GPIOC, GPIO_PIN_15);

	}

}

static void SPKR_Togg( uint8_t spkr_on ) {

	if(spkr_on) {
		spkr_flag = 1;						// turn on spkr_flag if speaker needs to be on
		HAL_TIM_Base_Start_IT(&htim2);		// enable the counter/clock interrupt, will go to
	}										// HAL_TIM_PeriodElapsedCallback each time the clock overflows
	else {
		spkr_flag = 0;						// clear the flag
		HAL_TIM_Base_STOP_IT(&htim2);		// stop the clock interrupt
		HAL_GPIO_WritePin(GPIOC, GPIO_PIN_15, GPIO_PIN_RESET);		// clear PC15
	}

}
