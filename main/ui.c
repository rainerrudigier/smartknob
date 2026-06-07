#include "ui.h"
#include <stdio.h>
#include <stdint.h>
#include <limits.h>

static lv_obj_t *s_arc          = NULL;
static lv_obj_t *s_deg          = NULL;
static lv_obj_t *s_dir_label    = NULL;
static lv_obj_t *s_lux_label    = NULL;
static lv_obj_t *s_strain_ring  = NULL;  // Press-Ring außerhalb des Winkel-Arcs
static lv_obj_t *s_strain_raw   = NULL;  // Rohwert-Label (Kalibrierung/Debug)

// Press-Erkennung: Schwellwert und Skalierung (raw HX711 Einheiten, nach Tare)
#define STRAIN_THRESHOLD   1500   // Mindestkraft für Anzeige (Rauschfilter)
#define STRAIN_MAX        80000   // Maximale Kraft (= maximale Ringbreite)
#define RING_SIZE          220    // Außendurchmesser des Press-Rings [px]
#define RING_WIDTH_MIN       2    // Mindest-Ringbreite [px]
#define RING_WIDTH_MAX      18    // Maximale Ringbreite [px]

void ui_init(lv_display_t *disp)
{
    lv_obj_t *screen = lv_display_get_screen_active(disp);
    lv_obj_set_style_bg_color(screen, lv_color_black(), LV_PART_MAIN);

    // Press-Ring: voller Kreis, transparenter Hintergrund, nur Rand sichtbar.
    // Liegt hinter dem Winkel-Arc (zuerst erstellt = unterste Ebene).
    s_strain_ring = lv_obj_create(screen);
    lv_obj_set_size(s_strain_ring, RING_SIZE, RING_SIZE);
    lv_obj_center(s_strain_ring);
    lv_obj_set_style_radius(s_strain_ring, LV_RADIUS_CIRCLE, LV_PART_MAIN);
    lv_obj_set_style_bg_opa(s_strain_ring, LV_OPA_TRANSP, LV_PART_MAIN);
    lv_obj_set_style_border_color(s_strain_ring, lv_palette_main(LV_PALETTE_ORANGE), LV_PART_MAIN);
    lv_obj_set_style_border_width(s_strain_ring, RING_WIDTH_MIN, LV_PART_MAIN);
    lv_obj_set_style_border_opa(s_strain_ring, LV_OPA_TRANSP, LV_PART_MAIN);  // anfangs unsichtbar
    lv_obj_clear_flag(s_strain_ring, LV_OBJ_FLAG_SCROLLABLE);

    // Winkel-Arc (0..360°)
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

    // Winkel-Wert in der Arc-Mitte
    s_deg = lv_label_create(screen);
    lv_obj_set_style_text_color(s_deg, lv_color_white(), LV_PART_MAIN);
    lv_label_set_text(s_deg, "---\xc2\xb0");
    lv_obj_align(s_deg, LV_ALIGN_CENTER, 0, -10);

    // Richtungs-Label unterhalb Winkel
    s_dir_label = lv_label_create(screen);
    lv_label_set_text(s_dir_label, "");
    lv_obj_align(s_dir_label, LV_ALIGN_CENTER, 0, 14);

    // Lux-Label oben
    s_lux_label = lv_label_create(screen);
    lv_obj_set_style_text_color(s_lux_label, lv_palette_main(LV_PALETTE_YELLOW), LV_PART_MAIN);
    lv_label_set_text(s_lux_label, "--- lx");
    lv_obj_align(s_lux_label, LV_ALIGN_TOP_MID, 0, 14);

    // Kraft-Rohwert unten (Debug/Kalibrierung)
    s_strain_raw = lv_label_create(screen);
    lv_obj_set_style_text_color(s_strain_raw, lv_palette_main(LV_PALETTE_GREY), LV_PART_MAIN);
    lv_label_set_text(s_strain_raw, "F: ---");
    lv_obj_align(s_strain_raw, LV_ALIGN_BOTTOM_MID, 0, -14);
}

void ui_update_angle(float degrees, motor_dir_t dir)
{
    if (s_arc == NULL) return;

    char buf[16];
    if (degrees < 0.0f) {
        lv_label_set_text(s_deg, "ERR");
    } else {
        lv_arc_set_value(s_arc, 360 - (int32_t)degrees);
        snprintf(buf, sizeof(buf), "%.1f\xc2\xb0", degrees);
        lv_label_set_text(s_deg, buf);
    }

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
    // Rohwert immer aktualisieren
    if (s_strain_raw != NULL) {
        char rbuf[24];
        if (value == INT32_MIN) {
            lv_label_set_text(s_strain_raw, "F: ERR");
        } else {
            snprintf(rbuf, sizeof(rbuf), "F: %ld", (long)value);
            lv_label_set_text(s_strain_raw, rbuf);
        }
    }

    if (s_strain_ring == NULL || value == INT32_MIN) {
        if (s_strain_ring) {
            lv_obj_set_style_border_opa(s_strain_ring, LV_OPA_TRANSP, LV_PART_MAIN);
        }
        return;
    }

    int32_t abs_val = (value < 0) ? -value : value;

    if (abs_val < STRAIN_THRESHOLD) {
        // Kein Druck → Ring ausblenden
        lv_obj_set_style_border_opa(s_strain_ring, LV_OPA_TRANSP, LV_PART_MAIN);
        return;
    }

    // Ringbreite: linear skaliert zwischen RING_WIDTH_MIN und RING_WIDTH_MAX
    if (abs_val > STRAIN_MAX) abs_val = STRAIN_MAX;
    int32_t width = RING_WIDTH_MIN +
                    (int32_t)((abs_val - STRAIN_THRESHOLD) *
                               (RING_WIDTH_MAX - RING_WIDTH_MIN) /
                               (STRAIN_MAX - STRAIN_THRESHOLD));

    lv_obj_set_style_border_width(s_strain_ring, (int16_t)width, LV_PART_MAIN);
    lv_obj_set_style_border_opa(s_strain_ring, LV_OPA_COVER, LV_PART_MAIN);
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
