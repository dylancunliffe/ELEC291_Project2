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
//#define KSPEED      250

//#define SCALE       1000   // same as sensors.c

static int16_t prev_error = 0;

// main control loop calculations
void Follow_Line(int16_t error)
{
	int16_t left_speed;
	int16_t right_speed;

    int16_t derror = error - prev_error;

    int16_t u = (KP * error + KD * derror) / GAIN_DIV;

    prev_error = error;

    // change pwm going into right motor
    left_speed = BASE - u;
    right_speed = BASE + u;
    Motors_SetSpeed(left_speed, right_speed);
    // change pwm going into left motor
}

// need to set base speed in main!!!!!!!!
// and then we can call this in main like this:
//int32_t error = Sensors_Get_Error();
//int32_t u = Control_Compute(error);
//int32_t left  = base + u;
//int32_t right = base - u;
//Motors_SetSpeed(left, right); (or whatever our function name ends up being)

