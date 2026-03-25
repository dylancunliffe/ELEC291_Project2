/*
 * tof.c
 *
 *  Created on: Mar 24, 2026
 *      Author: dylan
 */
#include "main.h"

bool TOFStartMeasurement(){
	uint8_t sysrange_start = 0;
	bool success;
    uint8_t interrupt_status = 0;

	success = TOF_WriteReg(0x80, 0x01);
    success &= TOF_WriteReg(0xFF, 0x01);
    success &= TOF_WriteReg(0x00, 0x00);
    success &= TOF_WriteReg(0x91, stop_variable);
    success &= TOF_WriteReg(0x00, 0x01);
    success &= TOF_WriteReg(0xFF, 0x00);
    success &= TOF_WriteReg(0x80, 0x00);
    if (!success) {
        return false;
    }
    if (!TOF_WriteReg(REG_SYSRANGE_START, 0x01)) {
        return false;
    }
}

bool TOFRecordMeasurement(){
	uint32_t distance;

	if(!TOF_ReadReg(REG_SYSRANGE_START, &sysrange_start)) {
		return false;
	}

	if(!TOF_ReadReg(REG_RESULT_INTERRUPT_STATUS, &interrupt_status)) {
		return false;
	}

	if(!TOF_ReadReg(REG_RESULT_RANGE_STATUS + 10, &distance)) {
		return false;
	}

	if(!TOF_WriteReg(REG_SYSTEM_INTERRUPT_CLEAR, 0x01)) {
		return false;
	}

	/* 8190 or 8191 may be returned when obstacle is out of range. */
	if (*distance == 8190 || *distance == 8191) {
		*distance = VL53L0X_OUT_OF_RANGE;
	}

	return true;

}

extern I2C_HandleTypeDef hi2c1;

bool TOF_WriteReg(uint8_t reg, uint8_t value) {
    if (HAL_I2C_Mem_Write(&hi2c1, (0x52 << 1), reg, I2C_MEMADD_SIZE_8BIT, &value, 1, 100) == HAL_OK) {
        return true;
    }
    return false;
}

bool TOF_ReadReg(uint8_t reg, uint8_t *value) {
    if (HAL_I2C_Mem_Read(&hi2c1, (0x52 << 1), reg, I2C_MEMADD_SIZE_8BIT, value, 1, 100) == HAL_OK) {
        return true;
    }
    return false;
}


// EVERYTHING BELOW IS INIT FUNTIONS
// THIS IS ALL JESUS CODE

/**
 * We can read the model id to confirm that the device is booted.
 * (There is no fresh_out_of_reset as on the vl6180x)
 */
static bool device_is_booted()
{
    uint8_t device_id = 0;


    if (!TOF_ReadReg(REG_IDENTIFICATION_MODEL_ID, &device_id)) {
        return false;
    }

    return device_id == VL53L0X_EXPECTED_DEVICE_ID;
}

/**
 * One time device initialization
 */
static bool data_init()
{
    bool success = true;

    /* Set 2v8 mode */
    uint8_t vhv_config_scl_sda = 0;
    if (!TOF_ReadReg(REG_VHV_CONFIG_PAD_SCL_SDA_EXTSUP_HV, &vhv_config_scl_sda)) {
        return false;
    }

    vhv_config_scl_sda |= 0x01;
    if (!TOF_WriteReg(REG_VHV_CONFIG_PAD_SCL_SDA_EXTSUP_HV, vhv_config_scl_sda)) {
        return false;
    }

    /* Set I2C standard mode */
    // CONVERTED FROM JESUS BLOCKING FUNCTIONS
    success &= TOF_WriteReg(0x88,0x00);
    TOF_WriteReg(0x80,0x01);
    TOF_WriteReg(0xFF,0x01);
    TOF_WriteReg(0x00,0x00);
    TOF_WriteReg(0x88,0x00);

    uint8_t stop_variable;
    TOF_ReadReg(0x91,&stop_variable);

    TOF_WriteReg(0x00,0x01);
    TOF_WriteReg(0xFF,0x00);
    TOF_WriteReg(0x80,0x00);


    return success;
}

/**
 * Basic device initialization
 */
static bool static_init()
{
    if (!load_default_tuning_settings()) {
        return false;
    }

    if (!configure_interrupt()) {
        return false;
    }

    if (!set_sequence_steps_enabled(RANGE_SEQUENCE_STEP_DSS +
                                    RANGE_SEQUENCE_STEP_PRE_RANGE +
                                    RANGE_SEQUENCE_STEP_FINAL_RANGE)) {
        return false;
    }

    return true;
}

/**
 * Temperature calibration needs to be run again if the temperature changes by
 * more than 8 degrees according to the datasheet.
 */
static bool perform_ref_calibration()
{
    if (!perform_single_ref_calibration(CALIBRATION_TYPE_VHV)) {
        return false;
    }
    if (!perform_single_ref_calibration(CALIBRATION_TYPE_PHASE)) {
        return false;
    }
    /* Restore sequence steps enabled */
    if (!set_sequence_steps_enabled(RANGE_SEQUENCE_STEP_DSS +
                                    RANGE_SEQUENCE_STEP_PRE_RANGE +
                                    RANGE_SEQUENCE_STEP_FINAL_RANGE)) {
        return false;
    }
    return true;
}

bool vl53l0x_init()
{
    if (!device_is_booted()) {
        return false;
    }
    if (!data_init()) {
        return false;
    }
    if (!static_init()) {
        return false;
    }
    if (!perform_ref_calibration()) {
        return false;
    }
    return true;
}

