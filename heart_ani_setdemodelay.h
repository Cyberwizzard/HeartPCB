/**
 * heart_ani_setdemodelay.h - Heart PCB Project - Animation to configure if the demo mode is active and how long the delay is between switches
 * 
 * @author  Berend Dekens <berend@cyberwizzard.nl>
 * @version 1
 * @date    2018.04.21
 * @license GNUGPLv3
 */

#ifndef _HEART_ANI_SETMODEDELAY_H_
#define _HEART_ANI_SETMODEDELAY_H_

#include "heart_settings.h"
#include "Arduino.h"

/**
 * Blink some LEDs to indicate what the current setting will be, when the button is released the animation will end.
 */
void animate_setdemodelay();

#endif