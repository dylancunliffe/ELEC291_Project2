// proportional controller
#include "sensors.h"
#include "motors.h"
#include <stdlib.h>

#define BASE 200
#define KP 0.5
#define BIAS 0 //this is optional i think, we can change it later

void Follow_Line_P(int16_t error){
	int16_t output = (KP * error) + BIAS;

	int16_t left_speed = BASE - output;
	int16_t right_speed = BASE + output;
	Motors_SetSpeed(left_speed, right_speed);

}









