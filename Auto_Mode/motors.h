#ifndef MOTORS_H
#define MOTORS_H

#include <stdint.h>

void Motors_SetSpeed(int32_t left, int32_t right);

void Motors_Stop(void);

#endif
