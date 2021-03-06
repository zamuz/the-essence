/*
    Copyright (C) 2022 Gonzalo Munoz.

    This file is part of The Essence.

    The Essence is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with The Essence.  If not, see <http://www.gnu.org/licenses/>.
*/

#include <pebble.h>

#define CLOCK_ANIMATION_LENGTH 2500
#define CLOCK_ANIMATION_DELAY 0
#define MINUTE_ANIMATION_LENGTH 200
#define TAP_ANIMATION_LENGTH 2500

typedef struct {
  int32_t minute_angle;
  int32_t hour_angle;
  int32_t day_angle;
  int32_t second_angle;
  int32_t month_angle;
  int32_t tick_month_angle;
  int date;
  int month;
  int hour;
} ClockState;

void watch_model_start_intro(ClockState start_state);
void watch_model_init(void);
void watch_model_deinit(void);

void watch_model_handle_clock_change(ClockState state);
void watch_model_handle_time_change(struct tm *tick_time);
void watch_model_handle_seconds_change(struct tm *tick_time);
void watch_model_handle_config_change(void);
void schedule_minute_animation(ClockState current_state);
void schedule_tap_animation(ClockState current_state);
void accel_tap_handler(AccelAxisType axis, int32_t direction);
void update_subscriptions(int hour);
int get_day_angle(int day);
bool battery_saver_enabled(int hour);
