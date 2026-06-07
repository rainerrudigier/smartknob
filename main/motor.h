#pragma once
#include <stdbool.h>
#include <stdint.h>
#include "setup.h"

typedef enum {
    MOTOR_DIR_LEFT  = 0,
    MOTOR_DIR_RIGHT = 1,
} motor_dir_t;

void motor_init(void);
void motor_set(motor_dir_t dir, uint32_t step_ms);
void motor_set_duty(uint8_t duty_percent);   // 0–100, default 50
void motor_stop(void);
bool motor_diag_ok(void);
