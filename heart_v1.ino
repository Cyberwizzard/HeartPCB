#include "heart_settings.h"
#include "heart_isr.h"
#include "heart_profiling.h"
#include "heart_ani_run_around.h"
#include "TimerOne.h"

void setup() {
  // configure relevant pins as outputs
  for(uint8_t l=0; l<NUM_LEDS; l++) {
    pinMode(PIN_LED_START+l, OUTPUT);
  }

  // Initialize measurement support for profiling (when enabled) - also inits Serial
  MEASUREMENT_INIT;
  // Some debug stats; only visible when measurements are enabled
  SERPRINT(TIMER_INTERVAL_US);
  SERPRINTLN(" us PWM interval");
  SERPRINT(FADER_UPDATE_TICKS);
  SERPRINTLN(" ticks between faders");

  led_pwm_val[1].major = 120;
  led_pwm_val[2].major = 40;
  led_pwm_val[3].major = 5;
  led_pwm_val[4].major = 180;
  led_pwm_val[5].major = 255;
  //led_pwm_val[6].major = 120;
  fader[6].delta = 256 * 5;
  fader[6].active = 1;
  fader[6].reload = JUMP;
  fader[6].upper  = 255;
  fader[6].lower  = 0;

  fader[7].delta = 20;
  fader[7].active = 1;
  fader[7].reload = INVERT;
  fader[7].upper  = 255;
  fader[7].lower  = 0;
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
  SERPRINTLN("OK:1");
}

  const run_around_setting_struct_t run_around_erasing = {
    .fade_speed_major = 0,
    .fade_lower       = 0,
    .fade_upper       = 255,
    .fade_up_start    = 255,
    .offset           = 0,
    .delay_base_ms    = 300,
    .delay_tgt_ms     = 50,
    .delay_step_ms    = 10
  };

void loop() {
  MEASUREMENT_PRINT;
  // put your main code here, to run repeatedly:
  //for(int i=0; i<10; i++) animate_run_around(i == 0);
  
  //for(int i=0; i<10; i++) animate_run_around(i == 0, -1); // 1 runner, reverse direction

  //for(int i=0; i<10; i++) animate_run_around(i == 0, 1, 2); // 2 runners

  //for(int i=0; i<10; i++) animate_run_around(i == 0, 1, 3); // 3 runners
  //for(int i=0; i<10; i++) animate_run_around(i == 0, 1, 4); // 4 runners
  //for(int i=0; i<10; i++) animate_run_around(i == 0, 1, 5); // 5 runners

  //for(int i=0; i<10; i++) animate_run_around(i == 0, 1, 2, 1); // 2 runners, one cross
  //for(int i=0; i<10; i++) animate_run_around(i == 0, 1, 4, 1); // 4 runners, two cross



  for(int i=0; i<10; i++) animate_run_around(i == 0, 1, 4, 0, 1, &run_around_erasing); // 2 runners, one erasers
}



