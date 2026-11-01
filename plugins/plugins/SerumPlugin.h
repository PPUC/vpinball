// license:GPLv3+

#pragma once

// What the Serum plugin publishes beside its colorized display, for a host that
// has something to do with it. Free of libserum, so a host needs neither its
// headers nor its library to listen.

#include <stdint.h>

#define SERUMPI_NAMESPACE "Serum"

// The speaker lights of a colorization, as SerumLightEffectsMsg. With isDefault,
// what the channels show when no effect runs, sent once after a colorization
// loads; channels == 0 then means it has no speaker lights, and the lights
// should go back to their own setting. Otherwise, the effects that frames just
// started. Sent on the main thread.
#define SERUMPI_EVT_ON_LIGHT_EFFECTS "OnLightEffects:1"

#define SERUMPI_LIGHT_CHANNELS 4

typedef struct SerumLightEffect
{
   uint8_t mode;
   uint8_t options;
   uint16_t speed;
   uint32_t durationMs;
   uint32_t colors[3];
   uint8_t brightness;
} SerumLightEffect;

typedef struct SerumLightEffectsMsg
{
   uint32_t isDefault;
   uint32_t channels; // Bit n: effects[n] is set
   SerumLightEffect effects[SERUMPI_LIGHT_CHANNELS];
} SerumLightEffectsMsg;
