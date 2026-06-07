idf_component_register(SRCS "main.c" "ui.c" "leds.c" "motor.c" "mag_sensor.c" "light_sensor.c" "strain_sensor.c"
                    INCLUDE_DIRS "."
                    REQUIRES esp_lcd driver esp_timer esp_driver_mcpwm)
