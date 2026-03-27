/*
 * tof.c
 *
 *  Created on: Mar 24, 2026
 *      Author: dylan
 */
#include "main.h"
#include "tof.h"

#define REG_IDENTIFICATION_MODEL_ID (0xC0)
#define REG_VHV_CONFIG_PAD_SCL_SDA_EXTSUP_HV (0x89)
#define REG_MSRC_CONFIG_CONTROL (0x60)
#define REG_FINAL_RANGE_CONFIG_MIN_COUNT_RATE_RTN_LIMIT (0x44)
#define REG_SYSTEM_SEQUENCE_CONFIG (0x01)
#define REG_DYNAMIC_SPAD_REF_EN_START_OFFSET (0x4F)
#define REG_DYNAMIC_SPAD_NUM_REQUESTED_REF_SPAD (0x4E)
#define REG_GLOBAL_CONFIG_REF_EN_START_SELECT (0xB6)
#define REG_SYSTEM_INTERRUPT_CONFIG_GPIO (0x0A)
#define REG_GPIO_HV_MUX_ACTIVE_HIGH (0x84)
#define REG_SYSTEM_INTERRUPT_CLEAR (0x0B)
#define REG_RESULT_INTERRUPT_STATUS (0x13)
#define REG_SYSRANGE_START (0x00)
#define REG_GLOBAL_CONFIG_SPAD_ENABLES_REF_0 (0xB0)
#define REG_RESULT_RANGE_STATUS (0x14)

#define VL53L0X_EXPECTED_DEVICE_ID (0xEE)

#define VL53L0X_8BIT_ADDR 0x52

#define RANGE_SEQUENCE_STEP_TCC (0x10) /* Target CentreCheck */
#define RANGE_SEQUENCE_STEP_MSRC (0x04) /* Minimum Signal Rate Check */
#define RANGE_SEQUENCE_STEP_DSS (0x28) /* Dynamic SPAD selection */
#define RANGE_SEQUENCE_STEP_PRE_RANGE (0x40)
#define RANGE_SEQUENCE_STEP_FINAL_RANGE (0x80)

typedef enum
{
    CALIBRATION_TYPE_VHV,
    CALIBRATION_TYPE_PHASE
} calibration_type_t;

extern I2C_HandleTypeDef hi2c1;

bool TOF_WriteReg(uint8_t reg, uint8_t value) {
    if (HAL_I2C_Mem_Write(&hi2c1, VL53L0X_8BIT_ADDR, reg, I2C_MEMADD_SIZE_8BIT, &value, 1, 100) == HAL_OK) {
        return true;
    }
    return false;
}

bool TOF_ReadReg(uint8_t reg, uint8_t *value) {
    if (HAL_I2C_Mem_Read(&hi2c1, VL53L0X_8BIT_ADDR, reg, I2C_MEMADD_SIZE_8BIT, value, 1, 100) == HAL_OK) {
        return true;
    }
    return false;
}

bool TOF_ReadReg16(uint8_t reg, uint16_t *value) {
	uint8_t buffer[2];
	if (HAL_I2C_Mem_Read(&hi2c1, VL53L0X_8BIT_ADDR, reg, I2C_MEMADD_SIZE_8BIT, buffer, 2, 100) == HAL_OK) {
		*value = (buffer[0] << 8 | buffer[1]);
		return true;
	}
	return false;
}

static uint8_t stop_variable = 0;

bool TOFStartMeasurement(){
	bool success = TOF_WriteReg(0x80, 0x01);
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

    return true;
}

bool TOFRecordMeasurement(uint16_t *distance){
	uint8_t sysrange_start;
	uint8_t interrupt_status;

	if(!TOF_ReadReg(REG_SYSRANGE_START, &sysrange_start)) {
		return false;
	}

	if(!TOF_ReadReg(REG_RESULT_INTERRUPT_STATUS, &interrupt_status)) {
		return false;
	}

	if ((interrupt_status & 0x07) == 0) {
		return false;
	}

	if(!TOF_ReadReg16(REG_RESULT_RANGE_STATUS + 10, distance)) {
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
    success &= TOF_WriteReg(0x80,0x01);
    success &= TOF_WriteReg(0xFF,0x01);
    success &= TOF_WriteReg(0x00,0x00);
    success &= TOF_WriteReg(0x88,0x00);

    success &= TOF_ReadReg(0x91,&stop_variable);

    success &= TOF_WriteReg(0x00,0x01);
    success &= TOF_WriteReg(0xFF,0x00);
    success &= TOF_WriteReg(0x80,0x00);


    return success;
}

/**
 * Load tuning settings (same as default tuning settings provided by ST api code)
 */
static bool load_default_tuning_settings()
{
    bool success = TOF_WriteReg(0xFF, 0x01);
    success &= TOF_WriteReg(0x00, 0x00);
    success &= TOF_WriteReg(0xFF, 0x00);
    success &= TOF_WriteReg(0x09, 0x00);
    success &= TOF_WriteReg(0x10, 0x00);
    success &= TOF_WriteReg(0x11, 0x00);
    success &= TOF_WriteReg(0x24, 0x01);
    success &= TOF_WriteReg(0x25, 0xFF);
    success &= TOF_WriteReg(0x75, 0x00);
    success &= TOF_WriteReg(0xFF, 0x01);
    success &= TOF_WriteReg(0x4E, 0x2C);
    success &= TOF_WriteReg(0x48, 0x00);
    success &= TOF_WriteReg(0x30, 0x20);
    success &= TOF_WriteReg(0xFF, 0x00);
    success &= TOF_WriteReg(0x30, 0x09);
    success &= TOF_WriteReg(0x54, 0x00);
    success &= TOF_WriteReg(0x31, 0x04);
    success &= TOF_WriteReg(0x32, 0x03);
    success &= TOF_WriteReg(0x40, 0x83);
    success &= TOF_WriteReg(0x46, 0x25);
    success &= TOF_WriteReg(0x60, 0x00);
    success &= TOF_WriteReg(0x27, 0x00);
    success &= TOF_WriteReg(0x50, 0x06);
    success &= TOF_WriteReg(0x51, 0x00);
    success &= TOF_WriteReg(0x52, 0x96);
    success &= TOF_WriteReg(0x56, 0x08);
    success &= TOF_WriteReg(0x57, 0x30);
    success &= TOF_WriteReg(0x61, 0x00);
    success &= TOF_WriteReg(0x62, 0x00);
    success &= TOF_WriteReg(0x64, 0x00);
    success &= TOF_WriteReg(0x65, 0x00);
    success &= TOF_WriteReg(0x66, 0xA0);
    success &= TOF_WriteReg(0xFF, 0x01);
    success &= TOF_WriteReg(0x22, 0x32);
    success &= TOF_WriteReg(0x47, 0x14);
    success &= TOF_WriteReg(0x49, 0xFF);
    success &= TOF_WriteReg(0x4A, 0x00);
    success &= TOF_WriteReg(0xFF, 0x00);
    success &= TOF_WriteReg(0x7A, 0x0A);
    success &= TOF_WriteReg(0x7B, 0x00);
    success &= TOF_WriteReg(0x78, 0x21);
    success &= TOF_WriteReg(0xFF, 0x01);
    success &= TOF_WriteReg(0x23, 0x34);
    success &= TOF_WriteReg(0x42, 0x00);
    success &= TOF_WriteReg(0x44, 0xFF);
    success &= TOF_WriteReg(0x45, 0x26);
    success &= TOF_WriteReg(0x46, 0x05);
    success &= TOF_WriteReg(0x40, 0x40);
    success &= TOF_WriteReg(0x0E, 0x06);
    success &= TOF_WriteReg(0x20, 0x1A);
    success &= TOF_WriteReg(0x43, 0x40);
    success &= TOF_WriteReg(0xFF, 0x00);
    success &= TOF_WriteReg(0x34, 0x03);
    success &= TOF_WriteReg(0x35, 0x44);
    success &= TOF_WriteReg(0xFF, 0x01);
    success &= TOF_WriteReg(0x31, 0x04);
    success &= TOF_WriteReg(0x4B, 0x09);
    success &= TOF_WriteReg(0x4C, 0x05);
    success &= TOF_WriteReg(0x4D, 0x04);
    success &= TOF_WriteReg(0xFF, 0x00);
    success &= TOF_WriteReg(0x44, 0x00);
    success &= TOF_WriteReg(0x45, 0x20);
    success &= TOF_WriteReg(0x47, 0x08);
    success &= TOF_WriteReg(0x48, 0x28);
    success &= TOF_WriteReg(0x67, 0x00);
    success &= TOF_WriteReg(0x70, 0x04);
    success &= TOF_WriteReg(0x71, 0x01);
    success &= TOF_WriteReg(0x72, 0xFE);
    success &= TOF_WriteReg(0x76, 0x00);
    success &= TOF_WriteReg(0x77, 0x00);
    success &= TOF_WriteReg(0xFF, 0x01);
    success &= TOF_WriteReg(0x0D, 0x01);
    success &= TOF_WriteReg(0xFF, 0x00);
    success &= TOF_WriteReg(0x80, 0x01);
    success &= TOF_WriteReg(0x01, 0xF8);
    success &= TOF_WriteReg(0xFF, 0x01);
    success &= TOF_WriteReg(0x8E, 0x01);
    success &= TOF_WriteReg(0x00, 0x01);
    success &= TOF_WriteReg(0xFF, 0x00);
    success &= TOF_WriteReg(0x80, 0x00);
    return success;
}

static bool configure_interrupt()
{
    uint8_t gpio_hv_mux_active_high = 0;

    /* Interrupt on new sample ready */
    if (!TOF_WriteReg(REG_SYSTEM_INTERRUPT_CONFIG_GPIO, 0x04)) {
        return false;
    }

    /* Configure active low since the pin is pulled-up on most breakout boards */
    if (!TOF_ReadReg(REG_GPIO_HV_MUX_ACTIVE_HIGH, &gpio_hv_mux_active_high)) {
        return false;
    }
    gpio_hv_mux_active_high &= ~0x10;
    if (!TOF_WriteReg(REG_GPIO_HV_MUX_ACTIVE_HIGH, gpio_hv_mux_active_high)) {
        return false;
    }

    if (!TOF_WriteReg(REG_SYSTEM_INTERRUPT_CLEAR, 0x01)) {
        return false;
    }
    return true;
}

/**
 * Enable (or disable) specific steps in the sequence
 */
static bool set_sequence_steps_enabled(uint8_t sequence_step)
{
    return TOF_WriteReg(REG_SYSTEM_SEQUENCE_CONFIG, sequence_step);
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

static bool perform_single_ref_calibration(calibration_type_t calib_type)
{
    uint8_t sysrange_start = 0;
    uint8_t sequence_config = 0;
    uint8_t interrupt_status = 0;
    bool success = false;

    switch (calib_type)
    {
    case CALIBRATION_TYPE_VHV:
        sequence_config = 0x01;
        sysrange_start = 0x01 | 0x40;
        break;
    case CALIBRATION_TYPE_PHASE:
        sequence_config = 0x02;
        sysrange_start = 0x01 | 0x00;
        break;
    }
    if (!TOF_WriteReg(REG_SYSTEM_SEQUENCE_CONFIG, sequence_config)) {
        return false;
    }
    if (!TOF_WriteReg(REG_SYSRANGE_START, sysrange_start)) {
        return false;
    }

    /* Wait for interrupt (Blocking here is OK because this ONLY runs during init before FSM starts) */
    do {
        success = TOF_ReadReg(REG_RESULT_INTERRUPT_STATUS, &interrupt_status);
    } while (success && ((interrupt_status & 0x07) == 0));

    if (!success) {
        return false;
    }
    if (!TOF_WriteReg(REG_SYSTEM_INTERRUPT_CLEAR, 0x01)) {
        return false;
    }

    if (!TOF_WriteReg(REG_SYSRANGE_START, 0x00)) {
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
