#include "strain_sensor.h"
#include "pins.h"
#include "driver/gpio.h"
#include "esp_log.h"
#include "rom/ets_sys.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static const char *TAG  = "strain";
static int32_t    s_tare = 0;

// SCK pulse timings from datasheet: T3 high ≥0.2µs, T4 low ≥0.2µs
#define SCK_HALF_US  1

void strain_sensor_init(void)
{
    gpio_config_t sck_cfg = {
        .pin_bit_mask = (1ULL << STRAIN_SCK),
        .mode         = GPIO_MODE_OUTPUT,
        .pull_up_en   = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type    = GPIO_INTR_DISABLE,
    };
    gpio_config(&sck_cfg);

    gpio_config_t do_cfg = {
        .pin_bit_mask = (1ULL << STRAIN_DO),
        .mode         = GPIO_MODE_INPUT,
        .pull_up_en   = GPIO_PULLUP_ENABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type    = GPIO_INTR_DISABLE,
    };
    gpio_config(&do_cfg);

    gpio_set_level(STRAIN_SCK, 0);  // SCK low = normal operation
    ESP_LOGI(TAG, "HX711 ready");
}

bool strain_sensor_ready(void)
{
    return gpio_get_level(STRAIN_DO) == 0;
}

// Read raw 24-bit signed value (blocking wait up to timeout_ms for data ready)
static int32_t read_raw(uint32_t timeout_ms)
{
    // wait for DOUT low (data ready)
    uint32_t waited = 0;
    while (gpio_get_level(STRAIN_DO)) {
        vTaskDelay(pdMS_TO_TICKS(1));
        if (++waited > timeout_ms) {
            ESP_LOGW(TAG, "timeout waiting for data");
            return INT32_MIN;
        }
    }

    // read 24 bits MSB first (from reference C driver in datasheet)
    uint32_t raw = 0;
    for (int i = 0; i < 24; i++) {
        gpio_set_level(STRAIN_SCK, 1);
        ets_delay_us(SCK_HALF_US);
        raw = (raw << 1) | gpio_get_level(STRAIN_DO);
        gpio_set_level(STRAIN_SCK, 0);
        ets_delay_us(SCK_HALF_US);
    }

    // 25th pulse: sets channel A, gain 128 for next conversion
    gpio_set_level(STRAIN_SCK, 1);
    ets_delay_us(SCK_HALF_US);
    gpio_set_level(STRAIN_SCK, 0);
    ets_delay_us(SCK_HALF_US);

    // HX711 outputs MSB inverted → XOR to correct (per datasheet reference driver)
    raw ^= 0x800000;

    // sign-extend 24-bit → 32-bit
    if (raw & 0x800000) {
        return (int32_t)(raw | 0xFF000000);
    }
    return (int32_t)raw;
}

void strain_sensor_tare(void)
{
    // average 8 readings for stable tare
    int32_t sum = 0;
    for (int i = 0; i < 8; i++) {
        int32_t v = read_raw(200);
        if (v == INT32_MIN) { ESP_LOGW(TAG, "tare read failed"); return; }
        sum += v;
    }
    s_tare = sum / 8;
    ESP_LOGI(TAG, "Tare set: %ld", (long)s_tare);
}

int32_t strain_sensor_read(void)
{
    int32_t raw = read_raw(200);
    if (raw == INT32_MIN) return INT32_MIN;
    return raw - s_tare;
}
