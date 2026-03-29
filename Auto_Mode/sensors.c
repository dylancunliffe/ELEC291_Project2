/*
 * sensors.c
 *
 *  Created on: Mar 24, 2026
 *      Author: yasna
 */


#include "sensors.h"
#include "main.h"
#include <stdlib.h>


// PARAMETERS
#define SCALE            1000
#define MIN_SIGNAL       50

// Intersection thresholds (change later!!!!!!)
#define TH_L             200
#define TH_C             200
#define TH_R             200


static int32_t Lf = 0;
static int32_t Cf = 0;
static int32_t Rf = 0;

static int32_t error = 0;
static int32_t sum = 0;

static int line_detected = 0;
static int intersection_detected = 0;
static int intersection_count;

// might need to change this, I just want to be able to read ADC channels

void Sensors_Update(uint16_t left, uint16_t right, uint16_t center)
{
    // read raw values form adc channels
    int16_t Lraw = (int16_t)left;
    int16_t Craw = (int16_t)center;
    int16_t Rraw = (int16_t)right;

    // averaged data, might change this later depending on how fast adc sampling is
    // 50% new, 50% old
    // Lf = (Lraw + 3 * Lf) / 4;
    Lf = (2*Lraw + 2*Lf) /4;
    // Cf = (Craw + 3 * Cf) / 4;
    Cf = (2* Craw + 2* Cf) / 4;
    // Rf = (Rraw + 3 * Rf) / 4;
    Rf = (2* Rraw + 2 * Rf) /4;

    // sum
    sum = Lf + Rf; //can maybe add the readings from the middle inductor?

    // line detection
    if(sum > MIN_SIGNAL)
    {
        line_detected = 1;

        // normalized error
        error = (Lf - Rf); //want error to be as close to 0 as possible, might be different based on the postion of inductor on the car
    }
    else
    {
        line_detected = 0;
        error = 0;
    }

    uint8_t is_over_intersection = (Lf > TH_L && Cf > TH_C && Rf > TH_R); //need to change this later

        // 2. Rising Edge Detection:
        // If we ARE over an intersection now, but we WEREN'T in the last loop...
    if(is_over_intersection && !intersection_detected)
      {
           intersection_count++; // Only happens once per physical intersection!
      }

        // 3. Save the current state for the next loop iteration
     intersection_detected = is_over_intersection;
}

// "getter" functions to use in main and such
int16_t Sensors_Get_Error(void)
{
    return error;
}

uint8_t Sensors_LineDetected(void)
{
    return line_detected;
}

uint8_t Sensors_IntersectionDetected(void)
{
    return intersection_detected;
}
int16_t Sensors_Get_IntersectionCount(void)
{
    return intersection_count;
}

// for debugging
int16_t Sensors_Get_Left(void)   { return Lf; }
int16_t Sensors_Get_Center(void) { return Cf; }
int16_t Sensors_Get_Right(void)  { return Rf; }
int16_t Sensors_Get_Sum(void)    { return sum; }
