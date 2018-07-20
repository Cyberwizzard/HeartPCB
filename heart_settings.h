/**
 * heart_settings.h - Heart PCB Project - Settings file for project
 * 
 * @author  Berend Dekens <berend@cyberwizzard.nl>
 * @version 1
 * @date    2018.04.13
 * @license GNUGPLv3
 */
#ifndef _HEART_SETTINGS_H_
#define _HEART_SETTINGS_H_

#include "stdint.h"

// ------------------------- Debug Settings ----------------------------

// Define to enable profiling support; slows down the program and enables serial debugging
// Comment out when not debugging the project!
//#define SUPPORT_MEASUREMENTS

// Define to enable measurements within the ISR; slows down the critical section of the interrupt routine so
// only enable when optimizing the interrupt routine.
// Comment out when not debugging the project!
//#define SUPPORT_ISR_MEASUREMENTS

// ------------------------- LED Settings ----------------------------

// When fading in up to a new lower bound on the LED brightness, use this speed for all animations.
// This essentially is used when 'wiping' the previous LED brightness to the start state of the current animation.
// Default: 5
#define SETUP_FADE_SPEED_MAJOR 5

// GPIO pin to LED mapping; by default LED0 is connected to pin 2, LED1 to pin 3 etc.
// !!!WARNING!!!: DO NOT CHANGE THESE SETTINGS AS THE INTERRUPT LOGIC IS HARDCODED TO THE ORIGINAL SETTINGS
#define PIN_LED_START 2
#define NUM_LEDS 10
#define PIN_LED_END (PIN_LED_START+NUM_LEDS)

// ---------------------------- Demo Settings --------------------------------
// Set this to the number of seconds before automatically switching effects (only when demo mode is on)
#define EFFECT_DURATION_S 20

// ---------------------------- Button Settings --------------------------------
#define PIN_BTN0 13
#define PIN_BTN1 12

// ------------------------- PWM and fader Settings ----------------------------

// PWM frequency, setting this to 100 means PWM_STEPS * 100 calls per second to callback() to update the PWM, and LEDs will turn off and on 100 times a second
// More is better (prevents flicker), but too high will eat up CPU without leaving time for the animation resulting in choppy animations
// Default: 75 Hz
#define TIMER_FREQ 75

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

// DO NOT CHANGE - The ISR has been profiled to take between 8us and 20us, together with overhead from other code, it turns out 34us is the minimum interval
// needed to prevent nested interrupts from occuring. Note that only when SUPPORT_NESTED_ISR is defined, is it possible to get nested interrupts, without it ticks of the timer will simply be skipped.
#define ISR_MINIMUM_INTERVAL_US 34

// This define is only needed during development and benchmarking of the ISR (interrupt routine) to enable nested interrupts; after development it should be disabled
//#define SUPPORT_NESTED_ISR

// Compute how many ISR fader ticks (which happen once every 255 ISR calls) make up the duration of the demo mode delay between effects
// FIXME also this is off by a factor 6
#define TIMER_DEMO_CNT_MAX (uint32_t)(((uint64_t)EFFECT_DURATION_S * 1000000 * 6) / ((uint64_t)FADER_UPDATE_INTERVAL_US))

// ------------------------- Error Mode Settings ----------------------------

// If something goes wrong, these 2 LEDs will be statically on
#define PIN_LED_ERR0 2
#define PIN_LED_ERR1 7

// To indicate that an error occured, count up to this value using the ISR to blink the LEDs on PIN_LED_ERR0 and PIN_LED_ERR1
#define ERROR_BLINK_CNT 4000

// Error codes:
// Code 100 - Generic error; because the value is so high, no other LEDs will light up
#define ERR_GENERIC 100
// Code 1 - The PWM ISR got nested, which means its too slow to function correctly at the required refresh speed
#define ERR_NESTED_PWM 1
// Code 2 - The fader ISR got nested, which means its too slow to function correctly at the required refresh speed
#define ERR_NESTED_FADER 2
// Code 3 - Other error in the ISR
#define ERR_ISR_ERROR 3
// Code 4 - The PWM frequency is set too high resulting in an ISR interval which is lower than the known working limit
#define ERR_ISR_INTERVAL_TOO_SMALL 4

// ------------------------- EEPROM Settings ----------------------------

// Define to enable storing of settings in EEPROM - when not defined, the entire EEPROM library is excluded and all eeprom functions become stubs
#define SUPPORT_EEPROM





// ------------------------- Sanity Checks on Settings ----------------------------

// Sanity: make sure the selected settings make sense
#if FADER_UPDATE_FREQ > TIMER_FREQ
#error "FADER_UPDATE_FREQ can not be higher than TIMER_FREQ (it also makes no sense)"
#endif

#if FADER_UPDATE_FREQ < 10
//#error "FADER_UPDATE_FREQ is less than 10 Hz (this will result in visible jumps in brightness during fading)"
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

// Number of animations in total - used in the main loop and the EEPROM sanity check
#define NUM_ANIMATIONS 9

#endif