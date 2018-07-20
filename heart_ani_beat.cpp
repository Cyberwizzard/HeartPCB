/**
 * heart_ani_beat.cpp - Heart PCB Project - Animation with beating heart
 * 
 * @author  Berend Dekens <berend@cyberwizzard.nl>
 * @version 1
 * @date    2018.07.19
 * @license GNUGPLv3
 */

#include "heart_ani_beat.h"
#include "heart_isr.h"
#include "heart_delay.h"

/**
 * Let the heart 'beat'
 * @param setup When non-zero, fade all LEDs to the lower brightness bound for this animation, useful for an animation transition
 */
void animate_beat(uint8_t num_beats = 2, uint16_t beat_interval_ms = 400, uint16_t beat_gap_ms = 1200) {
  const int16_t fade_delta = 5 << 8;
  const uint8_t fade_upper = 255;
  const uint8_t fade_lower = 2;
  // Timing for fade in
  const int16_t fadein_delta = 20 << 8;
  const int16_t delay_fadein_ms = 150;
  // Timing for fadeout
  const int16_t fadeout_delta = 5 << 8;

  uint8_t beat_cnt = 0;
  uint16_t delay_ms = 10;
  enum { BEAT_FADEIN, BEAT_FADEOUT, BEAT_WAIT } state;
  
  // Sanity checking
  if(beat_interval_ms < 200) beat_interval_ms = 200;

  // Start with a fade in
  state = BEAT_FADEIN;

  // Enable the delay function if it was turned off by button press to abort the previous animation
  enable_heart_delay();

  // Setup phase: mark all LEDs active and fade to the lower bound
  for(int8_t l=0; l < NUM_LEDS; l++) {
    fader[l].active = 0;
    fader[l].lower  = fade_lower;
    fader[l].upper  = fade_upper;
    fader[l].delta  = fade_delta;
    fader[l].reload = NONE;
    // Configure the faders to fade to the target intensity regardless of current state
    //setup_fade_to_lower(&fader[l], &GET_LED_BRIGHTNESS(l));
  }

  while(1) {
  //for(uint8_t ii = 0; ii < 5; ii++) {
    if(state == BEAT_FADEIN) {
      for(int i=0; i<NUM_LEDS; i++) {
        fader[i].delta = fadein_delta;
      }
      delay_ms = delay_fadein_ms;
      
      // Next state
      state = BEAT_FADEOUT;
    } else if(state == BEAT_FADEOUT) {
      for(int i=0; i<NUM_LEDS; i++) {
        fader[i].delta = -fadeout_delta;
      }
      delay_ms = beat_interval_ms - delay_fadein_ms;
      
      // Next state
      if(++beat_cnt < num_beats)
        state = BEAT_FADEIN;
      else
        state = BEAT_WAIT;
    } else {
      // Wait for the next beat
      delay_ms = beat_gap_ms;
      beat_cnt = 0;
      state = BEAT_FADEIN;
    }

    barrier();
    // Activate all animations
    for(int i=0; i<NUM_LEDS; i++) {
      fader[i].active = 1;
    }

    if(heart_delay(delay_ms))
      return; // Abort animation when requested
  }


}
