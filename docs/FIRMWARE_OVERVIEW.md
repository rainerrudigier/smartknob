# SmartKnob – Firmware-Dokumentation

**Plattform:** ESP32-S3  
**Framework:** ESP-IDF v5.5.2 (FreeRTOS, MCPWM, RMT, I2C, SPI)  
**Datum:** 2026-06-07

---

## Inhaltsverzeichnis

1. [Hardware-Übersicht](#hardware-übersicht)
2. [Pin-Belegung](#pin-belegung)
3. [Sensoren](#sensoren)
   - [MT6701 – Magnetsensor](#mt6701--magnetsensor)
   - [VEML7700 – Umgebungslichtsensor](#veml7700--umgebungslichtsensor)
   - [HX711 – Kraftsensor](#hx711--kraftsensor)
4. [Aktoren](#aktoren)
   - [GC9A01 – Runddisplay](#gc9a01--runddisplay)
   - [SK6812 – RGB-LEDs](#sk6812--rgb-leds)
   - [TMC6300 – BLDC-Motor-Treiber](#tmc6300--bldc-motor-treiber)
5. [Motor-Ansteuerung](#motor-ansteuerung)
   - [Block-Kommutierung](#1-block-kommutierung)
   - [Sinus-Kommutierung](#2-sinus-kommutierung)
   - [Vergleich beider Varianten](#vergleich-beider-varianten)
6. [Motorregelung (Normal-Modus)](#motorregelung-normal-modus)
7. [Betriebs-Modi](#betriebs-modi)
   - [Normal-Modus](#normal-modus)
   - [Motor-Test-Modus](#motor-test-modus)
8. [Task-Architektur](#task-architektur)
9. [Build-Konfiguration](#build-konfiguration)

---

## Hardware-Übersicht

```
┌─────────────────────────────────────────────────────┐
│                    ESP32-S3                         │
│                                                     │
│  SPI2 ──────────────────────── GC9A01 Display      │
│  IO1/IO2 (I2C) ─────────────── VEML7700            │
│  IO10–IO12 (SSI Bit-Bang) ───── MT6701             │
│  IO13/IO14 (Bit-Bang) ───────── HX711              │
│  IO15 (RMT) ────────────────── SK6812 × 8          │
│  IO3–IO9 (MCPWM + GPIO) ──────── TMC6300           │
└─────────────────────────────────────────────────────┘
```

---

## Pin-Belegung

| GPIO | Funktion           | Peripherie        |
|-----:|--------------------|-------------------|
|    1 | I2C SDA            | VEML7700          |
|    2 | I2C SCL            | VEML7700          |
|    3 | TMC_VL (LS)        | TMC6300           |
|    4 | TMC_WL (LS)        | TMC6300           |
|    5 | TMC_UL (LS)        | TMC6300           |
|    6 | TMC_UH (HS)        | TMC6300 / MCPWM   |
|    7 | TMC_VH (HS)        | TMC6300 / MCPWM   |
|    8 | TMC_WH (HS)        | TMC6300 / MCPWM   |
|    9 | TMC_DIAG (Input)   | TMC6300           |
|   10 | MAG_CSN            | MT6701 SSI        |
|   11 | MAG_CLK            | MT6701 SSI        |
|   12 | MAG_DO             | MT6701 SSI        |
|   13 | STRAIN_SCK         | HX711             |
|   14 | STRAIN_DO          | HX711             |
|   15 | LED_DATA           | SK6812 × 8 (RMT)  |
|   21 | LCD_CMD (DC)       | GC9A01 SPI        |
|   26 | LCD_CS             | GC9A01 SPI        |
|   38 | LCD_BACKLIGHT      | GC9A01            |
|   39 | LCD_DATA (MOSI)    | GC9A01 SPI        |
|   40 | LCD_SCK            | GC9A01 SPI        |
|   41 | LCD_RST            | GC9A01 SPI        |

---

## Sensoren

### MT6701 – Magnetsensor

**Protokoll:** SSI (Serial Synchronous Interface), Bit-Bang  
**Datenleitungen:** CSN (IO10), CLK (IO11), DO (IO12)

Der MT6701 ist ein kontaktloser Magnetsensor, der den Winkel eines Permanentmagneten
über Hall-Effekt erfasst. Er liefert 14-Bit Auflösung, was einer Winkelauflösung
von ca. 0,022° pro LSB entspricht.

**Übertragungsprotokoll (SSI):**
1. CSN → LOW (Chip auswählen)
2. 16 Taktflanken: je eine steigende CLK-Flanke → 1 Bit einlesen
3. Bit-Struktur: `[13:0]` Winkel (14 Bit) + 1 Parity-Bit + 1 Error-Bit
4. CSN → HIGH
5. Halbperiode: 2 µs pro Flanke (`SSI_HALF_PERIOD_US = 2`)

**Thread-Sicherheit:** Die Lesefunktion ist mit einem FreeRTOS-Mutex geschützt,
da mehrere Tasks gleichzeitig auf den Sensor zugreifen können.

**Ausgabe:** Winkel in Grad (0,0° – 359,98°), oder `-1.0f` bei Lesefehler.

---

### VEML7700 – Umgebungslichtsensor

**Protokoll:** I2C (IDF-5.x `i2c_master` API)  
**Adresse:** `0x10`  
**Leitungen:** SDA (IO1), SCL (IO2), Bus: `I2C_NUM_0`

Der VEML7700 misst die Umgebungshelligkeit im sichtbaren Spektrum. Er wird über
zwei Register angesteuert:

| Register | Adresse | Funktion                          |
|----------|---------|-----------------------------------|
| ALS_CONF | `0x00`  | Konfiguration (Gain, IT, Power)   |
| ALS      | `0x04`  | 16-Bit Messwert                   |

**Initialisierung:** `ALS_CONF = 0x0000` → Power-On, Standard-Gain (×1), IT 100 ms.
(Wichtig: Default-Wert nach Power-On ist `0x0001` = Shutdown → muss explizit auf 0 gesetzt werden.)

**Umrechnung:** `Lux = Rohwert × 0,0672`

**Task:** `lux_task` liest alle 500 ms und schreibt in `s_last_lux`.
Der LVGL-Timer-Callback aktualisiert die Anzeige alle 100 ms.

---

### HX711 – Kraftsensor (Strain Gauge)

**Protokoll:** Bit-Bang (2-Draht: SCK IO13, DOUT IO14)

Der HX711 ist ein 24-Bit ADC speziell für Wägezellen und Drucksensoren.
Er arbeitet im Kanal A mit Gain 128.

**Lese-Protokoll:**
1. Warten bis DOUT LOW (Messung bereit, typisch 10–100 ms)
2. 24 SCK-Impulse → 24 Datenbits MSB-first einlesen
3. 25. SCK-Impuls → wählt nächsten Messkanal (Kanal A, Gain 128)
4. Vorzeichenkorrektur: XOR mit `0x800000` (Two's-Complement-Offset)
5. Sign-Extension auf `int32_t`

**Tare-Funktion:** Mittelt 8 Messungen und speichert den Offset.
Alle nachfolgenden Messwerte sind relativ zum Tare-Wert.

**Task:** `strain_task` liest alle 500 ms und schreibt in `s_last_strain`.

---

## Aktoren

### GC9A01 – Runddisplay

**Auflösung:** 240 × 240 px, rund  
**Protokoll:** SPI2, 40 MHz  
**Treiber:** `esp_lcd_gc9a01` (ESP-IDF Managed Component)

**Besonderheiten dieser Hardware:**
- **Mirror:** `esp_lcd_panel_mirror(panel, true, false)` – horizontal gespiegelt,
  da das Panel physisch verdreht montiert ist
- **Color Invert:** `esp_lcd_panel_invert_color(panel, true)` – BGR-Byte-Order
- **Endian:** `LCD_RGB_ENDIAN_BGR`

**LVGL-Integration:**
- Partial-Render-Modus: zwei Draw-Buffer à `240 × 24 px × 2 Byte = 11,5 KB`
- Flush-Callback: übergibt fertig gerenderte Pixel direkt an `esp_lcd_panel_draw_bitmap()`
- Tick-Source: `esp_timer` Interrupt alle 2 ms → `lv_tick_inc(2)`

---

### SK6812 – RGB-LEDs

**Anzahl:** 8 LEDs (D1–D8)  
**Datenleitung:** IO15  
**Protokoll:** WS2812-kompatibel (800 kHz, PWM-kodierte Bits), angesteuert via ESP-IDF RMT

**Anzeigelogik:**

| Motor-Richtung | LEDs          | Farbe  |
|----------------|---------------|--------|
| Links          | D1 → D8       | Grün   |
| Rechts         | D8 → D1       | Orange |

Die Richtungsänderung wird über `leds_set_dir(motor_dir_t dir)` gesetzt.
Ein dedizierter LED-Task schreibt die Farben kontinuierlich über die
`led_strip`-Bibliothek.

---

### TMC6300 – BLDC-Motor-Treiber

Der TMC6300 ist ein 6-Pin-MOSFET-Treiber für bürstenlose Gleichstrommotoren (BLDC)
im 3-Phasen-H-Brücken-Layout. Er treibt die drei Phasen U, V, W jeweils mit
High-Side- (HS) und Low-Side-Schalter (LS).

**Pin-Zuordnung:**

| Signal | GPIO | Seite | Ansteuerung          |
|--------|-----:|-------|----------------------|
| UH     |    6 | HS    | MCPWM Group 0, Op 0  |
| VH     |    7 | HS    | MCPWM Group 0, Op 1  |
| WH     |    8 | HS    | MCPWM Group 0, Op 2  |
| UL     |    5 | LS    | GPIO                 |
| VL     |    3 | LS    | GPIO                 |
| WL     |    4 | LS    | GPIO                 |
| DIAG   |    9 | Input | Fehlerüberwachung    |

**DIAG-Pin:** LOW = OK, HIGH = Übertemperatur/Überstrom-Schutzabschaltung.
Der Motor-Task prüft diesen Pin in jeder Iteration.

---

## Motor-Ansteuerung

Die Firmware unterstützt zwei Kommutierungsverfahren, umschaltbar per Compile-Flag:

```c
// in motor.h:
#define MOTOR_SINE_COMM   // auskommentieren für Block-Kommutierung
```

---

### 1. Block-Kommutierung

> Aktiv wenn `MOTOR_SINE_COMM` **nicht** definiert ist.

**Prinzip:** Zu jedem Zeitpunkt ist genau ein HS- und ein LS-Pin aktiv.
Der Strom fließt immer durch zwei der drei Phasen. Pro Umdrehung werden
6 Schritte (je 60° elektrisch) durchlaufen.

**Kommutierungstabelle:**

| Schritt | HS (MCPWM) | LS (GPIO) | Stromfluss |
|---------|-----------|-----------|------------|
| 0       | UH        | VL        | U → V      |
| 1       | UH        | WL        | U → W      |
| 2       | VH        | WL        | V → W      |
| 3       | VH        | UL        | V → U      |
| 4       | WH        | UL        | W → U      |
| 5       | WH        | VL        | W → V      |

**Drehrichtung:**
- Links:  `step = (step + 5) % 6`  (rückwärts durch die Tabelle)
- Rechts: `step = (step + 1) % 6`  (vorwärts)

**Timing:** `vTaskDelay(step_ms)` zwischen jedem Schritt.
Langsamere Schrittzeiten (> 30 ms) führen ohne PWM zu Stottern
(Rotor oscilliert zwischen Schritten).

**PWM-Strombegrenzung:** Die HS-Pins werden nicht mit 100 % Tastverhältnis
betrieben, sondern mit MCPWM bei 20 kHz und einstellbarem Duty-Cycle.
Standard: **35 %** – reduziert Überschwingen und ermöglicht gleichmäßige
Bewegung auch bei langsamen Schrittzeiten.

---

### 2. Sinus-Kommutierung

> Aktiv wenn `#define MOTOR_SINE_COMM` gesetzt ist *(Standard)*.

**Prinzip:** Statt diskreter 60°-Schritte werden alle drei Phasen mit
sinusförmigen Strömen beaufschlagt, die 120° phasenverschoben sind.
Das erzeugt ein gleichmäßig rotierendes Magnetfeld → glatteres Drehmoment.

**Implementierung:**

```
Phase U: sin(θ)
Phase V: sin(θ - 120°)
Phase W: sin(θ + 120°)
```

Der elektrische Winkel θ wird alle `SINE_TASK_MS = 2 ms` inkrementiert:

```
Δθ = (60° / step_ms) × SINE_TASK_MS
```

*(Äquivalent zur Block-Geschwindigkeit: 1 Block-Schritt = 60° elektrisch)*

**Halbwellen-Aufteilung** (HS MCPWM + LS GPIO):

| Sinuswert      | HS-Pin     | LS-Pin     |
|---------------|------------|------------|
| > +Deadband    | PWM(duty)  | LOW (aus)  |
| < −Deadband    | OFF (aus)  | HIGH (ein) |
| ≈ 0 (Deadband) | OFF        | LOW        |

- **Deadband:** `SINE_DEADBAND = 0.02f` – verhindert Shoot-Through bei Nulldurchgang
- **Duty-Skalierung:** `duty_ticks = |sin(θ)| × s_duty_ticks`

**Vorteil gegenüber Block-Kommutierung:**
- Kein abrupter Stromsprung bei jedem Schritt
- Gleichmäßigeres Drehmoment über die gesamte Umdrehung
- Weniger Vibrationen und Geräusche

---

### Vergleich beider Varianten

| Merkmal                | Block-Kommutierung      | Sinus-Kommutierung         |
|------------------------|-------------------------|----------------------------|
| Komplexität            | Gering                  | Mittel                     |
| CPU-Last               | Sehr gering             | Gering (2 ms Task-Periode) |
| Drehmomentwelligkeit   | Hoch (6× pro Umdr.)     | Niedrig (kontinuierlich)   |
| Laufruhe               | Gut bei ≥ 35 % Duty     | Sehr gut                   |
| Geräuschentwicklung    | Merklich bei LS-Switch  | Gering                     |
| Geeignet für           | Test, einfache Szenarien| Produktionsbetrieb         |
| Compile-Flag           | *(kein Define)*         | `#define MOTOR_SINE_COMM`  |

---

## Motorregelung (Normal-Modus)

Im Normal-Modus läuft eine Closed-Loop-Regelung basierend auf dem MT6701:

```
┌──────────┐    Δangle ≥ 20°    ┌─────────────┐
│ MT6701   │ ──────────────────▶│ motor_set() │
│ (Winkel) │                    │ (dir, speed) │
└──────────┘◀──────────────────└─────────────┘
              Positions-Feedback
```

**Parameter:**

| Konstante     | Wert   | Bedeutung                              |
|---------------|-------:|----------------------------------------|
| `TRIGGER_DEG` | 20,0°  | Mindestauslenkung des Knobs            |
| `TARGET_DEG`  | 180,0° | Motorhub pro Auslösung                 |
| `REACH_TOL`   |  3,0°  | Toleranz für „Ziel erreicht"           |
| `SLOW_DEG`    | 40,0°  | Verbleibender Winkel → Langsamfahrt    |
| `STEP_MS_FAST`|  15 ms | Schrittzeit Normalfahrt                |
| `STEP_MS_SLOW`|  35 ms | Schrittzeit Langsamfahrt               |

**Ablauf:**
1. Knob dreht ≥ 20° → Trigger
2. Motor fährt mit `STEP_MS_FAST` zum Ziel
3. 40° vor Ziel: Umschalten auf `STEP_MS_SLOW`
4. Bei Erreichen des Ziels (±3°): `motor_stop()`
5. Neue Referenzposition setzen

---

## Betriebs-Modi

### Normal-Modus

Alle Peripheriegeräte aktiv. Aktiviert wenn `#define MOTOR_TEST` **nicht** gesetzt ist.

```c
// main.c – Normal-Modus
motor_init();        // BLDC-Treiber
mag_sensor_init();   // MT6701
light_sensor_init(); // VEML7700
strain_sensor_init();// HX711
leds_init();         // SK6812
ui_init(display);    // LVGL-Oberfläche
```

**LVGL-UI:**
- Bogen (Arc) 190 × 190 px, 270° Startwinkel, zeigt Magnetwinkel
- Label: Winkel in Grad, Richtung, Lux-Wert, Kraft-Wert

---

### Motor-Test-Modus

Aktiviert mit `#define MOTOR_TEST` in `main.c`.

Nur Motor + Display aktiv. 15 Szenarien werden automatisch der Reihe nach
abgefahren, um verschiedene Ansteuerungs-Parameter zu evaluieren:

| # | Richtung | step_ms | Duty | Dauer | Beschreibung         |
|---|----------|--------:|-----:|------:|----------------------|
| 1 | Links    |      10 |  50% |  3 s  | Schnell              |
| 2 | Links    |      20 |  50% |  3 s  | Mittel               |
| 3 | Links    |      40 |  50% |  3 s  | Langsam              |
| 4 | Rechts   |      10 |  50% |  3 s  | Schnell              |
| 5 | Rechts   |      20 |  50% |  3 s  | Mittel               |
| 6 | Rechts   |      40 |  50% |  3 s  | Langsam              |
| 7 | Links    |      20 |  25% |  3 s  | Niedriger Duty       |
| 8 | Links    |      20 |  35% |  3 s  | Standard Duty        |
| 9 | Links    |      20 |  60% |  3 s  | Hoher Duty           |
|10 | Links    |      20 |  80% |  3 s  | Sehr hoher Duty      |
|11 | Links    |      15 |  40% |  2 s  | Richtungswechsel     |
|12 | Rechts   |      15 |  40% |  2 s  | Richtungswechsel     |
|13 | Links    |       8 |  40% |  3 s  | Sehr schnell         |
|14 | Rechts   |       8 |  40% |  3 s  | Sehr schnell         |
|15 | Links    |      60 |  60% |  3 s  | Sehr langsam         |

Das Display zeigt Szenario-Nummer, -Bezeichnung und Stop-Status.
Alle LVGL-Aufrufe laufen **im selben Task** (`app_main`) – kein paralleler
LVGL-Zugriff, kein Watchdog-Risiko.

---

## Task-Architektur

### Normal-Modus

| Task              | Stack  | Priorität | Funktion                          |
|-------------------|-------:|----------:|-----------------------------------|
| `motor_task`      | 3072 B |         6 | BLDC-Kommutierung (2 ms Periode)  |
| `lvgl_task`       | 6144 B |         5 | `lv_timer_handler()` + Rendering  |
| `motor_ctrl`      | 3072 B |         4 | Closed-Loop Winkelregelung        |
| `lux_task`        | 2048 B |         3 | VEML7700 alle 500 ms              |
| `strain_task`     | 2048 B |         3 | HX711 alle 500 ms                 |
| `app_main`        | –      |         1 | Initialisierung, danach idle      |

### Motor-Test-Modus

| Task              | Stack  | Priorität | Funktion                          |
|-------------------|-------:|----------:|-----------------------------------|
| `motor_task`      | 3072 B |         6 | BLDC-Kommutierung                 |
| `app_main`        | –      |         1 | Szenarien-Loop + LVGL-Rendering   |

---

## Build-Konfiguration

Relevante Einstellungen aus `sdkconfig.defaults`:

| Parameter                    | Wert      | Begründung                                    |
|------------------------------|-----------|-----------------------------------------------|
| `CONFIG_FREERTOS_HZ`         | 1000      | 1 ms Tick-Auflösung für präzises Motor-Timing |
| CPU-Frequenz                 | 160 MHz   | –                                             |
| Flash                        | 2 MB, DIO, 80 MHz | –                                   |
| XTAL                         | 40 MHz    | ESP32-S3 Standard                             |
| LVGL Farbe                   | 16-Bit    | RGB565                                        |
| LVGL Heap                    | 64 KB     | –                                             |
| LVGL Font                    | Montserrat 14 | –                                         |

> **Hinweis:** `CONFIG_FREERTOS_HZ=1000` ist kritisch für die Motor-Ansteuerung.
> Mit dem Default-Wert 100 Hz (10 ms Auflösung) sind `step_ms`-Werte unter 10 ms
> nicht erreichbar und der Motor stottert bei langsamen Schrittzeiten.
