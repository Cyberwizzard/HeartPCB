#include "heart_settings.h"
#include "TimerOne.h"

#define ERROR_BLINK_CNT 4000
#define ERR_GENERIC 100
#define ERR_NESTED_PWM 1
#define ERR_NESTED_FADER 2
#define ERR_ISR_ERROR 2
#define ERR_ISR_INTERVAL_TOO_SMALL 3

volatile duint8_t led_pwm_val [NUM_LEDS]; // double uint8_t, the major byte is used for the PWM value
volatile uint8_t  _err = 0;               // when non-zero, an error occured and the LEDs will indicate what went wrong
volatile int16_t  _err_cnt = 0;           // during error, blink the single LEDs
volatile uint8_t  _isr_running = 0;       // flag to track when the software PWM is not meeting the interrupt interval (because it will result in an infinite recursive interrupt loop)
volatile uint8_t  _isr_fader = 0;         // flag to track when the LED fading logic is not meeting the interrupt interval (because it will result in an infinite recursive interrupt loop)
uint8_t           _pwm_step = 0;          // PWM step counter for all LEDs

volatile uint16_t fader_interval_cnt = 0; // Faders are updated every ANI_INTERVAL steps of the PWM interrupt
volatile int8_t   fader_update_ptr = -1;  // To speed up the ISR, when the update interval is reached, this pointer is set to the highest LED fader index; as long as its not 0, a single fader at a time is updated during the ISR

// special type controlling the faders per LED
fader_struct_t fader [NUM_LEDS];

#define SUPPORT_MEASUREMENTS

#ifdef SUPPORT_MEASUREMENTS
#define SERPRINT(x)   Serial.print(x)
#define SERPRINTLN(x) Serial.println(x)
#else
#define SERPRINT(x)
#define SERPRINTLN(x)
#endif

#ifdef SUPPORT_MEASUREMENTS
#define NUM_MEASUREMENTS 100
#define MEASUREMENT_INIT  { Serial.begin(9600); SERPRINTLN("Profiling active"); delay(100); }
#define MEASUREMENT_START { if(measure_start < NUM_MEASUREMENTS) { starts[measure_start] = micros(); measure_start++; }}
#define MEASUREMENT_STOP  { if(measure_stop < NUM_MEASUREMENTS) { stops[measure_stop] = micros(); measure_stop++; }}
#define MEASUREMENT_PRINT { if(measure_stop >= NUM_MEASUREMENTS && measure_stop != 127) { \
  unsigned long mindur, maxdur;                                                           \
  measure_stop = 127;                                                                     \
  SERPRINTLN("Profiling (duration, interval):");                                          \
  for(int __i = 0; __i < NUM_MEASUREMENTS; __i++) {                                       \
    unsigned long dur = stops[__i] - starts[__i];                                         \
    if(__i == 0) { mindur = dur; maxdur = dur; }                                          \
    else if(dur > maxdur) { maxdur = dur; }                                               \
    else if(dur < mindur) { mindur = dur; }                                               \
    SERPRINT(dur); SERPRINT(" us, ");                                                     \
    if(__i > 0) {                                                                         \
      unsigned long interval = starts[__i] - starts[__i - 1];                             \
      SERPRINT(interval); SERPRINTLN(" us");                                              \
    } else SERPRINTLN(" n/a");                                                            \
  }                                                                                       \
  SERPRINT("Dur: ["); SERPRINT(mindur);                                                   \
  SERPRINT(" us, "); SERPRINT(maxdur); SERPRINTLN(" us]");                                \
}}
#else
#define NUM_MEASUREMENTS 1
#define MEASUREMENT_INIT  { }
#define MEASUREMENT_START { }
#define MEASUREMENT_STOP  { }
#define MEASUREMENT_PRINT { }
#endif

#define SUPPORT_ISR_MEASUREMENTS


#ifndef SUPPORT_MEASUREMENTS
#  ifdef SUPPORT_ISR_MEASUREMENTS
#    undef SUPPORT_ISR_MEASUREMENTS
#  endif
#endif

#ifdef SUPPORT_ISR_MEASUREMENTS
#if 0
// Defines to measure the PWM part of the ISR only
#define MEASUREMENT_ISR_PWM_START MEASUREMENT_START
#define MEASUREMENT_ISR_PWM_STOP  MEASUREMENT_STOP
#define MEASUREMENT_ISR_ALL_START {}
#define MEASUREMENT_ISR_ALL_STOP  {}
#else
// Defines to measure the PWM + fader of the ISR
#define MEASUREMENT_ISR_PWM_START {}
#define MEASUREMENT_ISR_PWM_STOP  {}
#define MEASUREMENT_ISR_ALL_START MEASUREMENT_START
#define MEASUREMENT_ISR_ALL_STOP  MEASUREMENT_STOP
#endif
#else
// No measurement support; empty macros for all measurement modes
#define MEASUREMENT_ISR_PWM_START {}
#define MEASUREMENT_ISR_PWM_STOP  {}
#define MEASUREMENT_ISR_ALL_START {}
#define MEASUREMENT_ISR_ALL_STOP  {}
#endif

// Profiling code; records the start and stop times of the ISR
volatile unsigned long starts [NUM_MEASUREMENTS];
volatile unsigned long stops  [NUM_MEASUREMENTS];
volatile uint8_t       measure_start, measure_stop;

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
  Timer1.attachInterrupt(callback);

  // Second debug print; when the ISR is set way too high, the serial port dies - this canary will show this issue
  SERPRINTLN("OK:1");
}

typedef struct {
  const uint8_t fade_speed_major;    // Speed of the fade up and fade down
  const uint8_t fade_lower;          // Lower boundary for dimmed LEDs
  const uint8_t fade_upper;          // Upper boundary for bright LEDs
  const uint8_t fade_up_start;       // Runners set this goal for their LED, causing a soft-start, set to fade_upper to do a hard start
  const uint16_t base_delay_ms;      // Delay between steps in the animation
} run_around_setting_struct_t;

const static run_around_setting_struct_t run_around_default = {
  .fade_speed_major = 25,
  .fade_lower = 20,
  .fade_upper = 255,
  .fade_up_start = 200,
  .base_delay_ms = 150
};

/**
 * Running animation: one or more 'runners' run around the heart turning on LEDs as they go.
 * @param dir Direction for the animation, valid values are -1 and 1
 * @param runners Number of parallel runners, valid range is 1 to 8
 * @param cross Whether alternating runners move in the same direction or counter-directions
 * @param erasers Whether alternating runners should 'erasers'; these runners fade out LEDs rather than in
 * @param setup Configure all LEDs with brightness bounds and fade to them at the start of the animation, only needed when switching animation types
 */
void animate_run_around(const int8_t setup = 1, const int8_t dir = 1, const int8_t runners = 1, const int8_t cross = 0, const int8_t erasers = 0, run_around_setting_struct_t *s = NULL) {
  // When no settings are given, use the default ones
  if(s == NULL) s = &run_around_default;

  // Copy settings from struct
  const uint8_t  fade_speed_major = s->fade_speed_major;     // Speed of the fade up and fade down
  const uint8_t  fade_lower = s->fade_lower;                 // Lower boundary for dimmed LEDs
  const uint8_t  fade_upper = s->fade_upper;                 // Upper boundary for bright LEDs
  const uint8_t  fade_up_start = s->fade_up_start;           // Runners set this goal for their LED, causing a soft-start
  const uint16_t base_delay_ms = s->base_delay_ms;           // Base delay between steps in the animation

  if(runners < 1 || runners > 8) { _err = ERR_GENERIC; return; }
  // Make an array to hold the indexes of the 'runners'
  uint8_t li [8];
  // Distribute runners around the heart to begin
  for(int8_t i=0; i<runners; i++) {
    li[i] = (NUM_LEDS * i) / runners;
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

    // Animation step complete, step on
    delay(base_delay_ms);
  }
}

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

  const run_around_setting_struct_t run_around_erasing = {
    .fade_speed_major = 0,
    .fade_lower = 0,
    .fade_upper = 255,
    .fade_up_start = 255,
    .base_delay_ms = 150
  };

  for(int i=0; i<10; i++) animate_run_around(i == 0, 1, 4, 0, 1, &run_around_erasing); // 2 runners, one erasers
}

/**
 * Timer interrupt routine; provides software PWM, needs to be fast in order to
 * function correctly.
 */
void callback() {
  // Detect if this function was pre-empted by the current interrupt; if so the PWM is failing, switch to error mode
  if(_isr_running)
    _err = ERR_NESTED_PWM;
  
  if(_err) {
    // Error mode, blink 2 LEDs on to indicate something went wrong
    // Start by driving all LEDs off except for the LED indicating the problem.
    for(uint8_t l=0, el=PIN_LED_START; el<PIN_LED_END; el++, l++) {
      if(el != PIN_LED_ERR0 && el != PIN_LED_ERR1) {
        digitalWrite(el, (l != _err) ? HIGH : LOW); // Leave the LED on that indicates the problem
      }
    }
    // Blink the 2 indicator LEDs
    digitalWrite(PIN_LED_ERR0, (_err_cnt > 0) ? LOW : HIGH);
    digitalWrite(PIN_LED_ERR1, (_err_cnt > 0) ? LOW : HIGH);

    // Increase the counter used to blink the LEDs
    _err_cnt++;
    if(_err_cnt >= ERROR_BLINK_CNT) _err_cnt = -ERROR_BLINK_CNT;

    return;
  } else {
    // -----------------------------------------------------------------------------------------------
    // --                                     software PWM                                          --
    // -- Note: has to complete in 1 interval of the Timer1 interrupt, as tracked by _isr_running.  --
    // --       When this deadline is not met, infinite interrupt recursion would crash the AVR,    --
    // --       so instead _err is set to stop this interrupt handler until reset.                  --
    // -- Note 2: 
    // -----------------------------------------------------------------------------------------------
    // Mark this Interrupt-Service-Routine (ISR) active
    _isr_running = 1;

    // Enable interrupts again; the PWM logic is guarded against nesting via _isr_running and fader logic (which takes 3 to 4 times longer) is guarded against nesting via _isr_fader
    // WARNING: this means from this point on, nested interrupts can occur!!!
    sei();

    MEASUREMENT_ISR_PWM_START; // Measurement start when measuring PWM part only; every tick
    if(fader_update_ptr >= 0)
      MEASUREMENT_ISR_ALL_START; // Measurement start when measuring PWM + fader; every FADER_UPDATE_TICKS ticks

    // Increase PWM counter
    _pwm_step++;
    
    // Do PWM per LED
    // Note: since digitalWrite is very slow, we read the pin status registers, manipulate the copy and write back the result
    // Read pin 0..7 aka PORT D
    uint8_t pin0_7  = PORTD;
    uint8_t pin8_13 = PORTB;

    #if 0
    // Generic loop, optimized to run between 24us and 36us
    for(uint8_t l=0, li=PIN_LED_START; l<NUM_LEDS; l++, li++) {
      // Note: this boundary provides support for switching LEDs completely off, but completely on (255/255) will result in a single low cycle during PWM
      if(_pwm_step >= led_pwm_val[l].major) {
        //digitalWrite(li, HIGH); // slow - replaced by direct port manipulation below
        if(li <= 7) {
          // Pin in port D, turn on
          //pin0_7 |= 0x1 << li;
          pin0_7 |= LED_MASKON_PORTD[l];
        } else if(li <= 13) {
          // Pin in port B, turn on
          //pin8_13 |= 0x1 << (li - 8);
          pin8_13 |= LED_MASKON_PORTB[l - 8];
        } else {
          // Invalid pin number, go into error mode
          _err = ERR_ISR_ERROR;
        }
      } else {
        //digitalWrite(li, LOW); // slow - replaced by direct port manipulation below
        if(li <= 7) {
          // Pin in port D, turn off
          //pin0_7 &= ~(0x1 << li);
          pin0_7 &= LED_MASKOFF_PORTD[l];
        } else if(li <= 13) {
          // Pin in port B, turn on
          //pin8_13 &= ~(0x1 << (li - 8));
          pin8_13 &= LED_MASKOFF_PORTD[l - 8];
        } else {
          // Invalid pin number, go into error mode
          _err = ERR_ISR_ERROR;
        }
      }
    }
    #else
      // Unrolled loop resulting in removal of most of the if-then-else, optimized to run between 8us and 20us
      SOFT_PWM_LED( 2, led_pwm_val[0].major); // LED 0, pin 2
      SOFT_PWM_LED( 3, led_pwm_val[1].major); // LED 1, pin 3
      SOFT_PWM_LED( 4, led_pwm_val[2].major); // LED 2, pin 4
      SOFT_PWM_LED( 5, led_pwm_val[3].major); // LED 3, pin 5
      SOFT_PWM_LED( 6, led_pwm_val[4].major); // LED 4, pin 6
      SOFT_PWM_LED( 7, led_pwm_val[5].major); // LED 5, pin 7
      SOFT_PWM_LED( 8, led_pwm_val[6].major); // LED 6, pin 8
      SOFT_PWM_LED( 9, led_pwm_val[7].major); // LED 7, pin 9
      SOFT_PWM_LED(10, led_pwm_val[8].major); // LED 8, pin 10
      SOFT_PWM_LED(11, led_pwm_val[9].major); // LED 9, pin 11

      // Guard against changes in the design which would mismatch with the original pin layout
      #if NUM_LEDS != 10
      #error "Software PWM logic is for 10 LEDs only!"
      #endif
      #if PIN_LED_START != 2
      #error "Software PWM logic needs LED0 to be at pin 2!"
      #endif
    #endif

    // Apply the new port pin states
    PORTD = pin0_7;
    PORTB = pin8_13;

    // ------------------------------- fader controls -------------------------------------------
    // Increase the counter for the fader interval
    fader_interval_cnt++;

    // Fader interval reached, start updating all faders, one at a time
    if(fader_interval_cnt >= FADER_UPDATE_TICKS) {
      fader_update_ptr = NUM_LEDS - 1;
      // Clear the counter
      fader_interval_cnt = 0;
      // When measuring PWM + fader, on the first fader we missed the PWM part, but measure the fader part to make sure the stops match up
      MEASUREMENT_ISR_ALL_START;
    }

    MEASUREMENT_ISR_PWM_STOP; // When measuring PWM only, stop measuring here

    if(fader_update_ptr >= 0) {
      // Error handling; when this logic is so slow it results in nested fader logic, the program will lock up
      if(_isr_fader) {
        _err = ERR_NESTED_FADER;
        return;
      }
      
      // Update fader; mark fader ISR active and PWM ISR complete
      _isr_fader = 1;    // Note: needs to be set *before* clearing _isr_running
      _isr_running = 0;
      
      // Update the fader which fader_update_ptr points to
      {
        uint8_t l = fader_update_ptr; // FIXME rename l to fader_update_ptr
        //for(uint8_t l=0; l<NUM_LEDS; l++) {
        fader_struct_t * const f = (fader_struct_t * const)&fader[fader_update_ptr];
        const effect_enum_t    e = f->reload;
        if(f->active) {
          // Use a 32 bit to detect over and underflow on the 16 bit PWM counter (note that the uper 8 bits are used for PWM, this allows sub-stepping)
          int32_t newraw = (int32_t)led_pwm_val[fader_update_ptr].raw + (int32_t)f->delta;
          int16_t newmajor = newraw >> 8; // Remove the lower byte to obtain the PWM value (note that the first 8 bits are valid, upper bits are only needed to detect overflow)
          if(newmajor > f->upper) {
            // Upper-bound tripped, handle effect
            switch(e) {
              case NONE:
                // No effect, cap to upper and hold
                led_pwm_val[fader_update_ptr].major = f->upper;
                led_pwm_val[fader_update_ptr].minor = 0;
                f->active = 0;
                break;
              case JUMP:
                // Jump to lower bound
                led_pwm_val[fader_update_ptr].major = f->lower;
                led_pwm_val[fader_update_ptr].minor = 0;
                break;
              case INVERT:
              case UPPER_INVERT:
                // Cap to upper - delta (upper bound was used last update)
                led_pwm_val[fader_update_ptr].major = f->upper - (f->delta >> 8);
                led_pwm_val[fader_update_ptr].minor = 0 - (f->delta & 0xFF);
                f->delta = -f->delta;
                break;
              case LOWER_INVERT:
                // Already inverted once, disable fader
                led_pwm_val[fader_update_ptr].major = f->upper;
                led_pwm_val[fader_update_ptr].minor = 0;
                f->active = 0;
                break;
            }
          } else if(newmajor < f->lower) {
            // Lower bound tripped, handle effect
            switch(e) {
              case NONE:
                // No effect, cap to lower and hold
                led_pwm_val[fader_update_ptr].major = f->lower;
                led_pwm_val[fader_update_ptr].minor = 0;
                f->active = 0;
                break;
              case JUMP:
                // Jump to upper bound
                led_pwm_val[fader_update_ptr].major = f->upper;
                led_pwm_val[fader_update_ptr].minor = 0;
                break;
              case INVERT:
              case LOWER_INVERT:
                // Cap to lower + delta (lower bound was used last update)
                f->delta = -f->delta; // First invert delta, its now positive
                led_pwm_val[fader_update_ptr].major = f->lower + (f->delta >> 8);
                led_pwm_val[fader_update_ptr].minor = (f->delta & 0xFF);
                break;
              case UPPER_INVERT:
                // Already inverted once, disable fader
                led_pwm_val[fader_update_ptr].major = f->lower;
                led_pwm_val[fader_update_ptr].minor = 0;
                f->active = 0;
                break;
              case SETUP_LOWER:
                // Bug/programming fix: make sure the delta is positive
                if(f->delta < 0) {
                  f->delta = -f->delta;
                } else {
                  // Setup fade to the lower bound from *below* the current lower bound; simply apply the new value
                  led_pwm_val[fader_update_ptr].raw = newraw;
                }
                led_pwm_val[fader_update_ptr].raw = newraw;
                break;
            }
          } else {
            // Not below the lower bound or above the upper bound
            if(e == SETUP_LOWER) {
              // Setup to lower bound complete; cap to lower bound
              led_pwm_val[fader_update_ptr].major = f->lower;
              led_pwm_val[fader_update_ptr].minor = 0;
              f->active = 0;
            } else {
              // Normal fade step: apply new value
              led_pwm_val[fader_update_ptr].raw = newraw;
            }
          }
        }

        // Fader updated, move the pointer, when it hits -1 all faders are done
        fader_update_ptr--;
      }

      // When measuring PWM + fader duration, stop here
       MEASUREMENT_ISR_ALL_STOP;

      // End of fader updates; clear the flag (note: _isr_running is not touched as it was cleared before and might have been set again in a nested interrupt)
      _isr_fader = 0;
    } else {
      // No fader update, clear ISR active flag for the PWM logic part
      _isr_running = 0;
    }

    
  }
}

