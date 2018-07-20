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
 * Note: hardcoded for 10 LED groups.
 *
 * @param setup When 1; the state struct is cleared and the animation starts over
 */
void animate_dropfill(const int8_t setup = 1);

#endif