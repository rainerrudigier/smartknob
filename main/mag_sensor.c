#include "mag_sensor.h"
#include "pins.h"
#include "driver/gpio.h"
#include "esp_log.h"
#include "rom/ets_sys.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"

static const char *TAG = "mag";
static SemaphoreHandle_t s_mutex = NULL;

// SSI timing: CLK half-period in µs (max ~25 MHz; 2µs = 250 kHz, safe for bit-bang)
#define SSI_HALF_PERIOD_US  2

// MT6701 SSI frame: 14 bits angle (MSB first) + 1 parity bit + 1 error bit = 16 bits
#define SSI_TOTAL_BITS      16
#define SSI_ANGLE_BITS      14

void mag_sensor_init(void)
{
    gpio_config_t out_cfg = {
        .pin_bit_mask = (1ULL << MAG_CSN) | (1ULL << MAG_CLK),
        .mode         = GPIO_MODE_OUTPUT,
        .pull_up_en   = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type    = GPIO_INTR_DISABLE,
    };
    gpio_config(&out_cfg);

    gpio_config_t in_cfg = {
        .pin_bit_mask = (1ULL << MAG_DO),
        .mode         = GPIO_MODE_INPUT,
        .pull_up_en   = GPIO_PULLUP_ENABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type    = GPIO_INTR_DISABLE,
    };
    gpio_config(&in_cfg);

    // idle state: CSN high, CLK low
    gpio_set_level(MAG_CSN, 1);
    gpio_set_level(MAG_CLK, 0);

    s_mutex = xSemaphoreCreateMutex();
    ESP_LOGI(TAG, "MT6701 SSI ready");
}

int32_t mag_sensor_read_raw(void)
{
    if (xSemaphoreTake(s_mutex, pdMS_TO_TICKS(10)) != pdTRUE) return -1;

    uint32_t raw = 0;

    // start transfer: CSN low
    gpio_set_level(MAG_CSN, 0);
    ets_delay_us(SSI_HALF_PERIOD_US);

    // clock out 16 bits; data changes on falling CLK edge, sample on rising edge
    for (int i = SSI_TOTAL_BITS - 1; i >= 0; i--) {
        gpio_set_level(MAG_CLK, 1);
        ets_delay_us(SSI_HALF_PERIOD_US);

        if (gpio_get_level(MAG_DO)) {
            raw |= (1u << i);
        }

        gpio_set_level(MAG_CLK, 0);
        ets_delay_us(SSI_HALF_PERIOD_US);
    }

    // end transfer: CSN high
    gpio_set_level(MAG_CSN, 1);
    xSemaphoreGive(s_mutex);

    // bit 1 = error flag (1 = error)
    if (raw & 0x01) {
        ESP_LOGW(TAG, "MT6701 error flag set (raw=0x%04lX)", (unsigned long)raw);
        return -1;
    }

    // bits 15..2 = 14-bit angle
    return (int32_t)((raw >> 2) & 0x3FFF);
}

float mag_sensor_read_degrees(void)
{
    int32_t raw = mag_sensor_read_raw();
    if (raw < 0) return -1.0f;
    return (float)raw * 360.0f / 16384.0f;
}
