// pins.h, Gaitway hub (XIAO ESP32-S3). ALL hub pin assignments live here.
// VERIFY every pin against the physical XIAO ESP32-S3 silkscreen before flashing.
// Avoid strapping pins (GPIO 0, 3, 45, 46) for UART. No em dashes.
#pragma once

// ---------------------------------------------------------------------------
// Waist BNO085, I2C mode, addr 0x4A, wires under 10 cm, 400 kHz.
// Default XIAO I2C: SDA = D4 (GPIO5), SCL = D5 (GPIO6). Confirm on board.
// ---------------------------------------------------------------------------
#define PIN_WAIST_SDA        5   // D4
#define PIN_WAIST_SCL        6   // D5
#define WAIST_BNO_I2C_ADDR   0x4A

// ---------------------------------------------------------------------------
// Right thigh BNO085, UART-RVC mode (PS0 tied high), 115200 8N1, 100 Hz.
// Sensor TX -> hub RX only (RVC is one way, no polling). Assign a free UART RX.
// ---------------------------------------------------------------------------
#define PIN_RT_RVC_RX        44  // sensor TX into hub, TODO verify free on board

// ---------------------------------------------------------------------------
// UART out to Joseph's actuation MCU (GW1 stream). TX only from hub.
// ---------------------------------------------------------------------------
#define PIN_ACT_TX           43  // TODO verify free, avoid strapping pins

// ---------------------------------------------------------------------------
// UART out to GVS node (GV1 stream). TX only from hub.
// ---------------------------------------------------------------------------
#define PIN_GVS_TX           7   // D8, TODO verify free

// ---------------------------------------------------------------------------
// Status LED (calibrated / fault / walking patterns).
// XIAO ESP32-S3 onboard LED is on GPIO21 and is active LOW.
// ---------------------------------------------------------------------------
#define PIN_STATUS_LED       21
#define STATUS_LED_ACTIVE_LOW 1

// NOTE: XIAO ESP32-S3 exposes native USB for the serial monitor, so hardware
// UART0 GPIO pins are free for one of the TX streams above. Confirm at bring up.
