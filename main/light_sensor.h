#pragma once
#include <stdint.h>

// VEML7700 ambient light sensor via I2C
// Returns lux value, or -1.0f on error

void  light_sensor_init(void);
float light_sensor_read_lux(void);
