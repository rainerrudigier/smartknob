#include "ui.h"
#include <stdio.h>
#include <stdint.h>
#include <limits.h>

static lv_obj_t *s_arc          = NULL;
static lv_obj_t *s_deg          = NULL;
static lv_obj_t *s_dir_label    = NULL;
static lv_obj_t *s_lux_label    = NULL;
static lv_obj_t *s_strain_label = NULL;

void ui_init(lv_display_t *disp)
{
    lv_obj_t *screen = lv_display_get_screen_active(disp);
    lv_obj_set_style_bg_color(screen, lv_color_black(), LV_PART_MAIN);

    // arc showing angle (0..360°)
    s_arc = lv_arc_create(screen);
    lv_obj_set_size(s_arc, 190, 190);
    lv_obj_center(s_arc);
    lv_arc_set_rotation(s_arc, 270);
    lv_arc_set_bg_angles(s_arc, 0, 360);
    lv_arc_set_angles(s_arc, 0, 0);
    lv_arc_set_range(s_arc, 0, 360);
    lv_arc_set_value(s_arc, 0);
    lv_obj_remove_style(s_arc, NULL, LV_PART_KNOB);
    lv_obj_set_style_arc_width(s_arc, 8, LV_PART_INDICATOR);
    lv_obj_set_style_arc_color(s_arc, lv_palette_main(LV_PALETTE_CYAN), LV_PART_INDICATOR);
    lv_obj_set_style_arc_width(s_arc, 4, LV_PART_MAIN);
    lv_obj_set_style_arc_color(s_arc, lv_color_make(40, 40, 40), LV_PART_MAIN);

    // angle value in arc center
    s_deg = lv_label_create(screen);
    lv_obj_set_style_text_color(s_deg, lv_color_white(), LV_PART_MAIN);
    lv_label_set_text(s_deg, "---°");
    lv_obj_align(s_deg, LV_ALIGN_CENTER, 0, -10);

    // direction label below angle
    s_dir_label = lv_label_create(screen);
    lv_label_set_text(s_dir_label, "");
    lv_obj_align(s_dir_label, LV_ALIGN_CENTER, 0, 14);

    // lux label at top
    s_lux_label = lv_label_create(screen);
    lv_obj_set_style_text_color(s_lux_label, lv_palette_main(LV_PALETTE_YELLOW), LV_PART_MAIN);
    lv_label_set_text(s_lux_label, "--- lx");
    lv_obj_align(s_lux_label, LV_ALIGN_TOP_MID, 0, 14);

    // strain label at bottom
    s_strain_label = lv_label_create(screen);
    lv_obj_set_style_text_color(s_strain_label, lv_palette_main(LV_PALETTE_PURPLE), LV_PART_MAIN);
    lv_label_set_text(s_strain_label, "F: ---");
    lv_obj_align(s_strain_label, LV_ALIGN_BOTTOM_MID, 0, -14);
}

void ui_update_angle(float degrees, motor_dir_t dir)
{
    if (s_arc == NULL) return;

    // angle
    char buf[16];
    if (degrees < 0.0f) {
        lv_label_set_text(s_deg, "ERR");
    } else {
        lv_arc_set_value(s_arc, 360 - (int32_t)degrees);
        snprintf(buf, sizeof(buf), "%.1f\xc2\xb0", degrees);  // °  in UTF-8
        lv_label_set_text(s_deg, buf);
    }

    // direction
    if (dir == MOTOR_DIR_LEFT) {
        lv_obj_set_style_text_color(s_dir_label, lv_palette_main(LV_PALETTE_GREEN), LV_PART_MAIN);
        lv_label_set_text(s_dir_label, "< LEFT");
    } else {
        lv_obj_set_style_text_color(s_dir_label, lv_palette_main(LV_PALETTE_ORANGE), LV_PART_MAIN);
        lv_label_set_text(s_dir_label, "RIGHT >");
    }
}

void ui_update_strain(int32_t value)
{
    if (s_strain_label == NULL) return;
    char buf[20];
    if (value == INT32_MIN) {
        lv_label_set_text(s_strain_label, "F: ERR");
    } else {
        snprintf(buf, sizeof(buf), "F: %ld", (long)value);
        lv_label_set_text(s_strain_label, buf);
    }
}

void ui_update_lux(float lux)
{
    if (s_lux_label == NULL) return;
    char buf[20];
    if (lux < 0.0f) {
        lv_label_set_text(s_lux_label, "ERR lx");
    } else if (lux >= 1000.0f) {
        snprintf(buf, sizeof(buf), "%.1f klx", lux / 1000.0f);
        lv_label_set_text(s_lux_label, buf);
    } else {
        snprintf(buf, sizeof(buf), "%.1f lx", lux);
        lv_label_set_text(s_lux_label, buf);
    }
}
