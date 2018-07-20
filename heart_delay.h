/**
 * heart_delay.h - Heart PCB Project - Delay function which supports aborting the delay if needed
 * 
 * @author  Berend Dekens <berend@cyberwizzard.nl>
 * @version 1
 * @date    2018.04.22
 * @license GNUGPLv3
 */
#ifndef _HEART_DELAY_H_
#define _HEART_DELAY_H_

#include "heart_settings.h"

// Special flag set when heart_delay() should stop any delay and return control to the main loop
extern volatile uint8_t _abort_heart_delay;

/**
 * Modified version of delay() which aborts when _abort_heart_delay turns 1.
 * @return 1 when the delay is aborted, 0 when is completed like normal delay()
 */
uint8_t heart_delay(unsigned long ms);

/**
 * Abort the heart_delay() function; subsequent calls will simply abort immediately
 */
static inline void disable_heart_delay() {
  _abort_heart_delay = 1;
  barrier();
}

/**
 * Clear the flag that aborts the heart_delay() function
 */
static inline void enable_heart_delay() {
  _abort_heart_delay = 0;
  barrier();
}

/**
 * Return if the delay was aborted.
 */
static inline uint8_t heart_delay_aborted() {
  return _abort_heart_delay;
}

#endif