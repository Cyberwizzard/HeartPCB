/**
 * heart_ani_beat.h - Heart PCB Project - Animation with a beating heart
 * 
 * @author  Berend Dekens <berend@cyberwizzard.nl>
 * @version 1
 * @date    2018.04.21
 * @license GNUGPLv3
 */

#ifndef _HEART_ANI_BEAT_H_
#define _HEART_ANI_BEAT_H_

#include "heart_settings.h"
#include "Arduino.h"

/**
 * Let the heart 'beat'
 * @param setup When non-zero, fade all LEDs to the lower brightness bound for this animation, useful for an animation transition
 */
void animate_beat(uint8_t num_beats = 2, uint16_t beat_interval_ms = 400, uint16_t beat_gap_ms = 1200);

#endif