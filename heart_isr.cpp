/**
 * heart_isr.h - Heart PCB Project - Standard Interrupt Service Routine
 * 
 * @author  Berend Dekens <berend@cyberwizzard.nl>
 * @version 1
 * @date    2018.04.13
 * @license GNUGPLv3
 */

#include "heart_settings.h"
#include "heart_isr.h"
#include "heart_profiling.h"
#include "Arduino.h"

// Only support measuring inside the ISR when measuments in general are enabled
#ifndef SUPPORT_MEASUREMENTS
  #ifdef SUPPORT_ISR_MEASUREMENTS
    #undef SUPPORT_ISR_MEASUREMENTS
  #endif
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

#define SOFT_PWM_LED(led_pin, led_brightness) {          \
  if(_pwm_step >= led_brightness) {                      \
    if(led_pin <= 7) {                                   \
      /* Pin in port D, turn on */                       \
      pin0_7 |= LED_MASKON_PORTD[led_pin - 2];           \
    } else if(led_pin <= 13) {                           \
      /* Pin in port B, turn on */                       \
      pin8_13 |= LED_MASKON_PORTB[led_pin - 8];          \
    } else {                                             \
      /* Invalid pin number, go into error mode */       \
      _err = ERR_ISR_ERROR;                              \
    }                                                    \
  } else {                                               \
    if(led_pin <= 7) {                                   \
      /* Pin in port D, turn off */                      \
      pin0_7 &= LED_MASKOFF_PORTD[led_pin - 2];          \
    } else if(led_pin <= 13) {                           \
      /* Pin in port B, turn on */                       \
      pin8_13 &= LED_MASKOFF_PORTB[led_pin - 8];         \
    } else {                                             \
      /* Invalid pin number, go into error mode */       \
      _err = ERR_ISR_ERROR;                              \
    }                                                    \
  }                                                      \
}

// PORTD masks for all pins, PORTD is for pin 0 to 7, corresponding to LED 0 (pin 2) to 6 (pin 7)
static const uint8_t LED_MASKON_PORTD [6] = {
  0x1 << 2,   // LED 0 = pin 2
  0x1 << 3,
  0x1 << 4,
  0x1 << 5,
  0x1 << 6,
  0x1 << 7,   // LED 5 = pin 7
};
static const uint8_t LED_MASKOFF_PORTD [6] = {
  ~(0x1 << 2),   // LED 0 = pin 2
  ~(0x1 << 3),
  ~(0x1 << 4),
  ~(0x1 << 5),
  ~(0x1 << 6),
  ~(0x1 << 7),   // LED 5 = pin 7
};

// PORTB masks for all pins, PORTB is for pin 8 to 13, corresponding to LED 7 (pin 8) to 10 (pin 11)
static const uint8_t LED_MASKON_PORTB [6] = {
  0x1 << 0,   // LED 7 = pin 8
  0x1 << 1,
  0x1 << 2,
  0x1 << 3,   // LED 10 = pin 11
};
static const uint8_t LED_MASKOFF_PORTB [6] = {
  ~(0x1 << 0),   // LED 7 = pin 8
  ~(0x1 << 1),
  ~(0x1 << 2),
  ~(0x1 << 3),   // LED 10 = pin 11
};

#ifdef SUPPORT_NESTED_ISR
// Since nested interrupts can only occur when explicitly enabled, remove the tracking for nested interrupts when not needed
volatile uint8_t  _isr_running = 0;       // flag to track when the software PWM is not meeting the interrupt interval (because it will result in an infinite recursive interrupt loop)
volatile uint8_t  _isr_fader = 0;         // flag to track when the LED fading logic is not meeting the interrupt interval (because it will result in an infinite recursive interrupt loop)
#endif

uint8_t           _pwm_step = 0;          // PWM step counter for all LEDs

volatile uint16_t fader_interval_cnt = 0; // Faders are updated every ANI_INTERVAL steps of the PWM interrupt
volatile int8_t   fader_update_ptr = -1;  // To speed up the ISR, when the update interval is reached, this pointer is set to the highest LED fader index; as long as its not 0, a single fader at a time is updated during the ISR

volatile int16_t  _err_cnt = 0;           // during error, blink the single LEDs

// Shared error register, when set to non-zero the ISR will show an error using the LEDs
volatile uint8_t  _err = 0; // when non-zero, an error occured and the LEDs will indicate what went wrong

// Shared LED brightness tracking
volatile duint8_t led_pwm_val [NUM_LEDS]; // double uint8_t, the major byte is used for the PWM value

// special type controlling the faders per LED
fader_struct_t fader [NUM_LEDS];

/**
 * Timer interrupt routine; provides software PWM and LED fading logic, needs to be fast in order to
 * function correctly.
 *
 * Currently this ISR is measured to take between 8 us and 24 us depending on the settings.
 */
void heart_isr() {
  #ifdef SUPPORT_NESTED_ISR
    // Detect if this function was pre-empted by the current interrupt; if so the PWM is failing, switch to error mode
    if(_isr_running)
      _err = ERR_NESTED_PWM;
  #endif
  
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
    // -----------------------------------------------------------------------------------------------
    #ifdef SUPPORT_NESTED_ISR
      // Mark this Interrupt-Service-Routine (ISR) active
      _isr_running = 1;

      // Enable interrupts again; the PWM logic is guarded against nesting via _isr_running and fader logic (which takes 3 to 4 times longer) is guarded against nesting via _isr_fader
      // WARNING: this means from this point on, nested interrupts can occur!!!
      sei();
    #endif

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

      #ifdef SUPPORT_NESTED_ISR
        // End of fader updates; clear the flag (note: _isr_running is not touched as it was cleared before and might have been set again in a nested interrupt)
        _isr_fader = 0;
      #endif
    } else {
      #ifdef SUPPORT_NESTED_ISR
        // No fader update, clear ISR active flag for the PWM logic part
        _isr_running = 0;
      #endif
    }

    
  }
}