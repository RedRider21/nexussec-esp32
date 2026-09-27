/*
 * board_pins.h — mappa pin UFFICIALE della scheda ESP32-DIV V2 (CiferTech).
 * ------------------------------------------------------------------------
 * Ricavata dai sorgenti CiferTech (github.com/cifertech/ESP32-DIV,
 * ESP32-DIV/shared.h + Libraries/User_Setup v2.h) — ramo BOARD_ESP32_DIV_V2.
 *
 * Tre bus/aree distinte:
 *   1) DISPLAY (ILI9341, bus SPI dedicato)  -> pin nelle -D di platformio.ini
 *   2) RADIO+SD (bus SPI condiviso 12/13/11) -> CC1101, NRF24, microSD
 *   3) BUTTONS su espansore I2C PCF8574      -> non GPIO nativi (vedi nota)
 *
 * NB: su questa scheda alcuni pin sono CONDIVISI tra sottosistemi usati uno
 *     alla volta (menu-driven): IR e NRF24-scan usano gli stessi 14/21.
 */
#pragma once

// Selettore piattaforma: nel simulatore Wokwi usiamo i tasti nativi;
// sulla scheda reale i tasti sono sull'espansore PCF8574 (I2C).
// (Definire NXS_TARGET_SIM=1 nelle build_flags per il simulatore.)
#ifndef NXS_TARGET_SIM
#define NXS_TARGET_SIM 1
#endif

// ---------------------------------------------------------------------------
// 1) DISPLAY ILI9341 (bus SPI dedicato) — impostati via -D in platformio.ini:
//    TFT_MISO 37, TFT_MOSI 35, TFT_SCLK 36, TFT_CS 17, TFT_DC 16, TFT_RST 0
//    TOUCH_CS 18 (niente pin di backlight: retroilluminazione sempre ON)
// ---------------------------------------------------------------------------

// ---------------------------------------------------------------------------
// 2) BUS SPI CONDIVISO RADIO + microSD
// ---------------------------------------------------------------------------
#define RADIO_SPI_SCK   12
#define RADIO_SPI_MISO  13
#define RADIO_SPI_MOSI  11

// microSD
#define SD_CS_PIN       10

// CC1101 (Sub-GHz) — stesso bus radio
#define CC1101_CS_PIN   5
#define CC1101_GDO0_PIN 6    // = SUBGHZ_TX_PIN
#define CC1101_GDO2_PIN 3    // = SUBGHZ_RX_PIN

// NRF24L01 (3 moduli). Lo "scanner" 2.4 GHz usa il modulo 3 (CE14/CSN21).
#define NRF1_CE_PIN     15
#define NRF1_CSN_PIN    4
#define NRF2_CE_PIN     47
#define NRF2_CSN_PIN    48
#define NRF3_CE_PIN     14
#define NRF3_CSN_PIN    21
#define NRF_SCAN_CE     NRF3_CE_PIN
#define NRF_SCAN_CSN    NRF3_CSN_PIN

// Infrarossi (CONDIVIDONO i pin del NRF24 modulo 3: uso alternato)
#define IR_RX_PIN       21
#define IR_TX_PIN       14

// GPS Neo-6M (UART2, opzionale/esterno). ATTENZIONE: sulla V2 questi pin
// COINCIDONO con CC1101 (RX5=CC1101_CS, TX6=CC1101_GDO0) -> uso alternato
// (GPS in wardriving, CC1101 in sub-GHz: mai insieme).
#define GPS_UART_RX     5
#define GPS_UART_TX     6
#define GPS_UART_BAUD   9600

// Buzzer: non presente sulla V2 (-1)
#define BUZZER_PIN      -1

// ---------------------------------------------------------------------------
// 3) TASTI su PCF8574 (I2C, indirizzo auto-detect 0x20..0x27)
//    Bit espansore: UP/DOWN/LEFT/RIGHT/SELECT. I pin I2C SDA/SCL della V2
//    NON sono in un #define nei sorgenti (usano Wire di default): DA CONFERMARE
//    sullo schema prima di usare i tasti fisici reali.
// ---------------------------------------------------------------------------
#define PCF8574_ADDR_MIN 0x20
#define PCF8574_ADDR_MAX 0x27
// TODO(reale): confermare SDA/SCL della V2 e mappare i tasti PCF.

// ---------------------------------------------------------------------------
// Tasti di navigazione usati dal firmware
//   - SIM (Wokwi): GPIO nativi 4/5/6 (vedi diagram.json)
//   - REALE: da instradare sui bit del PCF8574 (TODO)
// ---------------------------------------------------------------------------
#if NXS_TARGET_SIM
// SIM (Wokwi): GPIO nativi. ATTENZIONE: sulla scheda REALE 4/5/6 sono pin radio
// (CSN1/CC1101_CS/SUBGHZ_TX) -> per questo sul reale i tasti stanno sul PCF8574.
#define BTN_PREV_PIN 4
#define BTN_OK_PIN   5
#define BTN_NEXT_PIN 6
#else
// REALE: tasti sull'espansore PCF8574 (I2C). SDA/SCL V2 non nei sorgenti:
// -1 => usa i pin Wire di default (TODO: confermare sullo schema).
#define I2C_SDA_PIN  -1
#define I2C_SCL_PIN  -1
// Bit del PCF8574 per i tasti (TODO: confermare sullo schema V2)
#define PCF_BTN_PREV 3   // LEFT
#define PCF_BTN_OK   6   // SELECT
#define PCF_BTN_NEXT 4   // RIGHT
#endif

// Touch ILI9341 (XPT2046, TOUCH_CS 18) — sempre attivo come input alternativo.
// Range ADC grezzo (da Touchscreen.h CiferTech): 300..3800 su X e Y.
#define TOUCH_RAW_MIN 300
#define TOUCH_RAW_MAX 3800
