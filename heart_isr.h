/**
 * heart_isr.h - Heart PCB Project - Standard Interrupt Service Routine
 * 
 * @author  Berend Dekens <berend@cyberwizzard.nl>
 * @version 1
 * @date    2018.04.13
 * @license GNUGPLv3
 */
#ifndef _HEART_ISR_H_
#define _HEART_ISR_H_

#include "heart_settings.h"

// Shared error register, when set to non-zero the ISR will show an error using the LEDs
extern volatile uint8_t  _err; // when non-zero, an error occured and the LEDs will indicate what went wrong

// Shared LED brightness tracking
// DO NOT SET THESE DIRECTLY; ALWAYS USE 'SET_LED_BRIGHTNESS' or 'SET_LED_BRIGHTNESS_MAJOR'
extern volatile duint8_t _led_brightness [NUM_LEDS];  // double uint8_t, the major byte is used for the PWM value
extern volatile uint8_t  _raw_pwm_val [NUM_LEDS];     // real PWM values
extern volatile uint8_t  _raw_scaler;

// special type controlling the faders per LED
extern fader_struct_t fader [NUM_LEDS];

// flag to enable or disable the demo mode (0 = disabled, anything higher is a duration multiplier)
extern volatile uint8_t demo_mode;

// flags to track that a button is held down
extern volatile uint8_t btn0_hold;
//extern volatile uint8_t btn1_hold; - not yet implemented

/**
 * Timer interrupt routine; provides software PWM and LED fading logic, needs to be fast in order to
 * function correctly.
 *
 * Currently this ISR is measured to take between 8 us and 24 us depending on the settings.
 */
void heart_isr();

#define _SET_SCALED_PWM(__led, __major) _raw_pwm_val[__led] = (__major) >> _raw_scaler;

#define SET_LED_BRIGHTNESS_MAJOR(__led, __major) {    \
  _led_brightness[__led].major = __major;             \
  _raw_pwm_val[__led] = __major >> _raw_scaler;       \
}

#define SET_LED_BRIGHTNESS(__led, __major, __minor) {   \
  _led_brightness[__led].major = (__major);             \
  _led_brightness[__led].minor = (__minor);             \
  _SET_SCALED_PWM(__led, __major);                      \
}

#define SET_LED_BRIGHTNESS_RAW(__led, __raw) {         \
  _led_brightness[__led].raw = (__raw);                \
  _SET_SCALED_PWM(__led, (__raw) >> 8);                \
}

#define GET_LED_BRIGHTNESS(i) _led_brightness[i]

#define SET_BRIGHTNESS_SCALE(__scaler) {                               \
  _raw_scaler = (__scaler < 5) ? ((__scaler >= 0) ? __scaler : 0) : 5; \
}

#define GET_BRIGHTNESS_SCALE _raw_scaler

#endif