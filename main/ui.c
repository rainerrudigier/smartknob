#include "ui.h"
#include <stdio.h>
#include <stdint.h>
#include <limits.h>

static lv_obj_t *s_arc         = NULL;
static lv_obj_t *s_deg         = NULL;
static lv_obj_t *s_dir_label   = NULL;
static lv_obj_t *s_lux_label   = NULL;
static lv_obj_t *s_strain_dot  = NULL;

// Kraft-Anzeige: Schwellwert und Skalierung (raw HX711 Einheiten)
#define STRAIN_THRESHOLD   1500    // Mindestkraft für Anzeige (Rauschfilter)
#define STRAIN_MAX        80000    // Maximale Kraft (= max. Punktgröße)
#define DOT_SIZE_MIN          8    // kleinster Punktdurchmesser [px]
#define DOT_SIZE_MAX         50    // größter Punktdurchmesser [px]
// X-Position der Punkt-Mittelpunkte (Display 240×240)
#define DOT_X_LEFT           42
#define DOT_X_RIGHT         198
#define DOT_Y               120

// Brücken-Physik:
//   Brücke links:  AVDD──[R]──INA+──[R]──GND
//   Brücke rechts: AVDD──[R]──INA-──[R]──GND
//   HX711 = V(INA+) − V(INA-)
//
//   Drücken LINKS  → linke DMS verformen → V(INA+) sinkt → HX711-Wert NEGATIV
//   Drücken RECHTS → rechte DMS verformen → V(INA-) sinkt → HX711-Wert POSITIV
//
// Falls links/rechts nach Einbau vertauscht sind, dieses Define setzen:
// #define STRAIN_INVERT

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

    // Kraft-Punkt (Kreis): links oder rechts, Größe = Kraft
    s_strain_dot = lv_obj_create(screen);
    lv_obj_set_style_bg_color(s_strain_dot, lv_palette_main(LV_PALETTE_CYAN), LV_PART_MAIN);
    lv_obj_set_style_border_width(s_strain_dot, 0, LV_PART_MAIN);
    lv_obj_set_style_radius(s_strain_dot, LV_RADIUS_CIRCLE, LV_PART_MAIN);
    lv_obj_set_size(s_strain_dot, DOT_SIZE_MIN, DOT_SIZE_MIN);
    lv_obj_set_pos(s_strain_dot, DOT_X_LEFT - DOT_SIZE_MIN/2, DOT_Y - DOT_SIZE_MIN/2);
    lv_obj_add_flag(s_strain_dot, LV_OBJ_FLAG_HIDDEN);  // anfangs unsichtbar
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
    if (s_strain_dot == NULL) return;

    if (value == INT32_MIN) {
        lv_obj_add_flag(s_strain_dot, LV_OBJ_FLAG_HIDDEN);
        return;
    }

    int32_t abs_val = (value < 0) ? -value : value;

    if (abs_val < STRAIN_THRESHOLD) {
        // Kraft zu gering → Punkt ausblenden
        lv_obj_add_flag(s_strain_dot, LV_OBJ_FLAG_HIDDEN);
        return;
    }

    // Punktgröße: linear skaliert zwischen DOT_SIZE_MIN und DOT_SIZE_MAX
    if (abs_val > STRAIN_MAX) abs_val = STRAIN_MAX;
    int32_t size = DOT_SIZE_MIN +
                   (int32_t)((abs_val - STRAIN_THRESHOLD) *
                              (DOT_SIZE_MAX - DOT_SIZE_MIN) /
                              (STRAIN_MAX - STRAIN_THRESHOLD));

    // Richtung aus Brücken-Physik:
    //   HX711 negativ → INA+ sank → linke DMS verformt → LINKS gedrückt
    //   HX711 positiv → INA- sank → rechte DMS verformt → RECHTS gedrückt
#ifdef STRAIN_INVERT
    bool press_left = (value > 0);
#else
    bool press_left = (value < 0);
#endif

    int32_t cx;
    lv_color_t color;
    if (press_left) {
        cx    = DOT_X_LEFT;
        color = lv_palette_main(LV_PALETTE_CYAN);
    } else {
        cx    = DOT_X_RIGHT;
        color = lv_palette_main(LV_PALETTE_ORANGE);
    }

    lv_obj_set_size(s_strain_dot, size, size);
    lv_obj_set_pos(s_strain_dot, cx - size / 2, DOT_Y - size / 2);
    lv_obj_set_style_bg_color(s_strain_dot, color, LV_PART_MAIN);
    lv_obj_remove_flag(s_strain_dot, LV_OBJ_FLAG_HIDDEN);
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
