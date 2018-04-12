#include "stdint.h"

// When fading in up to a new lower bound on the LED brightness, use this speed for all animations
// Default: 5
#define SETUP_FADE_SPEED_MAJOR 5

#define PIN_LED_START 2
#define NUM_LEDS 10
#define PIN_LED_END (PIN_LED_START+NUM_LEDS)

// If something goes wrong, these 2 LEDs will be statically on
#define PIN_LED_ERR0 2
#define PIN_LED_ERR1 7

// PWM frequency, setting this to 100 means PWM_STEPS * 100 calls per second to callback() to update the PWM, and LEDs will turn off and on 100 times a second
// More is better (prevents flicker), but too high will eat up CPU without leaving time for the animation resulting in choppy animations
// Default: 200 Hz
#define TIMER_FREQ 100

// Fader update frequency, setting this to 10 means a fade in or out is updated 10 times per second. Note that changing this setting changes the speed of a fade.
// Note: since the PWM frequency and length dictates the timer interval, this setting will be converted in a derrived frequency.
#define FADER_UPDATE_FREQ 15

// DO NOT CHANGE - PWM length - note: should be a power of 2 and matching of the type of _pwm_step
#define PWM_STEPS 255

// DO NOT CHANGE - Timer delay in us based on the requested update frequency
#define TIMER_INTERVAL_US (1000000 / ((uint32_t)(TIMER_FREQ) * (uint32_t)(PWM_STEPS)))

// DO NOT CHANGE - Compute how many timer interrupts are needed of the PWM update to span one interval for the fader update frequency
#define FADER_UPDATE_INTERVAL_US ( 1000000 / (uint32_t)(FADER_UPDATE_FREQ) )
// FIXME For some reason it seems the frequency is off by a factor three: at 50 Hz, a fader with delta set to major 5, in one second the value 250 should be reached, but it takes roughly 3 seconds instead...
#define FADER_UPDATE_TICKS       (( FADER_UPDATE_INTERVAL_US ) / ( ( TIMER_INTERVAL_US ) * 3) )

#if FADER_UPDATE_FREQ > TIMER_FREQ
#error "FADER_UPDATE_FREQ can not be higher than TIMER_FREQ (it also makes no sense)"
#endif

#if FADER_UPDATE_FREQ < 10
#error "FADER_UPDATE_FREQ is less than 10 Hz (this will result in visible jumps in brightness during fading)"
#endif

#if FADER_UPDATE_FREQ * 3 > TIMER_FREQ
#error "FADER_UPDATE_FREQ is less than 3 times TIMER_FREQ (this means the fader will update way too often)"
#endif

#define barrier() asm volatile("": : :"memory")

typedef enum {
  NONE,           // Do nothing, mark fader inactive when reaching target
  UPPER_INVERT,   // Invert the delta and fade out at the same pace, when reaching the lower bound turn inactive (single blink)
  LOWER_INVERT,   // Invert the delta and fade in at the same pace, when reaching the upper bound, turn inactive (single blink off)
  JUMP,           // Depending on delta, jump to the other bound (upper or lower) and start fading in or out again (sawtooth)
  INVERT,         // Invert the delta and thus fade in or out
  SETUP_LOWER     // Special fade: when a LED has brightness below the lower boundary (due to animations changing), this mode will fade up to the lower boundary
} effect_enum_t;

typedef struct {
  volatile int16_t       delta;   // step size for the animation, note that this is a 16-bit value in order to do smooth sub-step fades
  volatile int8_t        active;  // if the fader for this LED is active
  volatile effect_enum_t reload;  // what to do when the end of the fade (up or down) is reached
  volatile uint8_t       upper;   // upper bound for the fader - default is 255
  volatile uint8_t       lower;   // lower bound for the fader - default is 0
} fader_struct_t;

typedef union {
  struct {
    uint8_t minor;
    uint8_t major;
  } __attribute__ ((packed));
  uint16_t raw;
} duint8_t; // double uint8_t

/**
 * Utility function to quickly configure a fader to fade to the lower bound
 */
static inline void setup_fade_to_lower(fader_struct_t *f, duint8_t *val) {
  if(f->lower == val->major) {
    // Nothing to do, fader and LED have same value, disable fader
    f->active = 0;
    val->minor = 0;
  } else if(f->lower < val->major) {
    // LED is brighter, fade down
    f->reload = NONE;
    if(f->delta > 0) f->delta = -f->delta; // Reverse fade direction to fade out
    f->active = 1;
  } else {
    // LED is dimmer, fade in
    f->reload = SETUP_LOWER;
    f->delta = SETUP_FADE_SPEED_MAJOR << 8;
    f->active = 1;
  }
}

