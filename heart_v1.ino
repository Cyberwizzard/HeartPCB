#include "heart_settings.h"
#include "TimerOne.h"

#define ERROR_BLINK_CNT 4000
#define ERR_GENERIC 100
#define ERR_NESTED_PWM 1
#define ERR_NESTED_FADER 2
#define ERR_ISR_ERROR 2

volatile duint8_t led_pwm_val [NUM_LEDS]; // double uint8_t, the major byte is used for the PWM value
volatile uint8_t  _err = 0;               // when non-zero, an error occured and the LEDs will indicate what went wrong
volatile int16_t  _err_cnt = 0;           // during error, blink the single LEDs
volatile uint8_t  _isr_running = 0;       // flag to track when the software PWM is not meeting the interrupt interval (because it will result in an infinite recursive interrupt loop)
volatile uint8_t  _isr_fader = 0;         // flag to track when the LED fading logic is not meeting the interrupt interval (because it will result in an infinite recursive interrupt loop)
uint8_t           _pwm_step = 0;          // PWM step counter for all LEDs
volatile uint16_t fader_interval_cnt = 0; // Faders are updated every ANI_INTERVAL steps of the PWM interrupt

// special type controlling the faders per LED
fader_struct_t fader [NUM_LEDS];

#if 1
#define NUM_MEASUREMENTS 100
#define MEASUREMENT_INIT  { Serial.begin(9600); Serial.println("Profiling active"); delay(100); }
#define MEASUREMENT_START { if(measure_start < NUM_MEASUREMENTS) { starts[measure_start] = micros(); measure_start++; }}
#define MEASUREMENT_STOP  { if(measure_stop < NUM_MEASUREMENTS) { stops[measure_stop] = micros(); measure_stop++; }}
#define MEASUREMENT_PRINT { if(measure_stop >= NUM_MEASUREMENTS && measure_stop != 127) { \
  measure_stop = 127;                                                                     \
  Serial.println("Profiling (duration, interval):");                                      \
  for(int __i = 0; __i < NUM_MEASUREMENTS; __i++) {                                       \
    unsigned long dur = stops[__i] - starts[__i];                                         \
    Serial.print(dur); Serial.print(" us, ");                                             \
    if(__i > 0) {                                                                         \
      unsigned long interval = starts[__i] - starts[__i - 1];                             \
      Serial.print(interval); Serial.println(" us");                                      \
    } else Serial.println(" n/a");                                                        \
  }                                                                                       \
}}
#else
#define NUM_MEASUREMENTS 1
#define MEASUREMENT_INIT  { }
#define MEASUREMENT_START { }
#define MEASUREMENT_STOP  { }
#define MEASUREMENT_PRINT { }
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

  //Serial.begin(9600);
  MEASUREMENT_INIT;
 // Serial.print(TIMER_INTERVAL_US);
 // Serial.println("us PWM interval");
 // Serial.print(FADER_UPDATE_TICKS);
 // Serial.println("ticks for fader");
 // delay(2000);

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
  Serial.println("OK");
  
  // Use timer1 for the intervals for the software PWM and faders
  Timer1.initialize(TIMER_INTERVAL_US);      // initialize timer1 and set to a high pace interval
  Timer1.attachInterrupt(callback, 65);      // attaches callback() as a timer overflow interrupt

  Serial.println("OK2");
}

/**
 * Running animation: one or more 'runners' run around the heart turning on LEDs as they go.
 * @param dir Direction for the animation, valid values are -1 and 1
 * @param runners Number of parallel runners, valid range is 1 to 8
 * @param cross Whether alternating runners move in the same direction or counter-directions
 * @param erasers Whether alternating runners should 'erasers'; these runners fade out LEDs rather than in
 * @param setup Configure all LEDs with brightness bounds and fade to them at the start of the animation, only needed when switching animation types
 */

void animate_run_around(const int8_t setup = 1, const int8_t dir = 1, const int8_t runners = 1, const int8_t cross = 0, const int8_t erasers = 0) {
  const uint8_t fade_speed_major = 25;     // Speed of the fade up and fade down
  const uint8_t fade_lower = 20;           // Lower boundary for dimmed LEDs
  const uint8_t fade_upper = 255;          // Upper boundary for bright LEDs
  const uint8_t fade_up_start = 200;       // Runners set this goal for their LED, causing a soft-start

  if(runners < 1 || runners > 8) { _err = ERR_GENERIC; return; }
  // Make an array to hold the indexes of the 'runners'
  //uint8_t *li = (uint8_t *)malloc(runners * sizeof(uint8_t));
  uint8_t li [8];
  //if(*li == -1) { _err = ERR_GENERIC; return; }
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
        fader[led].reload = UPPER_INVERT;
        fader[led].delta = fade_speed_major << 8;
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
    }

    delay(100 * runners);
  }

  // OLD
  /*
  for(int8_t li=0; li < NUM_LEDS; li++) {
    // Compute the current LED index (which might be inverted)
    int8_t _li = (dir >= 0) ? li : NUM_LEDS - 1 - li;

    // Configure the fader
    fader[_li].delta = 25 << 8;
    fader[_li].reload = UPPER_INVERT;
    fader[_li].lower = 20;
    fader[_li].upper = 255;
    led_pwm_val[_li].major = 220;
    led_pwm_val[_li].minor = 0;
    // Mark it as active
    fader[_li].active = 1;

    delay(20);
  }*/
}

void loop() {
  MEASUREMENT_PRINT;
  // put your main code here, to run repeatedly:
  for(int i=0; i<10; i++) animate_run_around(i == 0);
  
  //for(int i=0; i<10; i++) animate_run_around(i == 0, -1);

  //for(int i=0; i<10; i++) animate_run_around(i == 0, 1, 2);

  //for(int i=0; i<10; i++) animate_run_around(i == 0, 1, 4);

  //for(int i=0; i<10; i++) animate_run_around(i == 0, 1, 2, 1);

 //for(int i=0; i<10; i++) animate_run_around(i == 0, 1, 6, 0, 1);
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

    MEASUREMENT_START;


    
    // Increase PWM counter
    _pwm_step++;
    
    // Do PWM per LED
    // Note: since digitalWrite is very slow, we read the pin status registers, manipulate the copy and write back the result
    // Read pin 0..7 aka PORT D
    uint8_t pin0_7  = PORTD;
    uint8_t pin8_13 = PORTB;
    
    for(uint8_t l=0, li=PIN_LED_START; l<NUM_LEDS; l++, li++) {
      // Note: this boundary provides support for switching LEDs completely off, but completely on (255/255) will result in a single low cycle during PWM
      if(_pwm_step >= led_pwm_val[l].major) {
        //digitalWrite(li, HIGH); // slow - replaced by direct port manipulation below
        if(li <= 7) {
          // Pin in port D, turn on
          pin0_7 |= 0x1 << li;
        } else if(li <= 13) {
          // Pin in port B, turn on
          pin8_13 |= 0x1 << (li - 8);
        } else {
          // Invalid pin number, go into error mode
          _err = ERR_ISR_ERROR;
        }
      } else {
        //digitalWrite(li, LOW); // slow - replaced by direct port manipulation below
        if(li <= 7) {
          // Pin in port D, turn off
          pin0_7 &= ~(0x1 << li);
        } else if(li <= 13) {
          // Pin in port B, turn on
          pin8_13 &= ~(0x1 << (li - 8));
        } else {
          // Invalid pin number, go into error mode
          _err = ERR_ISR_ERROR;
        }
      }
    }

    // Apply the new port pin states
    PORTD = pin0_7;
    PORTB = pin8_13;

    // ------------------------------- fader controls -------------------------------------------
    // Increase the counter for the fader interval
    fader_interval_cnt++;

    MEASUREMENT_STOP;

    if(fader_interval_cnt >= FADER_UPDATE_TICKS) {
      // Error handling; when this logic is so slow it results in nested fader logic, the program will lock up
      if(_isr_fader) {
        _err = ERR_NESTED_FADER;
        return;
      }
      
      // Update fader; mark fader ISR active and PWM ISR complete
      _isr_fader = 1;    // Note: needs to be set *before* clearing _isr_running
      _isr_running = 0;
      
      // Loop over all faders and update where applicable
      for(uint8_t l=0; l<NUM_LEDS; l++) {
        fader_struct_t * const f = (fader_struct_t * const)&fader[l];
        const effect_enum_t    e = f->reload;
        if(f->active) {
          // Use a 32 bit to detect over and underflow on the 16 bit PWM counter (note that the uper 8 bits are used for PWM, this allows sub-stepping)
          int32_t newraw = (int32_t)led_pwm_val[l].raw + (int32_t)f->delta;
          int16_t newmajor = newraw >> 8; // Remove the lower byte to obtain the PWM value (note that the first 8 bits are valid, upper bits are only needed to detect overflow)
          if(newmajor > f->upper) {
            // Upper-bound tripped, handle effect
            switch(e) {
              case NONE:
                // No effect, cap to upper and hold
                led_pwm_val[l].major = f->upper;
                led_pwm_val[l].minor = 0;
                f->active = 0;
                break;
              case JUMP:
                // Jump to lower bound
                led_pwm_val[l].major = f->lower;
                led_pwm_val[l].minor = 0;
                break;
              case INVERT:
              case UPPER_INVERT:
                // Cap to upper - delta (upper bound was used last update)
                led_pwm_val[l].major = f->upper - (f->delta >> 8);
                led_pwm_val[l].minor = 0 - (f->delta & 0xFF);
                f->delta = -f->delta;
                break;
              case LOWER_INVERT:
                // Already inverted once, disable fader
                led_pwm_val[l].major = f->upper;
                led_pwm_val[l].minor = 0;
                f->active = 0;
                break;
            }
          } else if(newmajor < f->lower) {
            // Lower bound tripped, handle effect
            switch(e) {
              case NONE:
                // No effect, cap to lower and hold
                led_pwm_val[l].major = f->lower;
                led_pwm_val[l].minor = 0;
                f->active = 0;
                break;
              case JUMP:
                // Jump to upper bound
                led_pwm_val[l].major = f->upper;
                led_pwm_val[l].minor = 0;
                break;
              case INVERT:
              case LOWER_INVERT:
                // Cap to lower + delta (lower bound was used last update)
                f->delta = -f->delta; // First invert delta, its now positive
                led_pwm_val[l].major = f->lower + (f->delta >> 8);
                led_pwm_val[l].minor = (f->delta & 0xFF);
                break;
              case UPPER_INVERT:
                // Already inverted once, disable fader
                led_pwm_val[l].major = f->lower;
                led_pwm_val[l].minor = 0;
                f->active = 0;
                break;
              case SETUP_LOWER:
                // Bug/programming fix: make sure the delta is positive
                if(f->delta < 0) {
                  f->delta = -f->delta;
                } else {
                  // Setup fade to the lower bound from *below* the current lower bound; simply apply the new value
                  led_pwm_val[l].raw = newraw;
                }
                led_pwm_val[l].raw = newraw;
                break;
            }
          } else {
            // Not below the lower bound or above the upper bound
            if(e == SETUP_LOWER) {
              // Setup to lower bound complete; cap to lower bound
              led_pwm_val[l].major = f->lower;
              led_pwm_val[l].minor = 0;
              f->active = 0;
            } else {
              // Normal fade step: apply new value
              led_pwm_val[l].raw = newraw;
            }
          }
        }

        // End of fader updates; clear the flag (note: _isr_running is not touched as it was cleared before and might have been set again in a nested interrupt)
        _isr_fader = 0;
      }

      // Clear the counter
      fader_interval_cnt = 0;

      
    } else {
      // No fader update, clear ISR active flag for the PWM logic part
      _isr_running = 0;
    }

    
  }
}

