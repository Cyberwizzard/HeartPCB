/**
 * heart_eeprom.h - Heart PCB Project - Delay function which supports aborting the delay if needed
 * 
 * @author  Berend Dekens <berend@cyberwizzard.nl>
 * @version 1
 * @date    2018.04.22
 * @license GNUGPLv3
 */
#ifndef _HEART_EEPROM_H_
#define _HEART_EEPROM_H_

#include "heart_settings.h"

typedef struct {
  uint8_t animation_id; // Last active animation ID
  uint8_t demo_mode;    // Demo mode level: 0 = off, manually cycle through animations, >0 automatically change where higher numbers indicate wait time multipliers, max: 5
  uint8_t brightness;   // Value 0-5
  uint8_t carrot;       // Always 42, when something else the data is not valid
} __attribute__ ((packed)) eeprom_settings_t;

// Expose the EEPROM settings for use elsewhere
extern eeprom_settings_t eeprom_settings;

#define BUILD_BUG_ON(condition) ((void)sizeof(char[1 - 2*!!(condition)]))

/**
 * Check if the EEPROM was initialized before; if it was, the magic number should be written at the start
 */
void eeprom_check();

/**
 * Initialize the EEPROM content so its all defined
 */
void eeprom_init();

/**
 * Store the settings struct into EEPROM. Note that the location is changed every time to do wear levelling.
 */
void eeprom_store();

/**
 * Get the settings struct from EEPROM. Because of the wear levelling we have to search for its location so only do this on boot-up.
 */
void eeprom_load();

#endif