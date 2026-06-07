#include "strain_sensor.h"
#include "pins.h"
#include "driver/gpio.h"
#include "esp_log.h"
#include "rom/ets_sys.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static const char *TAG = "strain";

// Ruhe-Referenzwert: bei init gemessen, danach fest
static uint32_t s_baseline = 0;

// SCK pulse timings from datasheet: T3 high ≥0.2µs, T4 low ≥0.2µs
#define SCK_HALF_US  1

// Sentinel: Lesefehler (DOUT timeout)
#define READ_ERROR   UINT32_MAX

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

    // Baseline: Mittelwert aus 4 Messungen im Ruhezustand
    uint32_t sum = 0;
    int valid = 0;
    for (int i = 0; i < 4; i++) {
        uint32_t v = strain_sensor_read_raw();
        if (v != READ_ERROR) { sum += v; valid++; }
        vTaskDelay(pdMS_TO_TICKS(10));
    }
    s_baseline = (valid > 0) ? (sum / valid) : (1u << 23);  // Fallback: Mittelpunkt
    ESP_LOGI(TAG, "HX711 ready, baseline=%lu", (unsigned long)s_baseline);
}

bool strain_sensor_ready(void)
{
    return gpio_get_level(STRAIN_DO) == 0;
}

// Liest rohen 24-Bit Unsigned-Wert (0..16777215).
// Gibt READ_ERROR (UINT32_MAX) bei Timeout zurück.
uint32_t strain_sensor_read_raw(void)
{
    // Warten bis DOUT low (Messung bereit), max. 200 ms
    uint32_t waited = 0;
    while (gpio_get_level(STRAIN_DO)) {
        vTaskDelay(pdMS_TO_TICKS(1));
        if (++waited > 200) {
            ESP_LOGW(TAG, "timeout waiting for data");
            return READ_ERROR;
        }
    }

    // 24 Bits MSB-first einlesen
    uint32_t raw = 0;
    for (int i = 0; i < 24; i++) {
        gpio_set_level(STRAIN_SCK, 1);
        ets_delay_us(SCK_HALF_US);
        raw = (raw << 1) | gpio_get_level(STRAIN_DO);
        gpio_set_level(STRAIN_SCK, 0);
        ets_delay_us(SCK_HALF_US);
    }

    // 25. Puls: wählt nächsten Messkanal (Kanal A, Gain 128)
    gpio_set_level(STRAIN_SCK, 1);
    ets_delay_us(SCK_HALF_US);
    gpio_set_level(STRAIN_SCK, 0);
    ets_delay_us(SCK_HALF_US);

    // HX711 gibt MSB invertiert aus → korrigieren (per Datenblatt)
    raw ^= 0x800000;

    return raw & 0x00FFFFFF;  // 24-Bit unsigned, kein Sign-Extend
}

// Gibt absoluten Abstand vom Ruhewert zurück (immer >= 0).
// Gibt READ_ERROR bei Lesefehler zurück.
uint32_t strain_sensor_read(void)
{
    uint32_t raw = strain_sensor_read_raw();
    if (raw == READ_ERROR) return READ_ERROR;

    // Betrag der Abweichung vom Baseline (unsigned-sicher)
    return (raw >= s_baseline) ? (raw - s_baseline) : (s_baseline - raw);
}
