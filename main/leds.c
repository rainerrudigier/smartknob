#include "leds.h"
#include "pins.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "led_strip.h"
#include "esp_log.h"

static const char *TAG = "leds";
static led_strip_handle_t    s_strip;
static volatile motor_dir_t  s_dir = MOTOR_DIR_LEFT;

void leds_init(void)
{
    led_strip_config_t strip_cfg = {
        .strip_gpio_num          = LED_STRIP_PIN,
        .max_leds                = LED_STRIP_COUNT,
        .led_model               = LED_MODEL_SK6812,
        .color_component_format  = LED_STRIP_COLOR_COMPONENT_FMT_GRB,
        .flags.invert_out        = false,
    };
    led_strip_rmt_config_t rmt_cfg = {
        .clk_src        = RMT_CLK_SRC_DEFAULT,
        .resolution_hz  = 10 * 1000 * 1000,
        .flags.with_dma = false,
    };
    ESP_ERROR_CHECK(led_strip_new_rmt_device(&strip_cfg, &rmt_cfg, &s_strip));
    led_strip_clear(s_strip);
    ESP_LOGI(TAG, "LED strip ready, %d LEDs on GPIO%d", LED_STRIP_COUNT, LED_STRIP_PIN);
}

void leds_set_dir(motor_dir_t dir)
{
    s_dir = dir;
}

static void leds_task(void *arg)
{
    int led = 0;

    while (1) {
        led_strip_clear(s_strip);

        if (s_dir == MOTOR_DIR_LEFT) {
            // D1→D8: index 0..7, Farbe grün
            led_strip_set_pixel(s_strip, led, 0, 180, 0);
        } else {
            // D8→D1: index 7..0, Farbe orange
            led_strip_set_pixel(s_strip, (LED_STRIP_COUNT - 1) - led, 180, 60, 0);
        }

        led_strip_refresh(s_strip);
        vTaskDelay(pdMS_TO_TICKS(150));

        led = (led + 1) % LED_STRIP_COUNT;
    }
}

void leds_start_task(void)
{
    xTaskCreate(leds_task, "leds", 2048, NULL, 4, NULL);
}
