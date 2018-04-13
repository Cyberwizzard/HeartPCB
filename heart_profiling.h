/**
 * heart_isr.h - Heart PCB Project - Profiling support by means of measurement of duration of code segments
 * 
 * @author  Berend Dekens <berend@cyberwizzard.nl>
 * @version 1
 * @date    2018.04.13
 * @license GNUGPLv3
 */
#ifndef _HEART_PROFILING_H_
#define _HEART_PROFILING_H_

#include "heart_settings.h"

#ifdef SUPPORT_MEASUREMENTS
  // Number of measurements to make
  // Default: 100, max 126
  #define NUM_MEASUREMENTS 100

  // Profiling code; records the start and stop times of the ISR
  extern volatile unsigned long starts [NUM_MEASUREMENTS];
  extern volatile unsigned long stops  [NUM_MEASUREMENTS];
  extern volatile uint8_t       measure_start, measure_stop;

  // When measurements for profiling are enabled, Serial support is also enabled
  #define SERPRINT(x)   Serial.print(x)
  #define SERPRINTLN(x) Serial.println(x)

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
  #define SERPRINT(x)
  #define SERPRINTLN(x)

  #define NUM_MEASUREMENTS 0
  #define MEASUREMENT_INIT  { }
  #define MEASUREMENT_START { }
  #define MEASUREMENT_STOP  { }
  #define MEASUREMENT_PRINT { }
#endif

#endif