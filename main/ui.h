#pragma once
#include "lvgl.h"

void ui_init(lv_display_t *disp);
#include "motor.h"
void ui_update_angle(float degrees, motor_dir_t dir);
void ui_update_lux(float lux);
void ui_update_strain(int32_t value);
