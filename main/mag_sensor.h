#pragma once
#include <stdint.h>
#include <stdbool.h>

// MT6701 14-bit magnetic angle sensor via SSI
// Returns angle in range 0..16383 (14-bit, 0=0°, 16383≈360°)
// Returns -1 on error

void     mag_sensor_init(void);
int32_t  mag_sensor_read_raw(void);       // 0..16383, or -1 on error
float    mag_sensor_read_degrees(void);   // 0.0..359.978°
