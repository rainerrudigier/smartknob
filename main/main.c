#include <stdio.h>
#include <math.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "esp_heap_caps.h"
#include "motor.h"   // inkludiert setup.h

// MOTOR_TEST und MOTOR_SINE_COMM werden über setup.h gesteuert

// LCD + LVGL werden in beiden Modi benötigt
#include "esp_timer.h"
#include "driver/gpio.h"
#include "driver/spi_master.h"
#include "esp_lcd_panel_io.h"
#include "esp_lcd_panel_vendor.h"
#include "esp_lcd_panel_ops.h"
#include "esp_lcd_gc9a01.h"
#include "lvgl.h"
#include "pins.h"

#ifndef MOTOR_TEST
#include "ui.h"
#include "leds.h"
#include "mag_sensor.h"
#include "light_sensor.h"
#include "strain_sensor.h"
#endif

static const char *TAG = "smartknob";

#ifdef MOTOR_TEST
// ═══════════════════════════════════════════════════════════════════
//  MOTOR TEST MODE
// ═══════════════════════════════════════════════════════════════════

// ── shared LCD/LVGL helpers (minimal, static display only) ───────────────────
#define MT_DRAW_LINES  (LCD_V_RES / 10)
#define MT_TICK_MS     2

static esp_lcd_panel_handle_t mt_panel = NULL;
static lv_display_t          *mt_disp  = NULL;
static lv_obj_t              *mt_num_label   = NULL;
static lv_obj_t              *mt_info_label  = NULL;
static lv_obj_t              *mt_stop_label  = NULL;

static void mt_flush_cb(lv_display_t *d, const lv_area_t *a, uint8_t *px)
{
    esp_lcd_panel_draw_bitmap(mt_panel, a->x1, a->y1, a->x2+1, a->y2+1, (uint16_t*)px);
    lv_display_flush_ready(d);
}
static void mt_tick_cb(void *arg) { lv_tick_inc(MT_TICK_MS); }


static void mt_display_init(void)
{
    // backlight
    gpio_config_t bl = { .pin_bit_mask=(1ULL<<LCD_PIN_BACKLIGHT), .mode=GPIO_MODE_OUTPUT };
    gpio_config(&bl);
    gpio_set_level(LCD_PIN_BACKLIGHT, 1);

    // SPI bus
    spi_bus_config_t bus = {
        .mosi_io_num=LCD_PIN_DATA, .miso_io_num=-1, .sclk_io_num=LCD_PIN_SCK,
        .quadwp_io_num=-1, .quadhd_io_num=-1,
        .max_transfer_sz=LCD_H_RES * MT_DRAW_LINES * 2,
    };
    ESP_ERROR_CHECK(spi_bus_initialize(LCD_SPI_HOST, &bus, SPI_DMA_CH_AUTO));

    esp_lcd_panel_io_handle_t io;
    esp_lcd_panel_io_spi_config_t io_cfg = {
        .dc_gpio_num=LCD_PIN_CMD, .cs_gpio_num=LCD_PIN_CS,
        .pclk_hz=LCD_SPI_FREQ_HZ, .lcd_cmd_bits=LCD_CMD_BITS,
        .lcd_param_bits=LCD_PARAM_BITS, .spi_mode=0, .trans_queue_depth=10,
    };
    ESP_ERROR_CHECK(esp_lcd_new_panel_io_spi((esp_lcd_spi_bus_handle_t)LCD_SPI_HOST, &io_cfg, &io));

    esp_lcd_panel_dev_config_t panel_cfg = {
        .reset_gpio_num=LCD_PIN_RST, .rgb_endian=LCD_RGB_ENDIAN_BGR, .bits_per_pixel=16,
    };
    ESP_ERROR_CHECK(esp_lcd_new_panel_gc9a01(io, &panel_cfg, &mt_panel));
    ESP_ERROR_CHECK(esp_lcd_panel_reset(mt_panel));
    ESP_ERROR_CHECK(esp_lcd_panel_init(mt_panel));
    ESP_ERROR_CHECK(esp_lcd_panel_mirror(mt_panel, true, false));
    ESP_ERROR_CHECK(esp_lcd_panel_invert_color(mt_panel, true));
    ESP_ERROR_CHECK(esp_lcd_panel_disp_on_off(mt_panel, true));

    lv_init();

    esp_timer_handle_t tick;
    esp_timer_create_args_t t = { .callback=mt_tick_cb, .name="mt_tick" };
    ESP_ERROR_CHECK(esp_timer_create(&t, &tick));
    ESP_ERROR_CHECK(esp_timer_start_periodic(tick, MT_TICK_MS * 1000));

    mt_disp = lv_display_create(LCD_H_RES, LCD_V_RES);
    lv_display_set_user_data(mt_disp, mt_panel);
    lv_display_set_flush_cb(mt_disp, mt_flush_cb);
    static lv_color_t buf1[LCD_H_RES * MT_DRAW_LINES];
    static lv_color_t buf2[LCD_H_RES * MT_DRAW_LINES];
    lv_display_set_buffers(mt_disp, buf1, buf2, sizeof(buf1), LV_DISPLAY_RENDER_MODE_PARTIAL);

    // build static UI
    lv_obj_t *scr = lv_display_get_screen_active(mt_disp);
    lv_obj_set_style_bg_color(scr, lv_color_black(), LV_PART_MAIN);

    lv_obj_t *title = lv_label_create(scr);
    lv_label_set_text(title, "MOTOR TEST");
    lv_obj_set_style_text_color(title, lv_palette_main(LV_PALETTE_YELLOW), LV_PART_MAIN);
    lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 18);

    mt_num_label = lv_label_create(scr);
    lv_obj_set_style_text_color(mt_num_label, lv_palette_main(LV_PALETTE_GREY), LV_PART_MAIN);
    lv_label_set_text(mt_num_label, "");
    lv_obj_align(mt_num_label, LV_ALIGN_CENTER, 0, -20);

    mt_info_label = lv_label_create(scr);
    lv_label_set_text_static(mt_info_label, "");
    lv_obj_set_style_text_color(mt_info_label, lv_color_white(), LV_PART_MAIN);
    lv_label_set_long_mode(mt_info_label, LV_LABEL_LONG_WRAP);
    lv_obj_set_width(mt_info_label, 180);
    lv_obj_align(mt_info_label, LV_ALIGN_CENTER, 0, 10);

    mt_stop_label = lv_label_create(scr);
    lv_obj_set_style_text_color(mt_stop_label, lv_palette_main(LV_PALETTE_RED), LV_PART_MAIN);
    lv_label_set_text(mt_stop_label, "");
    lv_obj_align(mt_stop_label, LV_ALIGN_BOTTOM_MID, 0, -18);

    // Initial render from this task (no separate LVGL task needed)
    lv_timer_handler();
}

static void mt_show(int idx, int total, const char *label, bool stopped)
{
    char buf[16];
    snprintf(buf, sizeof(buf), "%d / %d", idx, total);
    lv_label_set_text(mt_num_label, buf);
    lv_label_set_text(mt_info_label, label);
    lv_label_set_text(mt_stop_label, stopped ? "[ STOP ]" : "");
    // Alle LVGL-Calls laufen in app_main → kein Task-Konflikt
    lv_timer_handler();
}

// ── scenarios + app_main ─────────────────────────────────────────────────────

void app_main(void)
{
    ESP_LOGI(TAG, "*** MOTOR TEST MODE ***");

    motor_init();
    motor_set_duty(35);
    mt_display_init();

    typedef struct {
        motor_dir_t dir;
        uint32_t    step_ms;
        uint8_t     duty;
        uint32_t    run_ms;
        const char *label;
    } scenario_t;

    static const scenario_t scenarios[] = {
        { MOTOR_DIR_LEFT,  10, 50, 3000, "LEFT\nfast 10ms 50%" },
        { MOTOR_DIR_LEFT,  20, 50, 3000, "LEFT\nmid  20ms 50%" },
        { MOTOR_DIR_LEFT,  40, 50, 3000, "LEFT\nslow 40ms 50%" },
        { MOTOR_DIR_RIGHT, 10, 50, 3000, "RIGHT\nfast 10ms 50%" },
        { MOTOR_DIR_RIGHT, 20, 50, 3000, "RIGHT\nmid  20ms 50%" },
        { MOTOR_DIR_RIGHT, 40, 50, 3000, "RIGHT\nslow 40ms 50%" },
        { MOTOR_DIR_LEFT,  20, 25, 3000, "LEFT\nmid  20ms 25%" },
        { MOTOR_DIR_LEFT,  20, 35, 3000, "LEFT\nmid  20ms 35%" },
        { MOTOR_DIR_LEFT,  20, 60, 3000, "LEFT\nmid  20ms 60%" },
        { MOTOR_DIR_LEFT,  20, 80, 3000, "LEFT\nmid  20ms 80%" },
        { MOTOR_DIR_LEFT,  15, 40, 2000, "LEFT  → STOP\n→ RIGHT" },
        { MOTOR_DIR_RIGHT, 15, 40, 2000, "RIGHT → STOP\n→ LEFT"  },
        { MOTOR_DIR_LEFT,   8, 40, 3000, "LEFT\nvery fast 8ms" },
        { MOTOR_DIR_RIGHT,  8, 40, 3000, "RIGHT\nvery fast 8ms" },
        { MOTOR_DIR_LEFT,  60, 60, 3000, "LEFT\nvery slow 60ms" },
    };
    const int n = sizeof(scenarios) / sizeof(scenarios[0]);

    while (1) {
        for (int i = 0; i < n; i++) {
            const scenario_t *s = &scenarios[i];
            ESP_LOGI(TAG, "[%2d/%d] %s", i+1, n, s->label);

            motor_set_duty(s->duty);
            motor_set(s->dir, s->step_ms);
            mt_show(i+1, n, s->label, false);
            vTaskDelay(pdMS_TO_TICKS(s->run_ms));

            motor_stop();
            mt_show(i+1, n, s->label, true);
            vTaskDelay(pdMS_TO_TICKS(500));
        }
        ESP_LOGI(TAG, "--- Zyklus abgeschlossen ---");
    }
}

#else
// ═══════════════════════════════════════════════════════════════════
//  NORMAL MODE
// ═══════════════════════════════════════════════════════════════════

// LVGL draw buffer: double-buffered, 1/10 screen height
#define LVGL_DRAW_BUF_LINES  (LCD_V_RES / 10)
#define LVGL_TICK_PERIOD_MS  2

static lv_display_t *s_display  = NULL;
static esp_lcd_panel_handle_t s_panel = NULL;

// ── LCD ──────────────────────────────────────────────────────────────────────

static void lcd_backlight_init(void)
{
    gpio_config_t io_conf = {
        .pin_bit_mask = (1ULL << LCD_PIN_BACKLIGHT),
        .mode         = GPIO_MODE_OUTPUT,
        .pull_up_en   = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type    = GPIO_INTR_DISABLE,
    };
    gpio_config(&io_conf);
    gpio_set_level(LCD_PIN_BACKLIGHT, 1);
}

static esp_lcd_panel_handle_t lcd_init(void)
{
    spi_bus_config_t bus_cfg = {
        .mosi_io_num     = LCD_PIN_DATA,
        .miso_io_num     = -1,
        .sclk_io_num     = LCD_PIN_SCK,
        .quadwp_io_num   = -1,
        .quadhd_io_num   = -1,
        .max_transfer_sz = LCD_H_RES * LVGL_DRAW_BUF_LINES * 2,
    };
    ESP_ERROR_CHECK(spi_bus_initialize(LCD_SPI_HOST, &bus_cfg, SPI_DMA_CH_AUTO));

    esp_lcd_panel_io_handle_t io_handle = NULL;
    esp_lcd_panel_io_spi_config_t io_cfg = {
        .dc_gpio_num       = LCD_PIN_CMD,
        .cs_gpio_num       = LCD_PIN_CS,
        .pclk_hz           = LCD_SPI_FREQ_HZ,
        .lcd_cmd_bits      = LCD_CMD_BITS,
        .lcd_param_bits    = LCD_PARAM_BITS,
        .spi_mode          = 0,
        .trans_queue_depth = 10,
    };
    ESP_ERROR_CHECK(esp_lcd_new_panel_io_spi((esp_lcd_spi_bus_handle_t)LCD_SPI_HOST, &io_cfg, &io_handle));

    esp_lcd_panel_handle_t panel_handle = NULL;
    esp_lcd_panel_dev_config_t panel_cfg = {
        .reset_gpio_num = LCD_PIN_RST,
        .rgb_endian     = LCD_RGB_ENDIAN_BGR,
        .bits_per_pixel = 16,
    };
    ESP_ERROR_CHECK(esp_lcd_new_panel_gc9a01(io_handle, &panel_cfg, &panel_handle));

    ESP_ERROR_CHECK(esp_lcd_panel_reset(panel_handle));
    ESP_ERROR_CHECK(esp_lcd_panel_init(panel_handle));
    ESP_ERROR_CHECK(esp_lcd_panel_mirror(panel_handle, true, false));
    ESP_ERROR_CHECK(esp_lcd_panel_invert_color(panel_handle, true));
    ESP_ERROR_CHECK(esp_lcd_panel_disp_on_off(panel_handle, true));

    return panel_handle;
}

// ── LVGL callbacks ───────────────────────────────────────────────────────────

static void lvgl_flush_cb(lv_display_t *disp, const lv_area_t *area, uint8_t *px_map)
{
    esp_lcd_panel_handle_t panel = lv_display_get_user_data(disp);
    esp_lcd_panel_draw_bitmap(panel,
                              area->x1, area->y1,
                              area->x2 + 1, area->y2 + 1,
                              (uint16_t *)px_map);
    lv_display_flush_ready(disp);
}

static void lvgl_tick_cb(void *arg)
{
    lv_tick_inc(LVGL_TICK_PERIOD_MS);
}

// ── LVGL task ────────────────────────────────────────────────────────────────

static void lvgl_task(void *arg)
{
    ESP_LOGI(TAG, "LVGL task started");
    while (1) {
        uint32_t sleep_ms = lv_timer_handler();
        vTaskDelay(pdMS_TO_TICKS(sleep_ms > 0 ? sleep_ms : 1));
    }
}

// ── app_main ─────────────────────────────────────────────────────────────────

static volatile motor_dir_t s_current_dir = MOTOR_DIR_LEFT;

// Returns shortest signed difference a-b in [-180, 180]
static float angle_diff(float a, float b)
{
    float d = a - b;
    while (d >  180.0f) d -= 360.0f;
    while (d < -180.0f) d += 360.0f;
    return d;
}

static float angle_normalize(float a)
{
    while (a <   0.0f) a += 360.0f;
    while (a >= 360.0f) a -= 360.0f;
    return a;
}

#define TRIGGER_DEG   20.0f   // knob movement to trigger motor
#define TARGET_DEG   180.0f   // how far motor should turn
#define REACH_TOL      3.0f   // tolerance for target reached
#define SLOW_DEG      40.0f   // degrees before target to start slowing down
#define STEP_MS_FAST  15      // normal speed
#define STEP_MS_SLOW  35      // reduced speed near target

static void motor_control_task(void *arg)
{
    // wait for sensor to stabilise
    vTaskDelay(pdMS_TO_TICKS(200));

    float ref = mag_sensor_read_degrees();

    while (1) {
        float current = mag_sensor_read_degrees();
        if (current < 0.0f) {
            vTaskDelay(pdMS_TO_TICKS(10));
            continue;
        }

        float delta = angle_diff(current, ref);

        if (delta >= TRIGGER_DEG) {
            // knob turned left → drive motor 180° left
            float target = angle_normalize(current - TARGET_DEG);
            ESP_LOGI(TAG, "Trigger LEFT  cur=%.1f target=%.1f", current, target);

            s_current_dir = MOTOR_DIR_LEFT;
            leds_set_dir(MOTOR_DIR_LEFT);
            motor_set(MOTOR_DIR_LEFT, STEP_MS_FAST);

            while (1) {
                float pos = mag_sensor_read_degrees();
                if (pos < 0.0f) { vTaskDelay(pdMS_TO_TICKS(5)); continue; }
                float remaining = fabsf(angle_diff(pos, target));
                if (remaining <= REACH_TOL) break;
                motor_set(MOTOR_DIR_LEFT, remaining <= SLOW_DEG ? STEP_MS_SLOW : STEP_MS_FAST);
                vTaskDelay(pdMS_TO_TICKS(5));
            }
            motor_stop();
            ref = mag_sensor_read_degrees();
            ESP_LOGI(TAG, "Reached LEFT  pos=%.1f", ref);

        } else if (delta <= -TRIGGER_DEG) {
            // knob turned right → drive motor 180° right
            float target = angle_normalize(current + TARGET_DEG);
            ESP_LOGI(TAG, "Trigger RIGHT cur=%.1f target=%.1f", current, target);

            s_current_dir = MOTOR_DIR_RIGHT;
            leds_set_dir(MOTOR_DIR_RIGHT);
            motor_set(MOTOR_DIR_RIGHT, STEP_MS_FAST);

            while (1) {
                float pos = mag_sensor_read_degrees();
                if (pos < 0.0f) { vTaskDelay(pdMS_TO_TICKS(5)); continue; }
                float remaining = fabsf(angle_diff(pos, target));
                if (remaining <= REACH_TOL) break;
                motor_set(MOTOR_DIR_RIGHT, remaining <= SLOW_DEG ? STEP_MS_SLOW : STEP_MS_FAST);
                vTaskDelay(pdMS_TO_TICKS(5));
            }
            motor_stop();
            ref = mag_sensor_read_degrees();
            ESP_LOGI(TAG, "Reached RIGHT pos=%.1f", ref);

        } else {
            vTaskDelay(pdMS_TO_TICKS(10));
        }
    }
}

static volatile float s_last_lux = 0.0f;

static void lux_task(void *arg)
{
    while (1) {
        s_last_lux = light_sensor_read_lux();
        vTaskDelay(pdMS_TO_TICKS(500));
    }
}

static volatile uint32_t s_last_strain = 0;

static void strain_task(void *arg)
{
    while (1) {
        s_last_strain = strain_sensor_read();
#ifdef LOG_STRAIN
        ESP_LOGI(TAG, "STRAIN delta=%lu", (unsigned long)s_last_strain);
#endif
        vTaskDelay(pdMS_TO_TICKS(500));
    }
}

#ifdef LOG_HEAP
static void heap_task(void *arg)
{
    while (1) {
        ESP_LOGI(TAG, "HEAP free=%lu min=%lu",
                 (unsigned long)esp_get_free_heap_size(),
                 (unsigned long)esp_get_minimum_free_heap_size());

#ifdef LOG_STACK
        // vTaskList() gibt Tabelle: Name | State | Prio | FreeStack(Words) | TaskNum
        // FreeStack = minimaler freier Stack seit Task-Start (in 4-Byte-Words).
        // Faustregel: < 64 Words (~256 B) = kritisch, Stack-Größe erhöhen!
        char buf[512];
        vTaskList(buf);
        ESP_LOGI(TAG, "STACK WATERMARKS (min free words):\n%s", buf);
#endif

        vTaskDelay(pdMS_TO_TICKS(5000));
    }
}
#endif

static void sensor_update_cb(lv_timer_t *t)
{
    (void)t;
    float deg = mag_sensor_read_degrees();
    ui_update_angle(deg, s_current_dir);
    ui_update_lux(s_last_lux);
    ui_update_strain(s_last_strain);
}

void app_main(void)
{
    ESP_LOGI(TAG, "SmartKnob starting...");

    lcd_backlight_init();
    s_panel = lcd_init();
    ESP_LOGI(TAG, "LCD ready (%dx%d)", LCD_H_RES, LCD_V_RES);

    lv_init();

    // periodic tick via esp_timer
    const esp_timer_create_args_t tick_timer_args = {
        .callback = lvgl_tick_cb,
        .name     = "lvgl_tick",
    };
    esp_timer_handle_t tick_timer;
    ESP_ERROR_CHECK(esp_timer_create(&tick_timer_args, &tick_timer));
    ESP_ERROR_CHECK(esp_timer_start_periodic(tick_timer, LVGL_TICK_PERIOD_MS * 1000));

    // display + draw buffers
    s_display = lv_display_create(LCD_H_RES, LCD_V_RES);
    lv_display_set_user_data(s_display, s_panel);
    lv_display_set_flush_cb(s_display, lvgl_flush_cb);

    static lv_color_t buf1[LCD_H_RES * LVGL_DRAW_BUF_LINES];
    static lv_color_t buf2[LCD_H_RES * LVGL_DRAW_BUF_LINES];
    lv_display_set_buffers(s_display, buf1, buf2, sizeof(buf1), LV_DISPLAY_RENDER_MODE_PARTIAL);

    // LEDs
    leds_init();
    leds_start_task();

    // Motor (TMC6300)
    motor_init();
    motor_set_duty(35);
    // Core 0: Motor-Regelung zusammen mit motor_task auf gleichem Core
    xTaskCreatePinnedToCore(motor_control_task, "motor_ctrl", 6144, NULL, 4, NULL, 0);

    // Magnetic sensor (MT6701)
    mag_sensor_init();

    // Ambient light sensor (VEML7700)
    light_sensor_init();
    // Core 1: UI/Sensor-Core
    xTaskCreatePinnedToCore(lux_task, "lux", 3072, NULL, 3, NULL, 1);

    // Strain sensor (HX711)
    strain_sensor_init();
    xTaskCreatePinnedToCore(strain_task, "strain", 3072, NULL, 3, NULL, 1);

#ifdef LOG_HEAP
    xTaskCreatePinnedToCore(heap_task, "heap", 4096, NULL, 1, NULL, 1);
#endif

    // initial UI
    ui_init(s_display);

    // LVGL timer: reads sensor and updates UI every 50ms
    lv_timer_create(sensor_update_cb, 100, NULL);

    xTaskCreatePinnedToCore(lvgl_task, "lvgl", 6144, NULL, 5, NULL, 1);

    ESP_LOGI(TAG, "Init complete");
}

#endif // MOTOR_TEST
