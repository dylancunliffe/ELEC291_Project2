#include "control.h"
#include "sensors.h"
#include "motors.h"
#include <stdlib.h>



// speed control (need to adjust this later, idk what goes to the motors yet)
#define BASE        200

void Follow_Line(int16_t error){
	int16_t left_speed;
	int16_t right_speed;

	if (error < -15 ){ //if left < right
		left_speed  = BASE;
		right_speed = 0;
		Motor_SetSpeed(left_speed, right_speed);
	}

	else if (error > 15){ // left > right
		left_speed  = 0;
		right_speed = BASE;
		Motor_SetSpeed(left_speed, right_speed);
	}

	else {
		left_speed  = BASE;
		right_speed = BASE;
		Motor_SetSpeed(left_speed, right_speed);
	}
}
