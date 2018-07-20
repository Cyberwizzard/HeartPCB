#include "heart_settings.h"
#include "heart_isr.h"
#include "heart_profiling.h"
#include "heart_eeprom.h"
#include "heart_ani_run_around.h"
#include "heart_ani_dropfill.h"
#include "heart_ani_twinkle.h"
#include "heart_ani_beat.h"
#include "heart_ani_setdemodelay.h"
#include "TimerOne.h"

int start_animation = 0;

void setup() {
  // configure relevant pins as outputs
  for(uint8_t l=0; l<NUM_LEDS; l++) pinMode(PIN_LED_START+l, OUTPUT);
  pinMode(PIN_BTN0, INPUT);
  pinMode(PIN_BTN1, INPUT);

  // Initialize measurement support for profiling (when enabled) - also inits Serial
  MEASUREMENT_INIT;
  // Some debug stats; only visible when measurements are enabled
  SERPRINT(TIMER_INTERVAL_US);
  SERPRINT(" us ISR interval (");
  SERPRINT((TIMER_INTERVAL_US * 10000) / 625);
  SERPRINTLN(" cycles)");
  SERPRINT(FADER_UPDATE_TICKS);
  SERPRINTLN(" ticks between faders");
  SERPRINT("DEmo ticks: ");
  SERPRINTLN(TIMER_DEMO_CNT_MAX);
  
  // Load settings from EEPROM (if its disabled, defaults are loaded)
  eeprom_load();
  // Apply settings
  demo_mode = eeprom_settings.demo_mode;
  start_animation = eeprom_settings.animation_id;
  SET_BRIGHTNESS_SCALE(eeprom_settings.brightness);
  
  delay(5000); // Delay slightly in case profiling is enabled so that the serial buffer can flush (otherwise the ISR will not be fast enough and we end up in an error on boot)
  Serial.flush();

  //SET_LED_BRIGHTNESS_MAJOR(1, 120);
  //SET_LED_BRIGHTNESS_MAJOR(2, 40);
  //SET_LED_BRIGHTNESS_MAJOR(3, 5);
  //SET_LED_BRIGHTNESS_MAJOR(4, 180);
  //SET_LED_BRIGHTNESS_MAJOR(5, 255);

  //fader[6].delta = 256 * 5;
  //fader[6].active = 1;
  //fader[6].reload = JUMP;
  //fader[6].upper  = 255;
  //fader[6].lower  = 0;

  //fader[7].delta = 20;
  //fader[7].active = 1;
  //fader[7].reload = INVERT;
  //fader[7].upper  = 255;
  //fader[7].lower  = 0;
  SERPRINTLN("OK:0");
  
  // Use timer1 for the intervals for the software PWM and faders
  if(ISR_MINIMUM_INTERVAL_US > TIMER_INTERVAL_US) {
    // Note: the minimum interval is not verifiable at pre-compilation so it has to be at run-time
    // Ensure that when the PWM refresh is set to high that an error condition is shown
    Timer1.initialize(100000); // Initialize to a relatively slow interval of 100ms since this is an error state anyway
    _err = ERR_ISR_INTERVAL_TOO_SMALL;
  } else {
    Timer1.initialize(TIMER_INTERVAL_US); // initialize timer1 and set to a high pace interval
  }

  // Set the ISR for Timer 1
  Timer1.attachInterrupt(heart_isr);

  // Second debug print; when the ISR is set way too high, the serial port dies - this canary will show this issue
//  SERPRINTLN("OK:1");
}

  run_around_setting_struct_t run_around_erasing = {
    .fade_speed_major = 0,
    .fade_lower       = 0,
    .fade_upper       = 255,
    .fade_up_start    = 255,
    .offset           = 0,
    .delay_base_ms    = 220,
    .delay_tgt_ms     = 50,
    .delay_step_ms    = 10
  };

  run_around_setting_struct_t run_around_fade2 = {
    .fade_speed_major = 5,
    .fade_lower       = 0,
    .fade_upper       = 255,
    .fade_up_start    = 235,
    .offset           = 0,
    .delay_base_ms    = 220,
    .delay_tgt_ms     = 50,
    .delay_step_ms    = 10
  };

int first_run = 1; // Flag to skip storing settings in the first run

void loop() {
  // Set the ISR for Timer 1
  //Timer1.attachInterrupt(heart_isr);

  // Second debug print; when the ISR is set way too high, the serial port dies - this canary will show this issue
  SERPRINTLN("OK:1");
  
  while(1) {

    for(int i=0,j=start_animation; i<NUM_ANIMATIONS; i++, j=(i+start_animation)%NUM_ANIMATIONS) {
      MEASUREMENT_PRINT;
      
      if(!first_run) {
        // Save animation switch plus all other settings to EEPROM to resume when power is lost
        eeprom_settings.animation_id = j;
        eeprom_settings.demo_mode = demo_mode;
        eeprom_settings.brightness = GET_BRIGHTNESS_SCALE;
        eeprom_store();
      } else {
        first_run = 0;
      }
      switch(j) {
        case 0:
          // Show a beating heart
          animate_beat();
          break;
        case 1:
          // Animation of 1 runner going around the heart
          animate_run_around();
          break;
        case 2:
          // Setup, loop, 1 runner, reverse direction
          animate_run_around(1, 1, -1); 
          break;
        case 3:
          // Setup, loop, 2 runners, standard direction
          animate_run_around(1, 1, 1, 2);
          break;
        case 4:
          // 3 runners
          animate_run_around(1, 1, 1, 3);
          break;
        case 5:
          // 2 runners, one cross
          animate_run_around(1, 1, 1, 2, 1); // 2 runners, one cross
          break;
        case 6:
          // 2 runners, one erasers
          animate_run_around(1, 1, 1, 4, 0, 1, &run_around_erasing); // 2 runners, one erasers
          break;
        case 7:
          // Slowly filling heart
          animate_dropfill();
          break;
        case 8:
          // Starry twinkle
          animate_twinkle();
          break;
      } 
      // If tbtn0 was held down, show the delay configuration panel
      if(btn0_hold) {
        animate_setdemodelay();
        i--; // Return to the current animation
      }
      
    }
  }
  
  
  

  //for(int i=0; i<10; i++) animate_run_around(i == 0, 1, 3); // 3 runners
  //for(int i=0; i<10; i++) animate_run_around(i == 0, 1, 4); // 4 runners
  //for(int i=0; i<10; i++) animate_run_around(i == 0, 1, 5); // 5 runners

  //for(int i=0; i<10; i++) animate_run_around(i == 0, 1, 2, 1); // 2 runners, one cross
  //for(int i=0; i<10; i++) animate_run_around(i == 0, 1, 4, 1); // 4 runners, two cross



  //for(int i=0; i<10; i++) animate_run_around(i == 0, 1, 4, 0, 1, &run_around_erasing); // 2 runners, one erasers
  
  /*
  for(int i=0; i<10; i++) animate_run_around(i == 0, 1, 2, 0, 0, &run_around_fade2); // 2 runners
  if(run_around_fade2.delay_tgt_ms == run_around_fade2.delay_current_ms) {
    uint16_t swap = run_around_fade2.delay_tgt_ms;
    run_around_fade2.delay_tgt_ms = run_around_fade2.delay_base_ms;
    run_around_fade2.delay_base_ms = swap;
  } */
}



