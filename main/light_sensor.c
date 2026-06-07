#include "light_sensor.h"
#include "pins.h"
#include "driver/i2c_master.h"
#include "esp_log.h"

static const char *TAG = "light";

// VEML7700 registers
#define VEML7700_ADDR       0x10
#define REG_ALS_CONF        0x00
#define REG_ALS_DATA        0x04

// ALS_CONF: ALS_SD=0 (power on), ALS_GAIN=00 (x1), ALS_IT=0000 (100ms)
#define CONF_POWER_ON       0x0000

// Resolution for gain x1, IT=100ms = 0.0672 lx/bit
#define ALS_RESOLUTION      0.0672f

static i2c_master_bus_handle_t  s_bus    = NULL;
static i2c_master_dev_handle_t  s_dev    = NULL;

static esp_err_t write_reg(uint8_t reg, uint16_t value)
{
    uint8_t buf[3] = { reg, value & 0xFF, (value >> 8) & 0xFF };
    return i2c_master_transmit(s_dev, buf, sizeof(buf), 100);
}

static esp_err_t read_reg(uint8_t reg, uint16_t *out)
{
    uint8_t buf[2];
    esp_err_t ret = i2c_master_transmit_receive(s_dev, &reg, 1, buf, 2, 100);
    if (ret == ESP_OK) {
        *out = (uint16_t)(buf[0] | (buf[1] << 8));
    }
    return ret;
}

void light_sensor_init(void)
{
    i2c_master_bus_config_t bus_cfg = {
        .i2c_port            = VEML7700_I2C,
        .sda_io_num          = VEML7700_SDA,
        .scl_io_num          = VEML7700_SCL,
        .clk_source          = I2C_CLK_SRC_DEFAULT,
        .glitch_ignore_cnt   = 7,
        .flags.enable_internal_pullup = true,
    };
    ESP_ERROR_CHECK(i2c_new_master_bus(&bus_cfg, &s_bus));

    i2c_device_config_t dev_cfg = {
        .dev_addr_length = I2C_ADDR_BIT_LEN_7,
        .device_address  = VEML7700_ADDR,
        .scl_speed_hz    = 100000,
    };
    ESP_ERROR_CHECK(i2c_master_bus_add_device(s_bus, &dev_cfg, &s_dev));

    // power on: ALS_SD = 0
    ESP_ERROR_CHECK(write_reg(REG_ALS_CONF, CONF_POWER_ON));

    // integration time is 100ms, wait one cycle before first read
    vTaskDelay(pdMS_TO_TICKS(110));

    ESP_LOGI(TAG, "VEML7700 ready");
}

float light_sensor_read_lux(void)
{
    uint16_t raw = 0;
    if (read_reg(REG_ALS_DATA, &raw) != ESP_OK) {
        ESP_LOGW(TAG, "I2C read failed");
        return -1.0f;
    }
    return (float)raw * ALS_RESOLUTION;
}
