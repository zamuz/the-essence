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

#include "watch_model.h"
#include "enamel.h"
#include <pebble.h>

static EventHandle* s_evt_handler;

typedef struct {
  ClockState start_state;
  ClockState end_state;
} ClockAnimationContext;

static int64_t prv_interpolate_int64_linear(int64_t from, int64_t to, const AnimationProgress progress) {
    return from + ((progress * (to - from)) / ANIMATION_NORMALIZED_MAX);
}

static ClockState prv_interpolate_clock_states(ClockState *start, ClockState *end, uint32_t progress) {
  return (ClockState) {
    .minute_angle = prv_interpolate_int64_linear(start->minute_angle, end->minute_angle, progress),
    .hour_angle = prv_interpolate_int64_linear(start->hour_angle, end->hour_angle, progress),
    .day_angle = prv_interpolate_int64_linear(start->day_angle, end->day_angle, progress),
    .second_angle = prv_interpolate_int64_linear(start->second_angle, end->second_angle, progress),
    .month_angle = prv_interpolate_int64_linear(start->month_angle, end->month_angle, progress),
    .tick_month_angle = prv_interpolate_int64_linear(start->tick_month_angle, end->tick_month_angle, progress),
    .date = prv_interpolate_int64_linear(start->date, end->date, progress),
    .month = prv_interpolate_int64_linear(start->month, end->month, progress),
    .hour = end->hour
  };
}

static void prv_update_clock_animation(Animation *clock_animation,
                                       const AnimationProgress animation_progress) {
  ClockAnimationContext *clock_context = animation_get_context(clock_animation);
  ClockState interpolated_state = prv_interpolate_clock_states(&clock_context->start_state,
                                                               &clock_context->end_state,
                                                               animation_progress);
  watch_model_handle_clock_change(interpolated_state);
}

static void prv_teardown_clock_animation(Animation *clock_animation) {
  ClockAnimationContext *clock_context = animation_get_context(clock_animation);
  free(clock_context);
}

static void prv_handle_time_update(struct tm *tick_time, TimeUnits units_changed) {
  if (units_changed & SECOND_UNIT) watch_model_handle_seconds_change(tick_time);
  if (units_changed & MINUTE_UNIT) watch_model_handle_time_change(tick_time);
}

void update_subscriptions(int hour) {
  TimeUnits units = enamel_get_display_seconds() ? (SECOND_UNIT | MINUTE_UNIT) : MINUTE_UNIT;
  tick_timer_service_subscribe(units, prv_handle_time_update);
  if (enamel_get_tap_to_animate() && !battery_saver_enabled(hour))
      accel_tap_service_subscribe(accel_tap_handler);
  else
      accel_tap_service_unsubscribe();
}

static void prv_finish_animation(Animation *animation, bool finished, void *context) {
  const time_t t = time(NULL);
  struct tm *now = localtime(&t);
  prv_handle_time_update(now, SECOND_UNIT);
  update_subscriptions(now->tm_hour);
}

int get_day_angle(int day) {
  int angle = 0;
  switch (day) {
    case 0: // sunday
      angle = 327;
      break;
    case 1: // monday
      angle = 30;
      break;
    case 2: // tuesday
      angle = 76;
      break;
    case 3: // wednesday
      angle = 127;
      break;
    case 4: // thursday
      angle = 168;
      break;
    case 5: // friday
      angle = 220;
      break;
    case 6: // saturday
      angle = 270;
      break;
  }
  return angle;
}

static Animation *prv_make_clock_animation(int duration, ClockState start_state, AnimationCurve curve) {
  Animation *clock_animation = animation_create();
  static const AnimationImplementation animation_implementation = {
    .update = prv_update_clock_animation,
    .teardown = prv_teardown_clock_animation
  };
  animation_set_implementation(clock_animation, &animation_implementation);
  animation_set_duration(clock_animation, duration);
  animation_set_delay(clock_animation, CLOCK_ANIMATION_DELAY);
  animation_set_curve(clock_animation, curve);
  ClockAnimationContext *clock_context = malloc(sizeof(*clock_context));
  clock_context->start_state = start_state;
  time_t tm = time(NULL);
  struct tm *now = localtime(&tm);
  clock_context->end_state = (ClockState) {
    .minute_angle = now->tm_min * 6,
    .hour_angle = (now->tm_hour%12)*30 + now->tm_min*.48,
    .day_angle = get_day_angle(now->tm_wday),
    .second_angle = (now->tm_sec + duration*.001)*6,
    .month_angle = now->tm_mon*30,
    .tick_month_angle = now->tm_mon*30 + 30,
    .date = now->tm_mday,
    .month = now->tm_mon,
    .hour = now->tm_hour
  };
  animation_set_handlers(clock_animation, (AnimationHandlers) {
    .stopped = prv_finish_animation
  }, clock_context);
  return clock_animation;
}

int animation_direction(void) {
    return (rand()%2) ? 360 : -360;
}

void schedule_tap_animation(ClockState current_state) {
    //APP_LOG(APP_LOG_LEVEL_DEBUG_VERBOSE, "TAP!");
    ClockState start_state = (ClockState) {
        .minute_angle = current_state.minute_angle + animation_direction(),
        .hour_angle = current_state.hour_angle + animation_direction(),
        .day_angle = current_state.day_angle + animation_direction(),
        .second_angle = current_state.second_angle + animation_direction(),
        .month_angle = current_state.month_angle + animation_direction(),
        .tick_month_angle = current_state.tick_month_angle + animation_direction(),
	.date = current_state.date,
	.month = current_state.month,
	.hour = current_state.hour
    };
    Animation *const tap_animation = prv_make_clock_animation(TAP_ANIMATION_LENGTH,
                                                              start_state,
							      AnimationCurveEaseInOut);
    tick_timer_service_unsubscribe();
    accel_tap_service_unsubscribe();
    animation_schedule(tap_animation);
}

void watch_model_start_intro(ClockState start_state) {
    if (enamel_get_intro_enabled() && !battery_saver_enabled(start_state.hour)) {
        Animation *const clock_animation = prv_make_clock_animation(enamel_get_intro_duration(),
                                                                    start_state,
								    AnimationCurveEaseInOut);
        animation_schedule(clock_animation);
    }
    else {
        prv_finish_animation(NULL, true, NULL);
    }
}

static void prv_msg_received_handler(void *context) {
  watch_model_handle_config_change();
}

void watch_model_init(void) {
  s_evt_handler = enamel_settings_received_subscribe(prv_msg_received_handler, NULL);
}

void watch_model_deinit(void) {
  enamel_settings_received_unsubscribe(s_evt_handler);
}
