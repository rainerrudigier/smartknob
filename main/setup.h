#pragma once

// ═══════════════════════════════════════════════════════════════════════════
//  setup.h – Zentrale Projekt-Konfiguration
//
//  Alle Compile-Zeit-Schalter die das Verhalten der Firmware steuern sind
//  hier gesammelt. Defines einkommentieren um die jeweilige Funktion zu
//  aktivieren.
// ═══════════════════════════════════════════════════════════════════════════


// ── Betriebs-Modus ──────────────────────────────────────────────────────────

// Motor-Isolationstest: Nur Motor + Display aktiv, alle anderen Sensoren
// (MT6701, VEML7700, HX711, SK6812) werden nicht initialisiert.
// Im Test-Modus läuft ein automatischer Zyklus durch 15 Szenarien mit
// verschiedenen Richtungen, Schrittzeiten und Duty-Cycles.
// #define MOTOR_TEST


// ── Motor-Ansteuerung ───────────────────────────────────────────────────────

// Sinus-Kommutierung statt Block-Kommutierung für den TMC6300.
// Block: diskreter 6-Schritt-Betrieb (60° pro Schritt), einfach, geringe CPU-Last.
// Sinus: kontinuierlich sinusförmige Phasenströme (120° versetzt), glatteres
//        Drehmoment, weniger Vibrationen, empfohlen für Produktionsbetrieb.
#define MOTOR_SINE_COMM


// ── Debug / Logging ─────────────────────────────────────────────────────────

// HX711 Kraftsensor: Rohwert (nach Tare) alle 500 ms auf der Konsole ausgeben.
// Nützlich zur Kalibrierung von STRAIN_THRESHOLD und STRAIN_MAX in ui.c.
// #define LOG_STRAIN

// MT6701 Magnetsensor: Winkelwert alle 100 ms auf der Konsole ausgeben.
// #define LOG_MAG_SENSOR

// Motor-Regelung: Trigger-, Ziel- und Ziel-Erreicht-Meldungen ausgeben.
// Hilfreich zur Diagnose des Closed-Loop-Verhaltens im Normal-Modus.
// #define LOG_MOTOR_CTRL

// VEML7700 Lichtsensor: Lux-Messwert alle 500 ms auf der Konsole ausgeben.
// #define LOG_LUX

// Heap-Monitor: freien Heap-Speicher alle 5 Sekunden ausgeben.
// Nützlich zur Erkennung von Memory-Leaks oder knappem RAM.
// #define LOG_HEAP
