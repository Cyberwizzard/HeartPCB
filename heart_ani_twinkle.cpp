/**
 * heart_ani_twinkle.h - Heart PCB Project - Animation with twinkling LEDs
 * 
 * @author  Berend Dekens <berend@cyberwizzard.nl>
 * @version 1
 * @date    2018.04.21
 * @license GNUGPLv3
 */

#include "heart_ani_twinkle.h"
#include "heart_isr.h"
#include "heart_delay.h"

/**
 * Let the heart 'twinkle' like little stars
 * @param setup When non-zero, fade all LEDs to the lower brightness bound for this animation, usefull for an animation transition
 */
void animate_twinkle(uint8_t setup = 1) {
  const int16_t fade_delta = 5 << 8;
  const uint8_t fade_upper = 255;
  const uint8_t fade_twinkle_lower = 96;
  const uint8_t fade_lower = 5;
  const uint16_t delay_ms = 200;

  // Enable the delay function if it was turned off by button press to abort the previous animation
  enable_heart_delay();

  // Setup phase: mark all LEDs active and fade to the lower bound
  if(setup) {
    for(int8_t l=0; l < NUM_LEDS; l++) {
      fader[l].active = 0;
      fader[l].lower  = fade_lower;
      fader[l].upper  = fade_upper;
      fader[l].delta  = fade_delta;
      // Configure the faders to fade to the target intensity regardless of current state
      setup_fade_to_lower(&fader[l], &GET_LED_BRIGHTNESS(l));
    }
  }

  while(1) {
    for(int i=0; i<NUM_LEDS; i++) {
      uint16_t turnOn = random(0,10);
      // Only turn a LED on with a 10% chance
      turnOn = (turnOn == 0);

      if(turnOn && fader[i].active == 0) {
        // idle LED, fade it in
        fader[i].upper  = random(fade_twinkle_lower, fade_upper);
        fader[i].delta  = fade_delta;
        fader[i].reload = UPPER_INVERT;
        barrier();
        fader[i].active = 1;
      }
    }

    if(heart_delay(delay_ms))
      return; // Abort animation when requested
  }


}
