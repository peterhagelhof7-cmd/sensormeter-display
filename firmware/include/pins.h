#pragma once

// Pinbelegung HW-458B (ESP-WROOM-32 + ST7789P3 2,8" 240x320)
//
// TFT- und Touch-Pins sind NICHT im Hersteller-Datenblatt aufgefuehrt
// (CBAA0055-008_DE.pdf listet nur die Touch-Pins). Sie wurden ueber das
// identische Referenzdesign LCDWIKI E32R28T/E32N28T (2.8inch_ESP32-32E-7789)
// bestaetigt: dessen Touch-Pinbelegung (IO25/32/33/36/39) und Backlight-Pin
// (IO21) stimmen exakt mit dem Datenblatt dieses Boards ueberein - siehe
// docs/entscheidungen.md. TFT-Pins gelten daher als sehr wahrscheinlich,
// aber nicht am eigenen Board nachgemessen.

// --- TFT (ST7789P3, 4-Draht-SPI) ---
// TFT_MOSI/MISO/SCLK/CS/DC/RST/BL werden zentral in platformio.ini als
// Build-Flags fuer TFT_eSPI gesetzt (TFT_eSPI liest keine Header-Defines
// zur Laufzeit, sondern Compile-Flags). Hier nur zur Dokumentation:
//   TFT_MOSI = 13   TFT_MISO = 12   TFT_SCLK = 14
//   TFT_CS   = 15   TFT_DC   = 2    TFT_RST  = -1 (an EN, kein eigener Pin)
//   TFT_BL   = 21   (High = an, siehe Datenblatt)
//
// Fuer die Helligkeitsregelung (P5) wird derselbe physische Pin zusaetzlich
// per LEDC-PWM angesteuert (BacklightManager) - TFT_eSPI schaltet ihn beim
// Init nur digital ein, ledcAttach() uebernimmt den Pin danach fuer PWM.
#define BACKLIGHT_PIN 21

// --- Touch (resistiv, XPT2046-artiger Controller, eigener SPI-Bus) ---
#define TOUCH_PIN_CLK 25
#define TOUCH_PIN_CS  33
#define TOUCH_PIN_DIN 32  // Touch-MOSI
#define TOUCH_PIN_OUT 39  // Touch-MISO
#define TOUCH_PIN_IRQ 36

// --- DHT11 ---
// Am Steckverbinder "Erweiterungsanschluss IO2" (GND/IO22/IO27/3.3V).
// GPIO2 selbst ist die TFT-DC-Leitung (siehe oben) und daher belegt -
// entgegen der woertlichen Angabe in den Vorgaenger-Lastenheften.
#define DHT11_PIN 22
#define DHT11_TYPE DHT11

// --- RGB-Status-LED ---
#define LED_R_PIN 17
#define LED_G_PIN 4
#define LED_B_PIN 16

// --- Sonstige (laut Datenblatt, aktuell ungenutzt) ---
#define AUDIO_PIN 26
#define LIGHT_SENSOR_PIN 34
#define SD_CD_PIN 5
#define SD_CMD_PIN 23
#define SD_CLK_PIN 18
#define SD_DAT0_PIN 19
