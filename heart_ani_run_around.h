/**
 * heart_ani_run_around.h - Heart PCB Project - Animation with 'runners' running around the heart, turning on (or off) LEDs
 * 
 * @author  Berend Dekens <berend@cyberwizzard.nl>
 * @version 1
 * @date    2018.04.13
 * @license GNUGPLv3
 */
#ifndef _HEART_RUN_AROUND_H_
#define _HEART_RUN_AROUND_H_

#include "heart_settings.h"
#include "Arduino.h"

typedef struct {
  const uint8_t  fade_speed_major;    // Speed of the fade up and fade down
  const uint8_t  fade_lower;          // Lower boundary for dimmed LEDs
  const uint8_t  fade_upper;          // Upper boundary for bright LEDs
  const uint8_t  fade_up_start;       // Runners set this goal for their LED, causing a soft-start, set to fade_upper to do a hard start
  const uint8_t  offset;              // LED offset for the start position of the runners, should be 0 by default, values larger than NUM_LEDS - 1 make no sense
  const uint16_t delay_base_ms;       // Delay between steps in the animation, start value
  const uint16_t delay_tgt_ms;        // Delay between steps in the animation, target value
  const uint16_t delay_step_ms;       // Step size to alter the delay between animation steps (always positive)
        uint16_t delay_current_ms;    // Mutable current value of the delay in ms; set on the first run when setup == 1, use this to 
                                      // track what the animation step delay was when the animation function returns
} run_around_setting_struct_t;

/**
 * Running animation: one or more 'runners' run around the heart turning on LEDs as they go.
 * @param dir Direction for the animation, valid values are -1 and 1
 * @param runners Number of parallel runners, valid range is 1 to 8
 * @param cross Whether alternating runners move in the same direction or counter-directions
 * @param erasers Whether alternating runners should 'erasers'; these runners fade out LEDs rather than in
 * @param s Configure all LEDs with brightness bounds and fade to them at the start of the animation, only needed when switching animation types
 */
void animate_run_around(const int8_t setup = 1, const int8_t dir = 1, const int8_t runners = 1, const int8_t cross = 0, const int8_t erasers = 0, run_around_setting_struct_t *s = NULL);

#endif