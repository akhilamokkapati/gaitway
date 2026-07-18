// pins.h, Gaitway hub (XIAO ESP32-C3). ALL hub pin assignments live here.
// VERIFY every pin against the physical XIAO ESP32-C3 silkscreen before flashing.
// No em dashes.
//
// UART budget on the C3: 2 hardware UARTs plus USB. This design needs only ONE
// hardware UART: Serial1 carries the right-thigh RVC frames IN (RX) and the
// merged GW1 + GV1 command stream OUT (TX). CSV logging goes over USB (Serial).
#pragma once

// ---------------------------------------------------------------------------
// Waist BNO085, I2C mode, addr 0x4A, wires under 10 cm, 400 kHz.
// XIAO ESP32-C3 default I2C: SDA = D4 (GPIO6), SCL = D5 (GPIO7).
// ---------------------------------------------------------------------------
#define PIN_WAIST_SDA        6    // D4
#define PIN_WAIST_SCL        7    // D5
#define WAIST_BNO_I2C_ADDR   0x4A

// ---------------------------------------------------------------------------
// Serial1 (UART1): both directions on the single output-node link.
//   RX  = right-thigh BNO085 in UART-RVC mode (PS0 high), sensor TX into here.
//   TX  = merged GW1 (motor) + GV1 (GVS) command lines to the output node,
//         which routes them by prefix to the ODrive and the GVS DAC.
// ---------------------------------------------------------------------------
#define PIN_RT_RVC_RX        20   // D7
#define PIN_CMD_TX           21   // D6

// ---------------------------------------------------------------------------
// Status LED. The XIAO ESP32-C3 has no broken-out user LED, so wire an external
// LED (with a series resistor) from this pin to GND. Active high.
// ---------------------------------------------------------------------------
#define PIN_STATUS_LED       4    // D2
#define STATUS_LED_ACTIVE_LOW 0

// NOTE: CSV logging and the 'c' calibration command use USB (Serial), so no
// hardware UART pins are spent on the laptop link.
