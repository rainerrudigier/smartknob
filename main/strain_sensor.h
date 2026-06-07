#pragma once
#include <stdint.h>
#include <stdbool.h>

// Initialisiert GPIO und misst Baseline im Ruhezustand.
void     strain_sensor_init(void);

// Liest rohen 24-Bit Unsigned-Wert (0..16777215).
// Gibt UINT32_MAX bei Lesefehler zurück.
uint32_t strain_sensor_read_raw(void);

// Gibt absoluten Abstand vom Ruhewert zurück (Press-Magnitude, immer >= 0).
// Gibt UINT32_MAX bei Lesefehler zurück.
uint32_t strain_sensor_read(void);

// Gibt true zurück wenn DOUT low ist (Messung bereit).
bool     strain_sensor_ready(void);
