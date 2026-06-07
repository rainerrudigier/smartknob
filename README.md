# SmartKnob

Ein haptischer Drehknopf auf Basis eines ESP32-S3, entwickelt mit ESP-IDF v5.5.2.
Der Knopf erkennt Drehbewegungen über einen Magnetsensor und fährt einen BLDC-Motor
als haptisches Feedback. Zusätzlich sind ein Kraftsensor (Press Detection),
ein Umgebungslichtsensor und ein rundes LVGL-Display verbaut.

---

## Hardware

| Komponente | Funktion |
|-----------|----------|
| ESP32-S3 | Mikrocontroller, Dual-Core 160 MHz |
| GC9A01 | 240×240 px rundes SPI-Display |
| MT6701 | 14-Bit Magnetsensor (SSI Bit-Bang) |
| HX711 | 24-Bit ADC für Kraftsensor (Press Detection) |
| VEML7700 | Umgebungslichtsensor (I2C) |
| TMC6300 | 3-Phasen BLDC-Motor-Treiber |
| SK6812 | 8× RGB-LED Ring (RMT) |

---

## Features

- **Haptisches Feedback:** BLDC-Motor mit Sinus-Kommutierung (TMC6300 + MCPWM)
- **Winkelerfassung:** Kontaktloser Magnetsensor MT6701, 14-Bit, 0,022°/LSB
- **Press Detection:** HX711 Kraftsensor erkennt Druckkraft auf den Knopf
- **Ambient Light:** VEML7700 misst Umgebungshelligkeit in Lux
- **LVGL UI:** Runddisplay zeigt Winkel, Richtung, Lux, Press-Ring und Git-Hash
- **LED-Ring:** SK6812 zeigt Drehrichtung farblich an (Grün = Links, Orange = Rechts)

---

## Motor-Ansteuerung

Die Firmware unterstützt zwei Kommutierungsverfahren, umschaltbar in `main/setup.h`:

```c
#define MOTOR_SINE_COMM   // Sinus-Kommutierung (Standard, empfohlen)
// auskommentieren für Block-Kommutierung
```

| Modus | Beschreibung | Empfehlung |
|-------|-------------|------------|
| **Sinus** | Sinusförmige Phasenströme, glatt, leise | Produktionsbetrieb |
| **Block** | 6-Schritt-Kommutierung, einfach | Test / Diagnose |

---

## Build & Flash

**Voraussetzungen:**
- ESP-IDF v5.5.2

```bash
# Konfigurieren + Bauen + Flashen
idf.py build flash monitor

# Vollständiger Neustart (nach sdkconfig.defaults Änderungen)
idf.py fullclean
idf.py build flash monitor
```

---

## Projektstruktur

```
smartknob/
├── main/
│   ├── setup.h          # Zentrale Konfiguration (Betriebs-Modus, Debug-Defines)
│   ├── main.c           # app_main, Tasks, Motor-Regelung
│   ├── motor.c/.h       # TMC6300 BLDC Ansteuerung (Block + Sinus)
│   ├── ui.c/.h          # LVGL UI (Display-Elemente, Update-Funktionen)
│   ├── mag_sensor.c/.h  # MT6701 SSI Bit-Bang
│   ├── strain_sensor.c/.h # HX711 24-Bit ADC
│   ├── light_sensor.c/.h  # VEML7700 I2C
│   ├── leds.c/.h        # SK6812 RMT LED-Strip
│   └── idf_component.yml
├── docs/
│   └── FIRMWARE_OVERVIEW.md  # Detaillierte technische Dokumentation
├── sdkconfig.defaults   # Build-Konfiguration
└── CMakeLists.txt
```

---

## Konfiguration (`main/setup.h`)

Alle Compile-Zeit-Schalter sind zentral in `setup.h` gesammelt:

```c
// Betriebs-Modus
// #define MOTOR_TEST       // Isolationstest: nur Motor + Display

// Motor
#define MOTOR_SINE_COMM     // Sinus-Kommutierung (Standard)

// Debug / Logging (alle default: aus)
// #define LOG_STRAIN       // HX711 Rohwert auf Konsole
// #define LOG_MAG_SENSOR   // MT6701 Winkel auf Konsole
// #define LOG_MOTOR_CTRL   // Motor Trigger/Ziel-Meldungen
// #define LOG_LUX          // VEML7700 Lux auf Konsole
// #define LOG_HEAP         // Heap-Monitor alle 5 s
// #define LOG_STACK        // Stack-Watermarks aller Tasks (nur zur Diagnose)
```

---

## Task-Architektur

Alle Tasks sind per `xTaskCreatePinnedToCore` fest zugewiesen:

**Core 0 – Motor (zeitkritisch)**

| Task | Priorität | Stack | Funktion |
|------|----------:|------:|---------|
| `motor` | 6 | 6144 B | Sinus-Kommutierung (2 ms) |
| `motor_ctrl` | 4 | 6144 B | Closed-Loop Winkelregelung |

**Core 1 – UI / Sensoren**

| Task | Priorität | Stack | Funktion |
|------|----------:|------:|---------|
| `lvgl` | 5 | 6144 B | Display-Rendering |
| `leds` | 4 | 2048 B | SK6812 Lauflicht |
| `lux` | 3 | 3072 B | VEML7700 alle 500 ms |
| `strain` | 3 | 3072 B | HX711 alle 500 ms |
| `heap` | 1 | 4096 B | Heap-Monitor (optional) |

---

## Abhängigkeiten (Managed Components)

| Komponente | Version |
|-----------|---------|
| `lvgl/lvgl` | 9.5.0 |
| `espressif/esp_lcd_gc9a01` | 2.0.4 |
| `espressif/led_strip` | 3.0.3 |
| `espressif/cmake_utilities` | 0.5.3 |

---

## Dokumentation

Detaillierte technische Beschreibung aller Sensoren, Motor-Kommutierungsverfahren,
UI-Elemente, Task-Architektur und Build-Konfiguration:

→ [`docs/FIRMWARE_OVERVIEW.md`](docs/FIRMWARE_OVERVIEW.md)
