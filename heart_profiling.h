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

  // When measurements for profiling are enabled, Serial support is also enabled
  #define SERPRINT(x)   Serial.print(x)
  #define SERPRINTLN(x) Serial.println(x)

  // Number of measurements to make
  // Default: 100, max 254
  #define NUM_MEASUREMENTS 50

  // Profiling code; records the start and stop times of the ISR
  extern volatile unsigned long starts [NUM_MEASUREMENTS];
  extern volatile unsigned long stops  [NUM_MEASUREMENTS];
  extern volatile uint16_t      measure_start, measure_stop;

#if 1
  // Classical profiling: record a number of entry and exit times and print them after a while
  #define MEASUREMENT_INIT  { Serial.begin(9600); SERPRINTLN("Profiling active"); delay(100); }
  #define MEASUREMENT_START { if(measure_start < NUM_MEASUREMENTS) { starts[measure_start] = micros(); measure_start++; }}
  #define MEASUREMENT_STOP  { if(measure_stop < NUM_MEASUREMENTS) { stops[measure_stop] = micros(); measure_stop++; }}
  #define MEASUREMENT_PRINT { if(measure_stop >= NUM_MEASUREMENTS && measure_stop != 255) { \
    unsigned long mindur, maxdur, minival, maxival;                                         \
    measure_stop = 255;                                                                     \
    SERPRINTLN("Profiling (duration, interval):");                                          \
    for(int __i = 0; __i < NUM_MEASUREMENTS; __i++) {                                       \
      unsigned long dur = stops[__i] - starts[__i];                                         \
      if(__i == 0) { mindur = dur; maxdur = dur; }                                          \
      else if(dur > maxdur) { maxdur = dur; }                                               \
      else if(dur < mindur) { mindur = dur; }                                               \
      SERPRINT(dur); SERPRINT(" us, ");                                                     \
      if(__i > 0) {                                                                         \
        unsigned long interval = starts[__i] - starts[__i - 1];                             \
        if(__i == 1 || __i == 2) { minival = interval; maxival = interval; }                \
        else if(interval > maxival) { maxival = interval; }                                 \
        else if(interval < minival) { minival = interval; }                                 \
        SERPRINT(interval); SERPRINTLN(" us");                                              \
      } else SERPRINTLN(" n/a");                                                            \
    }                                                                                       \
    SERPRINT("Dur: ["); SERPRINT(mindur);                                                   \
    SERPRINT(" us, "); SERPRINT(maxdur); SERPRINTLN(" us]");                                \
    SERPRINT("Interval: ["); SERPRINT(minival);                                             \
    SERPRINT(" us, "); SERPRINT(maxival); SERPRINTLN(" us]");                               \
  }}
#else
  // Continuous profiling: only capture the bounds
  #define NUM_CMEASUREMENTS 30000
  
  #define MEASUREMENT_INIT  { Serial.begin(9600); SERPRINTLN("Profiling active"); delay(100); starts[0] = 0; starts[1] = 0; starts[2] = 0; starts[3] = 0; starts[4] = 0; starts[5] = 0; starts[6] = 0; }
  
  // 0 = lower bound duration, 1 = upper bound duration, 2 = lower bound interval, 
  // 3 = upper bound interval, 4 = last start time, 5 = start count, 6 = stop count
  #define MEASUREMENT_START if(measure_start < NUM_CMEASUREMENTS) {                         \
    unsigned long curtime = micros(), ival;                                                 \
    starts[5]++;  /* Update start count */                                                  \
    ival = curtime - starts[4]; /* Compute interval */                                      \
    if(measure_start > 2) { /* After startup, do interval bound tracking */                 \
      if(ival > starts[3]) starts[3] = ival;                                                \
      if(ival < starts[2]) starts[2] = ival;                                                \
    } else {                                                                                \
      starts[2] = ival;  /* First few iterations, simply copy the interval */               \
      starts[3] = ival;                                                                     \
    }                                                                                       \
                                                                                            \
    starts[4] = curtime; /* Record start time */                                            \
    starts[5] = 1; /* Mark that a start has been recorded */                                \
    measure_start++;                                                                        \
  }

  #define MEASUREMENT_STOP {if(measure_stop < NUM_CMEASUREMENTS) {                          \
    unsigned long curtime = micros(), dur;                                                  \
    starts[6]++; /* Update stop count */                                                    \
    dur = curtime - starts[4]; /* Compute duration */                                       \
    if(measure_start > 1) { /* After startup, do interval bound tracking */                 \
      if(dur > starts[1]) starts[1] = dur;                                                  \
      if(dur < starts[0]) starts[0] = dur;                                                  \
    } else {                                                                                \
      starts[2] = dur;  /* First simply copy the duration */                                \
      starts[3] = dur;                                                                      \
    }                                                                                       \
                                                                                            \
    measure_stop++;                                                                         \
  }}

  #define MEASUREMENT_PRINT { if(measure_stop != NUM_CMEASUREMENTS + 2) {                   \
    unsigned long mindur, maxdur, minival, maxival;                                         \
    measure_stop = NUM_CMEASUREMENTS + 2;                                                   \
    SERPRINTLN("Profiling (continuous measurements):");                                     \
    SERPRINT("Dur: ["); SERPRINT(starts[0]);                                                \
    SERPRINT(" us, "); SERPRINT(starts[1]); SERPRINTLN(" us]");                             \
    SERPRINT("Interval: ["); SERPRINT(starts[2]);                                           \
    SERPRINT(" us, "); SERPRINT(starts[3]); SERPRINTLN(" us]");                             \
    if(starts[5] != starts[6]) {                                                            \
      SERPRINTLN("Start/stop invalid - all measurements invalid!");                         \
      SERPRINT("Starts "); SERPRINT(starts[5]); SERPRINT(" - Stops: "); SERPRINTLN(starts[6]);\
  }}}

#endif
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