#pragma once
#include "driver/spi_master.h"

// LCD (GC9A01) via SPI2
#define LCD_PIN_CMD         21   // DC
#define LCD_PIN_CS          26
#define LCD_PIN_DATA        39   // MOSI
#define LCD_PIN_SCK         40
#define LCD_PIN_RST         41
#define LCD_PIN_BACKLIGHT   38

// HX711 strain sensor (bit-bang)
#define STRAIN_SCK  13
#define STRAIN_DO   14

// VEML7700 ambient light sensor (I2C)
#define VEML7700_SDA    1
#define VEML7700_SCL    2
#define VEML7700_I2C    I2C_NUM_0

// MT6701 magnetic angle sensor (SSI)
#define MAG_CSN     10
#define MAG_CLK     11
#define MAG_DO      12

// TMC6300 BLDC motor driver
#define TMC_VL      3
#define TMC_WL      4
#define TMC_UL      5
#define TMC_UH      6
#define TMC_VH      7
#define TMC_WH      8
#define TMC_DIAG    9

// SK6812 RGB LEDs
#define LED_STRIP_PIN       15
#define LED_STRIP_COUNT     8

#define LCD_H_RES           240
#define LCD_V_RES           240
#define LCD_SPI_HOST        SPI2_HOST
#define LCD_SPI_FREQ_HZ     (40 * 1000 * 1000)
#define LCD_CMD_BITS        8
#define LCD_PARAM_BITS      8
