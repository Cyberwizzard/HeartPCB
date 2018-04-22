/**
 * heart_delay.cpp - Heart PCB Project - Delay function which supports aborting the delay if needed
 * 
 * @author  Berend Dekens <berend@cyberwizzard.nl>
 * @version 1
 * @date    2018.04.22
 * @license GNUGPLv3
 */

#include "heart_delay.h"
#include "Arduino.h"

// Special flag set when heart_delay() should stop any delay and return control to the main loop
volatile uint8_t _abort_heart_delay = 0;

/**
 * Modified version of delay() which aborts when _abort_heart_delay turns 1.
 * @return 1 when the delay is aborted, 0 when is completed like normal delay()
 */
uint8_t heart_delay(unsigned long ms)
{
  uint32_t start = micros();

  while (ms > 0 && _abort_heart_delay == 0) {
    yield();
    while ( ms > 0 && (micros() - start) >= 1000) {
      ms--;
      start += 1000;
    }
  }

  return _abort_heart_delay;
}