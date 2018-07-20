/**
 * heart_ani_setdemodelay.cpp - Heart PCB Project - Animation to configure if the demo mode is active and how long the delay is between switches
 * 
 * @author  Berend Dekens <berend@cyberwizzard.nl>
 * @version 1
 * @date    2018.07.19
 * @license GNUGPLv3
 */

#include "heart_ani_setdemodelay.h"
#include "heart_isr.h"
#include "heart_delay.h"

void inline configure_LEDs(int8_t level, uint8_t off = 0) {
  // Setup phase: mark all LEDs active and fade to the lower bound
  for(int8_t l=0; l < NUM_LEDS; l++) {
    fader[l].active = 0;
    // Drive LED intensity directly to full off or on
    if(!off && l < level) {
      SET_LED_BRIGHTNESS(l, 255, 0);
    } else {
      SET_LED_BRIGHTNESS(l, 0, 0);
    }
  }
  // Turn on top LED
  SET_LED_BRIGHTNESS(5, 255, 0);
}

/**
 * Blink some LEDs to indicate what the current setting will be, when the button is released the animation will end.
 */
void animate_setdemodelay() {
  const uint8_t polling_interval_ms = 50; // Delay per loop iteration, checks if the button is still held down
  const uint8_t cnt_max = 50;             // After this many iterations, step to the next setting
  uint8_t cnt = 0;                        // Counter to create a delay before switching to the next multiplier
  uint8_t num_demo_multi = demo_mode;     // Start from whatever the demo mode multiplier currently is
  const uint8_t blink_max = 5;            // Blink counter maximum, when this is reached, the LEDs turn on or off
  uint8_t blink_cnt = 0;
  uint8_t off = 0;

  // Disable demo mode to make sure this configuration 'screen' is not interrupted
  demo_mode = 0;
  barrier();
  
  // Sanity
  if(num_demo_multi > 5) num_demo_multi = 5;
  
  // Enable the delay function if it was turned off by button press to abort the previous animation
  enable_heart_delay();
  
  // Show whatever level is active
  configure_LEDs(num_demo_multi);

  while(btn0_hold) {
   
    cnt++;
    blink_cnt++;
    
    // See if we counted long enough to switch to the next level
    if(cnt >= cnt_max) {
      num_demo_multi = ( num_demo_multi + 1 ) % 6;
      cnt = 0;
    }
    
    if(blink_cnt == blink_max) {
      off = !off;
      configure_LEDs(num_demo_multi, off);
      blink_cnt = 0;
    }
    
    if(heart_delay(polling_interval_ms)){ _err = ERR_GENERIC; }
      //return; // Abort animation when requested - THIS SHOULD NEVER HAPPEN
  }

  // Show what was selected
  configure_LEDs(num_demo_multi, 0);
  heart_delay(2000);

  // Apply setting
  demo_mode = num_demo_multi;
  Serial.println(demo_mode);
}
