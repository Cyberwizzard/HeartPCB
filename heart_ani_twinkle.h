/**
 * heart_ani_twinkle.h - Heart PCB Project - Animation with twinkling LEDs
 * 
 * @author  Berend Dekens <berend@cyberwizzard.nl>
 * @version 1
 * @date    2018.04.21
 * @license GNUGPLv3
 */

#ifndef _HEART_ANI_TWINKLE_H_
#define _HEART_ANI_TWINKLE_H_

#include "heart_settings.h"
#include "Arduino.h"

/**
 * Let the heart 'twinkle' like little stars
 * @param setup When non-zero, fade all LEDs to the lower brightness bound for this animation, usefull for an animation transition
 */
void animate_twinkle(uint8_t setup = 1);

#endif