#include "main.h"
#include "speaker.h"

extern TIM_HandleTypeDef htim2;		// define TIM2 for this function

volatile uint16_t spkr_on_time = 0;	// counter for speaker being on

void HAL_TIM_PeriodElapsedCallback(TIM_HandleTypeDef *htim) {		// htim interrupt (i dont know if we have to merge this lol)

	if(htim->Instance == TIM2) {		// if the htim address is TIM2, toggle the pin HIGH/LOW
		HAL_GPIO_TogglePin(GPIOC, GPIO_PIN_15);
	}

	spkr_on_time++;

	if(spkr_on_time == 1000) {
		HAL_TIM_Base_Stop_IT(&htim2);
		HAL_GPIO_WritePin(GPIOC, GPIO_PIN_15, GPIO_PIN_RESET);
	}

}

void SPKR_ON() {
	HAL_TIM_Base_Start_IT(&htim2);
	spkr_on_time = 0;
}
