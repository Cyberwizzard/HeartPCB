/**
 * heart_ani_dropfill.h - Heart PCB Project - Animation where heart fills with drops
 * 
 * @author  Berend Dekens <berend@cyberwizzard.nl>
 * @version 1
 * @date    2018.04.13
 * @license GNUGPLv3
 */
#ifndef _HEART_ANI_DROPFILL_H_
#define _HEART_ANI_DROPFILL_H_

#include "heart_settings.h"
#include "Arduino.h"

/**
 * Drip heart: the top of the heart 'fills' up until a 'drop' falls over the edge, filling up the bottom of the heart.
 * @param setup When 1; the state struct is cleared and the animation starts over
 * @param delay_between_ms Number of parallel runners, valid range is 1 to 8
 * @param delay_fill_ms Whether alternating runners move in the same direction or counter-directions
 * @param delay_drop_ms Whether alternating runners should 'erasers'; these runners fade out LEDs rather than in
 */
void animate_fill(const int8_t setup = 1, const int16_t delay_between_ms = 3000, const int16_t delay_fill_ms = 200, const int16_t delay_drop_ms = 200);

#endif