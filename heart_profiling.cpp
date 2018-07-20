/**
 * heart_isr.h - Heart PCB Project - Profiling support by means of measurement of duration of code segments
 * 
 * @author  Berend Dekens <berend@cyberwizzard.nl>
 * @version 1
 * @date    2018.04.13
 * @license GNUGPLv3
 */

#include "heart_profiling.h"

#ifdef SUPPORT_MEASUREMENTS

  // Profiling code; records the start and stop times of the ISR
  volatile unsigned long starts [NUM_MEASUREMENTS];
  volatile unsigned long stops  [NUM_MEASUREMENTS];
  volatile uint16_t      measure_start, measure_stop;

#endif