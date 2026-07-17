// config.h, Gaitway GVS node (existing v1 ESP32 + DAC output stage).
// No em dashes.
//
// ###########################################################################
// #  VERIFY EVERY PIN AND LEVEL BELOW AGAINST THE PHYSICAL v1 GVS BOARD     #
// #  BEFORE THE FIRST POWERED TEST. Bench test with a multimeter on the     #
// #  DAC output first (developer in the loop). Software only sets the DAC    #
// #  level and the polarity pins. The analog output stage and its hardware  #
// #  enforced current ceiling (below 3.5 mA) are NOT in software's hands.    #
// ###########################################################################
#pragma once

// UART from the hub GV1 TX into this node (receive only).
#define PIN_GV1_RX        16      // TODO verify against v1 board wiring

// DAC output pin. Classic ESP32 DAC is GPIO25 or GPIO26 only.
#define PIN_DAC           25      // TODO verify

// Polarity select pins for LEFT vs RIGHT stimulation (H-bridge style select).
// LEFT  = POL_A high, POL_B low.  RIGHT = POL_A low, POL_B high.  OFF = both low.
#define PIN_POL_A         26      // TODO verify
#define PIN_POL_B         27      // TODO verify

// DAC code for "stimulation on" (0..255 via dacWrite). Start conservative and
// confirm the resulting current on the bench. 0 = output off.
#define DAC_LEVEL_ON      64      // TODO verify on the bench, keep conservative

// Fail safe: kill the output after this much silence from the hub (spec 6.2).
#define GV1_SILENCE_MS    2000
