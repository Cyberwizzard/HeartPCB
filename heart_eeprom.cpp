/**
 * heart_eeprom.cpp - Heart PCB Project - EEPROM management to save and restore settings
 * 
 * @author  Berend Dekens <berend@cyberwizzard.nl>
 * @version 1
 * @date    2018.07.20
 * @license GNUGPLv3
 */

/**
 * EEPROM layout:
 *   0            4 + N * 4              4 + (N+1) * 4
 * | 4 bytes    | X bytes (can be 0)   | 4 bytes           |
 * | MAGIC      | old unused data      | eeprom_settings_t |
 *
 * After using a position, the carrot field is set to something not 42, marking it as invalid.
 * By shifting the position where the settings are stored, the EEPROM is worn out gradually (years of use when writing every 20 seconds)
 */


#include "heart_eeprom.h"
#include "heart_profiling.h"
#include "heart_settings.h"

#ifdef SUPPORT_EEPROM
#include "EEPROM.h"
#include "Arduino.h"

#if 0
#define EEPROM_SERPRINT(x)   SERPRINT(x)
#define EEPROM_SERPRINTLN(x) SERPRINTLN(x)
#else
#define EEPROM_SERPRINT(x)
#define EEPROM_SERPRINTLN(x)
#endif

// Magic value to store at position 0 in the EEPROM; when this differs the EEPROM content is invalid
#define EEPROM_MAGIC 0xCAFED00D

// Number of setting structs that can be written after another before having to start over. Note that the magic number at the start uses up 1 position
#define NUM_SETTINGS ((E2END + 1 / 4) - 1)
//#define NUM_SETTINGS 25

volatile int16_t eeprom_pos = -1; // Pointer to the location where the settings will be stored next - updated when loading the EEPROM on boot-up
int eeprom_ok = 0;                // On the first call to check_eeprom this is set to 1, so the magic number is only read once

eeprom_settings_t eeprom_settings; // Struct containing the current settings (shared as an exernal)

/**
 * Check if the EEPROM was initialized before; if it was, the magic number should be written at the start
 */
void eeprom_check() {
  uint32_t magic;
  // Make sure the settings struct is indeed packed (results in a compiler error, no code is added, can be done in any function)
  BUILD_BUG_ON( sizeof(eeprom_settings_t) != 4 );
  
  if(eeprom_ok) return;
  EEPROM_SERPRINTLN("EEPROM check");
  EEPROM.get(0, magic);
  if(magic != EEPROM_MAGIC) eeprom_init();
    
  // Flag to only do this once
  eeprom_ok = 1;
}

/**
 * Initialize the EEPROM content so its all defined
 */
void eeprom_init() {
  EEPROM_SERPRINTLN("EEPROM init");
  
  // Erase remainder of EEPROM
  for(int a = 1; a < NUM_SETTINGS; a++) {
    EEPROM.put(a * 4, (uint32_t)0);
  }
  
  // Write the magic word
  EEPROM.put(0, (uint32_t)EEPROM_MAGIC);
  
  // EEPROM ready for use
  //SERPRINTLN(F("EEPROM inited"));
  EEPROM_SERPRINTLN("EEPROM inited");
}

/**
 * Store the settings struct into EEPROM. Note that the location is changed every time to do wear levelling.
 */
void eeprom_store() {
  // When calling store without load, the position is wrong - attempt to load it first and then write
  if(eeprom_pos == -1) eeprom_load();
  // Now eeprom_pos is valid and ready to be used; use it to compute the potential previous position and wipe that
  int prev = eeprom_pos - 1;
  eeprom_settings.carrot = 42; // Hardcode to 42
  // Store settings
  EEPROM.put(4 + eeprom_pos * 4, eeprom_settings);
  //SERPRINT(F("EEPROM store at pos "));
  EEPROM_SERPRINT("EEPROM store at pos ");
  EEPROM_SERPRINTLN(eeprom_pos);
  // Move pointer to next position, honoring wrap around
  eeprom_pos = (eeprom_pos+1) % NUM_SETTINGS;
  
  // Invalidate previous setting
  if(prev < 0) prev += NUM_SETTINGS;
  EEPROM.put(4 + prev * 4, (uint32_t)0);
  //SERPRINT(F("EEPROM erased prev at pos "));
  EEPROM_SERPRINT("EEPROM erased prev at pos ");
  EEPROM_SERPRINTLN(prev);
}

/**
 * Get the settings struct from EEPROM. Because of the wear levelling we have to search for its location so only do this on boot-up.
 */
void eeprom_load() {
  // Check the EEPROM before using it
  eeprom_check();
  // Reset the position counter
  eeprom_pos = 0;
  
  // Now scan for the previous position that held data
  for(int o=0; o<NUM_SETTINGS; o++) {
    // Get the struct at this EEPROM position
    EEPROM.get(4+o*4, eeprom_settings);
    
    if(eeprom_settings.carrot == 42) {
      // Debug output
      //EEPROM_SERPRINT(F("EEPROM load pos "));
      EEPROM_SERPRINT("EEPROM load pos ");
      EEPROM_SERPRINTLN(o);
      // Record the position for the next store
      eeprom_pos = (o+1) % NUM_SETTINGS;
      break;
    }
  }
  
  // See if the settings are valid
  if(eeprom_settings.carrot != 42) {
    // No valid settings found - loading defaults
    EEPROM_SERPRINTLN(F("EEPROM loading defaults"));
    // Set store pointer to start
    eeprom_pos = 0;
    // Set defaults, all zeros is fine
    eeprom_settings = {};
  } else {
    EEPROM_SERPRINT("EEPROM loaded (ani, demo, brightness, next pos): ");
    EEPROM_SERPRINT(eeprom_settings.animation_id);
    EEPROM_SERPRINT(" ");
    EEPROM_SERPRINT(eeprom_settings.demo_mode);
    EEPROM_SERPRINT(" ");
    EEPROM_SERPRINT(eeprom_settings.brightness);
    EEPROM_SERPRINT(" ");
    EEPROM_SERPRINTLN(eeprom_pos); 
  }
  
  // Sanity checking
  if(eeprom_settings.animation_id >= NUM_ANIMATIONS) {
    //SERPRINT(F("EEPROM invalid animation ID "));
    EEPROM_SERPRINTLN(eeprom_settings.animation_id);
    eeprom_settings.animation_id = 0;
  }
  if(eeprom_settings.demo_mode > 5) {
    //SERPRINT(F("EEPROM invalid demo level "));
    EEPROM_SERPRINTLN(eeprom_settings.demo_mode);
    eeprom_settings.demo_mode = 0;
  }
  if(eeprom_settings.brightness > 5) {
    //SERPRINTLN(F("EEPROM invalid brightness "));
    EEPROM_SERPRINTLN(eeprom_settings.brightness);
    eeprom_settings.brightness = 0;
  }
}

#else
// *** No EEPROM support ***

// Dummy settings struct
eeprom_settings_t eeprom_settings = {.animation_id = 0, .demo_mode = 1, .brightness = 0, .carrot = 42};

void eeprom_check() {}

/**
 * Initialize the EEPROM content so its all defined
 */
void eeprom_init() {}

/**
 * Store the settings struct into EEPROM. Note that the location is changed every time to do wear levelling.
 */
void eeprom_store() {}

/**
 * Get the settings struct from EEPROM. Because of the wear levelling we have to search for its location so only do this on boot-up.
 */
void eeprom_load() {}

#endif