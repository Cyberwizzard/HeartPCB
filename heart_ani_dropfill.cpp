/**
 * heart_ani_dropfill.h - Heart PCB Project - Animation where heart fills with drops
 * 
 * @author  Berend Dekens <berend@cyberwizzard.nl>
 * @version 1
 * @date    2018.04.13
 * @license GNUGPLv3
 */

#include "heart_ani_dropfill.h"
#include "heart_isr.h"
#include "heart_delay.h"

/**
 * Drip heart: the top of the heart 'fills' up until a 'drop' falls over the edge, filling up the bottom of the heart.
 * Note: hardcoded for 10 LED groups.
 *
 * @param setup When 1; the state struct is cleared and the animation starts over
 * @param delay_between_ms Number of parallel runners, valid range is 1 to 8
 * @param delay_fill_ms Whether alternating runners move in the same direction or counter-directions
 * @param delay_drop_ms Whether alternating runners should 'erasers'; these runners fade out LEDs rather than in
 */
void animate_dropfill(const int8_t setup = 1, const int16_t delay_between_ms = 3000, const int16_t delay_fill_ms = 200, const int16_t delay_drop_ms = 200) {
  const uint16_t ani_delay_ms  = 25;
  const uint16_t dur_top_fill  = 100; //300;
  const uint16_t dur_drop      = 20;
  const uint16_t dur_splash    = 100;
  const uint16_t dur_idle      = 10; //100;
  
  const uint8_t fade_upper       = 255;
  const uint8_t fade_lower       = 0;
  const uint8_t fade_speed_major = 40;
  const uint8_t fade_filled_low  = 80;
  
  const uint16_t fill_max      = 2;
  
  // Hardcoded LED indices denoting the top of the heart and the groups next to the top center
  const uint8_t LED_LAYER0       = 5;
  const uint8_t LED_LAYER1 [2]   = {4, 6};
  
  // Utility constants
  const uint16_t dur_top_fill_h = (dur_top_fill + 1) / 2; // Half of the steps to fill up the top. Note: +1 to round up
  const uint16_t dur_drop_step  = (dur_drop + 1) / 4;     // Animation steps per LED, 4 layers in total. Note: +1 to round up
  
  uint16_t cnt = 0;
  uint16_t fill [4] = {}; // Fill counter per 'layer' in the heart
  uint8_t done = 0;
  
  uint8_t top_layer0, top_layer1;

  enum { TOP_FILL, DROP, SPLASH_END, IDLE } state = IDLE;

  // Enable the delay function if it was turned off by button press to abort the previous animation
  enable_heart_delay();

  // Setup phase: mark all LEDs active and fade to the lower bound
  if(setup) {
    for(int8_t l=0; l < NUM_LEDS; l++) {
      fader[l].active = 0;
      fader[l].lower  = fade_lower;
      fader[l].upper  = fade_upper;
      fader[l].delta  = fade_speed_major << 8;
      fader[l].reload = NONE;
      // Configure the faders to fade to the target intensity regardless of current state
      setup_fade_to_lower(&fader[l], &GET_LED_BRIGHTNESS(l));
    }
  }

  while(!done) {
    switch(state) {
      case IDLE:
        // IDLE mode, just count until the IDLE is done
        cnt++;
        if(cnt >= dur_idle) {
          // IDLE done, go to top-fill
          cnt = 0;
          state = TOP_FILL;
        }
        break;
      case TOP_FILL:
       // Fill the 'top' of the heart by slowly fading in the first 2 layers (5 LEDs, 3 groups)
       cnt++;

       // First compute how much of each of the 2 layers is on
       if(cnt >= dur_top_fill_h) {
         // First layer full, cap it
         top_layer0 = 255;
         
         // Compute the remainder for the second layer
         top_layer1 = (uint16_t)((uint16_t)(fade_upper * (cnt - dur_top_fill_h)) / dur_top_fill_h);
       } else {
         // First layer not full, compute how much its on
         top_layer0 = (uint16_t)((uint16_t)(fade_upper * cnt) / dur_top_fill_h);
         top_layer1 = 0;
       }
       
       // Set the PWM brightness
       SET_LED_BRIGHTNESS_MAJOR(LED_LAYER0,    top_layer0);
       SET_LED_BRIGHTNESS_MAJOR(LED_LAYER1[0], top_layer1);
       SET_LED_BRIGHTNESS_MAJOR(LED_LAYER1[1], top_layer1);
       
       // Detect when done
       if(cnt >= dur_top_fill) {
         // TOP_FILL done, go to drop
         cnt = 0;
         state = DROP;
        }
        break;
      case DROP:
        // At the start of the drop, disable the 'top fill' LEDs
        if(cnt == 0) {
          fader[LED_LAYER0].active = 0;
          fader[LED_LAYER1[0]].active = 0;
          fader[LED_LAYER1[1]].active = 0;
          barrier();
          fader[LED_LAYER0].delta    = -fade_speed_major << 8;
          fader[LED_LAYER1[0]].delta = -fade_speed_major << 8;
          fader[LED_LAYER1[1]].delta = -fade_speed_major << 8;
          barrier();
          fader[LED_LAYER0].active    = 1;
          fader[LED_LAYER1[0]].active = 1;
          fader[LED_LAYER1[1]].active = 1;
        }
        
        cnt++;
        
        // 4 layers of dropping
        if(cnt  <= dur_drop_step * 1) {
          // First layer of dropping
          SET_LED_BRIGHTNESS_MAJOR(3, fade_upper);
          SET_LED_BRIGHTNESS_MAJOR(7, fade_upper);
          if(cnt == dur_drop_step * 1) {
            // Fill counter if bin 3 is not full yet
            if(fill[2] >= fill_max && fill[3] < fill_max) {
              // Drop came by, fill the bucket
              fill[3]++;
              // See if the bucket is full now, if so, change the lower boundary so it stays 'on'
              if(fill[3] == fill_max) {
                // Note: these faders are enabled when the drop reaches the next layer
                fader[3].lower = fade_filled_low;
                fader[7].lower = fade_filled_low;
              } 
            }
          }
        } else if(cnt  <= dur_drop_step * 2) {
          // Second layer of dropping
          SET_LED_BRIGHTNESS_MAJOR(2, fade_upper);
          SET_LED_BRIGHTNESS_MAJOR(8, fade_upper);
          if(cnt == dur_drop_step * 2) {
            // Turn off previous layer on the switch point
            fader[3].active = 0;
            fader[7].active = 0;
            barrier();
            fader[3].delta = -fade_speed_major << 8;
            fader[7].delta = -fade_speed_major << 8;
            barrier();
            fader[3].active = 1;
            fader[7].active = 1;
            
            // Fill counter if bin 2 is not full yet
            if(fill[1] >= fill_max && fill[2] < fill_max) {
              // Drop came by, fill the bucket
              fill[2]++;
              // See if the bucket is full now, if so, change the lower boundary so it stays 'on'
              if(fill[2] == fill_max) {
                // Note: these faders are enabled when the drop reaches the next layer
                fader[2].lower = fade_filled_low;
                fader[8].lower = fade_filled_low;
              } 
            }
          }
        } else if(cnt  <= dur_drop_step * 3) {
          // Third layer of dropping
          SET_LED_BRIGHTNESS_MAJOR(1, fade_upper);
          SET_LED_BRIGHTNESS_MAJOR(9, fade_upper);
          if(cnt == dur_drop_step * 3) {
            // Turn off previous layer on the switch point
            fader[2].active = 0;
            fader[8].active = 0;
            barrier();
            fader[2].delta = -fade_speed_major << 8;
            fader[8].delta = -fade_speed_major << 8;
            barrier();
            fader[2].active = 1;
            fader[8].active = 1;
            
            // Fill counter if bin 1 is not full yet
            if(fill[0] >= fill_max && fill[1] < fill_max) {
              // Drop came by, fill the bucket
              fill[1]++;
              // See if the bucket is full now, if so, change the lower boundary so it stays 'on'
              if(fill[1] == fill_max) {
                // Note: these faders are enabled when the drop reaches the next layer
                fader[1].lower = fade_filled_low;
                fader[9].lower = fade_filled_low;
              } 
            }
          }
        } else if(cnt <= dur_drop_step * 4) {
          // Last layer of dropping: heart bottom center
          SET_LED_BRIGHTNESS_MAJOR(0, fade_upper);
          if(cnt == dur_drop_step * 4) {
            // Turn off previous layer on the switch point
            fader[1].active = 0;
            fader[9].active = 0;
            barrier();
            fader[1].delta = -fade_speed_major << 8;
            fader[9].delta = -fade_speed_major << 8;
            barrier();
            fader[1].active = 1;
            fader[9].active = 1;
            
            // Fill counter if bin 0 is not full yet
            if(fill[0] < fill_max) {
              // Drop came by, fill the bucket
              fill[0]++;
              // See if the bucket is full now, if so, change the lower boundary so it stays 'on'
              if(fill[0] == fill_max) {
                // Note: this fader is enabled when entering splash
                fader[0].lower = fade_filled_low;
              } 
            }
          }
        }

        // Detect when done
        if(cnt >= dur_top_fill) {
          // DROP done, go to SPLASH
          cnt = 0;
          state = SPLASH_END;
          
          // Turn on the fader for the last LED
          fader[0].active = 0;
          barrier();
          fader[0].delta = -fade_speed_major << 8;
          barrier();
          fader[0].active = 1;
        }
        break;
      case SPLASH_END:
        // Detect if all bins are full
        if(fill[3] >= fill_max) {
          cnt++;
          
          if(cnt == dur_splash / 2) {
            // Splash ending complete, fade whole heart in
            for(int8_t l=0; l < NUM_LEDS; l++) {
              fader[l].active = 0;
              barrier();
              fader[l].lower  = fade_lower;
              fader[l].upper  = fade_upper;
              fader[l].delta  = fade_speed_major << 8;
              fader[l].reload = NONE;
              barrier();
              fader[l].active = 1;
            }
          } else if(cnt >= dur_splash) {
            // Fade out and go to IDLE
            for(int8_t l=0; l < NUM_LEDS; l++) {
              fader[l].active = 0;
              barrier();
              fader[l].lower  = fade_lower;
              fader[l].upper  = fade_upper;
              fader[l].delta  = -fade_speed_major << 8;
              fader[l].reload = NONE;
              barrier();
              fader[l].active = 1;
            }
            
            // After fading out the heart, clear the bins
            fill[0] = 0;
            fill[1] = 0;
            fill[2] = 0;
            fill[3] = 0;
            
            state = IDLE;
          }
        } else
          state = IDLE;
        break;
    }
    
    // Delay between steps of the animation
    if(heart_delay(ani_delay_ms))
      return; // When 1, the delay is aborted and this animation will end
  }
}
