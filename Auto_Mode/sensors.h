#ifndef SENSORS_H
#define SENSORS_H

#include <stdint.h>


// Call this every control loop (in timer interrupt)
void Sensors_Update(uint16_t left, uint16_t right, uint16_t center);

// Normalized line-following error (-SCALE to +SCALE)
int32_t Sensors_Get_Error(void);

// 1 = line detected, 0 = no line
uint8_t Sensors_LineDetected(void);

// 1 = intersection detected
uint8_t Sensors_IntersectionDetected(void);

// for debugging
// filtered sensor values
int32_t Sensors_Get_Left(void);
int32_t Sensors_Get_Center(void);
int32_t Sensors_Get_Right(void);

// total signal strength (might not need)
int32_t Sensors_Get_Sum(void);

#endif
