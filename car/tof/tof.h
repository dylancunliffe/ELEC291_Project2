/*
 * tof.h
 *
 *  Created on: Mar 24, 2026
 *      Author: Dylan
 */

#ifndef INC_TOF_H_
#define INC_TOF_H_

#include <stdbool.h>
#include <stdint.h>

#define VL53L0X_OUT_OF_RANGE (8190)

bool TOFStartMeasurement();
bool TOFRecordMeasurement(uint16_t *distance);
bool vl53l0x_init();

#endif /* INC_TOF_H_ */

