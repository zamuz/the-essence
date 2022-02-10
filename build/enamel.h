/**
 * This file was generated with Enamel : http://gregoiresage.github.io/enamel
 */

#ifndef ENAMEL_H
#define ENAMEL_H

#include <pebble.h>

// -----------------------------------------------------
// Getter for 'screen_color'
GColor enamel_get_screen_color();
// -----------------------------------------------------

// -----------------------------------------------------
// Getter for 'clock_bg_color'
GColor enamel_get_clock_bg_color();
// -----------------------------------------------------

// -----------------------------------------------------
// Getter for 'clock_fg_color'
GColor enamel_get_clock_fg_color();
// -----------------------------------------------------

// -----------------------------------------------------
// Getter for 'hour_hand_color'
GColor enamel_get_hour_hand_color();
// -----------------------------------------------------

// -----------------------------------------------------
// Getter for 'minute_hand_color'
GColor enamel_get_minute_hand_color();
// -----------------------------------------------------

// -----------------------------------------------------
// Getter for 'subdial_highlight_color'
GColor enamel_get_subdial_highlight_color();
// -----------------------------------------------------

// -----------------------------------------------------
// Getter for 'animate_minutes'
bool enamel_get_animate_minutes();
// -----------------------------------------------------

// -----------------------------------------------------
// Getter for 'display_seconds'
bool enamel_get_display_seconds();
// -----------------------------------------------------

void enamel_init();

void enamel_deinit();

typedef void* EventHandle;
typedef void(EnamelSettingsReceivedHandler)(void* context);

EventHandle enamel_settings_received_subscribe(EnamelSettingsReceivedHandler *handler, void *context);
void enamel_settings_received_unsubscribe(EventHandle handle);

#endif