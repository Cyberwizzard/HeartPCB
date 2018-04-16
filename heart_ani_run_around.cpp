/**
 * heart_ani_run_around.h - Heart PCB Project - Animation with 'runners' running around the heart, turning on (or off) LEDs
 * 
 * @author  Berend Dekens <berend@cyberwizzard.nl>
 * @version 1
 * @date    2018.04.13
 * @license GNUGPLv3
 */

#include "heart_ani_run_around.h"
#include "heart_isr.h"

const static run_around_setting_struct_t run_around_default = {
  .fade_speed_major = 25,
  .fade_lower = 20,
  .fade_upper = 255,
  .fade_up_start = 200,
  .offset = 0,
  .delay_base_ms = 150,
  .delay_tgt_ms = 150,
  .delay_step_ms = 0,
  .delay_current_ms = 0
};

/**
 * Running animation: one or more 'runners' run around the heart turning on LEDs as they go.
 * @param dir Direction for the animation, valid values are -1 and 1
 * @param runners Number of parallel runners, valid range is 1 to 8
 * @param cross Whether alternating runners move in the same direction or counter-directions
 * @param erasers Whether alternating runners should 'erasers'; these runners fade out LEDs rather than in
 * @param s Configure all LEDs with brightness bounds and fade to them at the start of the animation, only needed when switching animation types
 */
void animate_run_around(const int8_t setup = 1, const int8_t dir = 1, const int8_t runners = 1, const int8_t cross = 0, const int8_t erasers = 0, run_around_setting_struct_t *s = NULL) {
  // When no settings are given, use the default ones
  if(s == NULL) s = &run_around_default;

  // Copy settings from struct
  const uint8_t  fade_speed_major = s->fade_speed_major;     // Speed of the fade up and fade down
  const uint8_t  fade_lower = s->fade_lower;                 // Lower boundary for dimmed LEDs
  const uint8_t  fade_upper = s->fade_upper;                 // Upper boundary for bright LEDs
  const uint8_t  fade_up_start = s->fade_up_start;           // Runners set this goal for their LED, causing a soft-start
  const uint16_t delay_base_ms = s->delay_base_ms;           // Base delay between steps in the animation
  const uint16_t delay_tgt_ms = s->delay_tgt_ms;             // Target delay between steps in the animation
  const int16_t  delay_delta = delay_base_ms < delay_tgt_ms ? s->delay_step_ms : -s->delay_step_ms;
  
  // When running the setup, configure the current delay in the settings struct
  if(setup)
    s->delay_current_ms = delay_base_ms;

  if(runners < 1 || runners > 8) { _err = ERR_GENERIC; return; }
  // Make an array to hold the indexes of the 'runners'
  uint8_t li [8];
  // Distribute runners around the heart to begin
  for(int8_t i=0; i<runners; i++) {
    li[i] = (((NUM_LEDS * i) / runners) + s->offset) % NUM_LEDS;
  }

  // Setup phase: mark all LEDs active and fade to the lower bound
  if(setup) {
    for(int8_t l=0; l < NUM_LEDS; l++) {
      fader[l].active = 0;
      fader[l].lower  = fade_lower;
      fader[l].upper  = fade_upper;
      fader[l].delta  = fade_speed_major << 8;
      // Configure the faders to fade to the target intensity regardless of current state
      setup_fade_to_lower(&fader[l], &led_pwm_val[l]);
    }
  }

  // Do one full animation; by calling this function repeatedly a fluid animation is obtained
  for(int8_t cnt=0; cnt < NUM_LEDS; cnt++) {
    // Loop over all runners
    for(int8_t r=0; r < runners; r++) {
      int8_t odd = r & 0x1;
      uint8_t led = li[r]; // Get the LED index for the current runner

      // Apply the current status before moving the runner
      if(erasers && odd) {
        // Eraser runner, fade the current LED out (if it wasn't off before)
        fader[led].active = 0;
        barrier(); // Barrier after disabling the fader; makes sure the fader is inactive while settings change
        led_pwm_val[led].major = fade_lower;
        led_pwm_val[led].minor = 0;
      } else {
        // Normal runner, fade current LED in
        fader[led].active = 0;
        barrier(); // Barrier after disabling the fader; makes sure the fader is inactive while settings change
        if(fade_up_start == fade_upper) {
          // Hard-start; fade out from the start
          fader[led].reload = NONE;
        } else {
          // Soft-start: fade in a bit before fading out again, for the twinkly feeling
          fader[led].reload = UPPER_INVERT;
        }
        // Compute the 16-bit delta
        fader[led].delta = fade_speed_major << 8;
        // Set the LED brightness
        led_pwm_val[led].major = fade_up_start;
        led_pwm_val[led].minor = 0;
        barrier(); // Barrier after configuring the fader; makes sure the fader settings are committed before animating it again
        // Mark the fader active again
        fader[led].active = 1;
      }

      // Move the current runner
      if(cross && odd) {
        // Crossing runner mode: every other runner runs in the 'wrong' direction
        li[r] = (li[r] - dir) % NUM_LEDS;
      } else {
        // Normal runner or an even numbered runner in cross mode: do a step in the standard direction
        li[r] = (li[r] + dir) % NUM_LEDS;
      }
      // Fix negative wrap-around:
      if(li[r] == 255) li[r] = NUM_LEDS - 1;
    }

    // Animation step complete, do delay
    delay(s->delay_current_ms);
    
    // Update delay amount if requested
    if(delay_delta != 0 && s->delay_current_ms != delay_tgt_ms) {
      //Serial.print("delay: "); Serial.println(s->delay_current_ms);
      // Compute the absolute gap between the current delay and the target delay
      uint16_t gap = (s->delay_current_ms < delay_tgt_ms) ? (delay_tgt_ms - s->delay_current_ms) : (s->delay_current_ms - delay_tgt_ms);
      if(gap > s->delay_step_ms) {
        // The gap between the current delay and the target delay is bigger than the step size
        s->delay_current_ms += delay_delta;
      } else {
        // The gap between the current delay and the target is smaller than the step size, cap it
        s->delay_current_ms = delay_tgt_ms;
      }
      //Serial.print("delay: "); Serial.println(s->delay_current_ms);
    }
  }
}