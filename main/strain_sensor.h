#pragma once
#include <stdint.h>
#include <stdbool.h>

void    strain_sensor_init(void);
void    strain_sensor_tare(void);       // store current reading as zero reference
int32_t strain_sensor_read(void);       // returns tared signed value, INT32_MIN on error
bool    strain_sensor_ready(void);      // true if DOUT is low (data available)
