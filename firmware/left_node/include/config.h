// config.h, Gaitway left thigh node (XIAO ESP32-C3). No em dashes.
#pragma once
#include <cstdint>

// ###########################################################################
// #  SET HUB_MAC to the hub's STA MAC address. The hub prints it at boot,   #
// #  and this node prints its own MAC at boot for pairing the other way.    #
// ###########################################################################
static const uint8_t HUB_MAC[6] = { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 };  // TODO SET

static const uint16_t TX_PERIOD_MS      = 20;     // transmit at 50 Hz
static const uint16_t NODE_CAL_FRAMES   = 100;    // 1 s baseline at 100 Hz
static const float    NODE_RATE_LP_ALPHA = 0.4f;  // match the hub rate filter
static const uint8_t  NODE_BNO_ADDR     = 0x4A;

// I2C pins: XIAO ESP32-C3 defaults are SDA=D4(GPIO6), SCL=D5(GPIO7). Wire.begin()
// with no args uses the board defaults. Keep the runs short (this node exists so
// the BNO085 wiring is NOT a long cable). Verify on the board.
