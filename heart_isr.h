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
extern volatile duint8_t led_pwm_val [NUM_LEDS]; // double uint8_t, the major byte is used for the PWM value

// special type controlling the faders per LED
extern fader_struct_t fader [NUM_LEDS];

/**
 * Timer interrupt routine; provides software PWM and LED fading logic, needs to be fast in order to
 * function correctly.
 *
 * Currently this ISR is measured to take between 8 us and 24 us depending on the settings.
 */
void heart_isr();

#endif