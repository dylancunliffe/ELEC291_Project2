#include "control.h"
#include "sensors.h"
#include "motors.h"
#include <stdlib.h>

// PD controller parameters
#define KP          45
#define KD          30
#define GAIN_DIV    10

// speed control (need to adjust this later, idk what goes to the motors yet)
#define BASE        200
#define KSPEED      250

#define SCALE       1000   // same as sensors.c

static int32_t prev_error = 0;

// main control loop calculations
void Follow_Line(int32_t error)
{
	int32_t left_speed = 500;
	int32_t right_speed = 500;
	Motors_SetSpeed(left_speed, right_speed);

    //int32_t derror = error - prev_error;

    //int32_t u = (KP * error + KD * derror) / GAIN_DIV;

    //prev_error = error;
/*
	if (error<-10){
		left_speed=200;
		right_speed=0;
		Motors_SetSpeed(left_speed, right_speed);
	}
	else if (error >10){
		left_speed= 0;
		right_speed = 200;
		Motors_SetSpeed(left_speed, right_speed);
	}
*/
    // change pwm going into right motor

    //left_speed = BASE - u;
    //right_speed = BASE + u;


    //Motors_SetSpeed(left_speed, right_speed);
    // change pwm going into left motor
}



