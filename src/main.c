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

#include "enamel.h"
#include "watch_model.h"
#include <pebble-events/pebble-events.h>
#include <ctype.h>
#include "text_render.h"

static Window *window;
static Layer *clock_layer;
static Layer *seconds_date_layer;
static Layer *day_layer;
static Layer *marks_layer;
ClockState clock_state;
GFont digital_font;

void watch_model_handle_clock_change(ClockState state) {
  clock_state = state;
  layer_mark_dirty(clock_layer);
  layer_mark_dirty(seconds_date_layer);
  layer_mark_dirty(day_layer);
}

void watch_model_handle_time_change(struct tm *tick_time) {
  //APP_LOG(APP_LOG_LEVEL_DEBUG_VERBOSE, "MINUTES update");
  if (enamel_get_animate_minutes()){
    if (clock_state.minute_angle == 354) {
      clock_state.minute_angle = -6;
      if (clock_state.hour_angle == 330) clock_state.hour_angle = -2;
      if (clock_state.day_angle == 327 && tick_time->tm_hour == 23) clock_state.day_angle = -33;
    }
    schedule_minute_animation(clock_state);
  }
  else {
    clock_state.minute_angle = tick_time->tm_min * 6;
    clock_state.hour_angle = tick_time->tm_hour%12 * 30;
    clock_state.day_angle = (tick_time->tm_wday-1)*52 + 13;
    clock_state.second_angle = tick_time->tm_sec * 6;
    clock_state.date = get_day_angle(tick_time->tm_mday);
    clock_state.month = tick_time->tm_mon;
    layer_mark_dirty(clock_layer);
    layer_mark_dirty(seconds_date_layer);
    layer_mark_dirty(day_layer);
  }
}

void watch_model_handle_seconds_change(struct tm *tick_time) {
  clock_state.second_angle = tick_time->tm_sec * 6;
  layer_mark_dirty(seconds_date_layer);
  //APP_LOG(APP_LOG_LEVEL_DEBUG_VERBOSE, "SECONDS update");
}

void watch_model_handle_config_change(void) {
  update_tick_timer_subscription();
  layer_mark_dirty(clock_layer);
  layer_mark_dirty(seconds_date_layer);
  layer_mark_dirty(day_layer);
  //APP_LOG(APP_LOG_LEVEL_DEBUG_VERBOSE, "CONFIG update");
}

static void draw_date_seconds(Layer *layer, GContext *ctx) {
    GRect layer_bounds = layer_get_bounds(layer);
    int w = layer_bounds.size.w;
    int h = layer_bounds.size.h;
    GRect seconds_center_rect = (GRect) { .size = GSize(w*.5, h*.5) };
    grect_align(&seconds_center_rect, &layer_bounds, GAlignCenter, false);
    GRect seconds_frame = grect_centered_from_polar(seconds_center_rect, GOvalScaleModeFitCircle,
                                                    DEG_TO_TRIGANGLE(clock_state.minute_angle-55),
                                                    GSize(w*.17, h*.17));
    // second dial background
    graphics_context_set_fill_color(ctx, enamel_get_clock_bg_color());
    graphics_fill_radial(ctx, seconds_frame, GOvalScaleModeFitCircle, w*.09, 0, TRIG_MAX_ANGLE);
    if (enamel_get_display_seconds()) {
        // second dial markers
        int sec;
        for (sec = 0; sec < 360; sec = sec+30 ) {
            int angle_from = sec - 2;
            int angle_to = sec + 5;
            GColor mark_color = (sec == 0) ?
		                enamel_get_subdial_highlight_color() :
				enamel_get_clock_fg_color();
            graphics_context_set_fill_color(ctx, mark_color);
            graphics_fill_radial(ctx, seconds_frame, GOvalScaleModeFitCircle, w*.025,
                                 DEG_TO_TRIGANGLE(angle_from), DEG_TO_TRIGANGLE(angle_to));
        }
        // seconds hand
        // end point
        GPoint sec_to = gpoint_from_polar(seconds_frame, GOvalScaleModeFitCircle,
                                          DEG_TO_TRIGANGLE(clock_state.second_angle));
        // draw seconds hand
        graphics_context_set_stroke_width(ctx, 3);
        graphics_context_set_stroke_color(ctx, enamel_get_clock_fg_color());
        graphics_draw_line(ctx, grect_center_point(&seconds_frame), sec_to);
    }
    else {
        // show date
	int month;
	for(month = 0; month < 12; month = month+1) {
	    int angle_from = month * 30;
	    int angle_to = angle_from + 20;
	    int fill;
	    GColor month_color;
	    if (month == clock_state.month) {
	        month_color = enamel_get_subdial_highlight_color();
		fill = w*.03;
	    }
	    else {
	        month_color = enamel_get_clock_fg_color();
		fill = w*.012;
	    }
	    graphics_context_set_fill_color(ctx, month_color);
	    graphics_fill_radial(ctx, seconds_frame, GOvalScaleModeFitCircle, fill,
			         DEG_TO_TRIGANGLE(angle_from), DEG_TO_TRIGANGLE(angle_to));
	}
	char s_date_string[3];
	snprintf(s_date_string, sizeof(s_date_string), "%d", clock_state.date);
        GSize text_size = graphics_text_layout_get_content_size(s_date_string, digital_font,
                                                                layer_bounds,
                                                                GTextOverflowModeFill,
                                                                GTextAlignmentCenter);
        GRect text_box = (GRect) { .size = text_size };
	grect_align(&text_box, &seconds_frame, GAlignCenter, false);
        graphics_context_set_text_color(ctx, enamel_get_clock_fg_color());
	graphics_draw_text(ctx, s_date_string, digital_font, text_box,
                           GTextOverflowModeFill, GTextAlignmentCenter, NULL);
    }

}

static void draw_day(Layer *layer, GContext *ctx) {
    GRect layer_bounds = layer_get_bounds(layer);
    int w = layer_bounds.size.w;
    int h = layer_bounds.size.h;
    GRect day_center_rect = (GRect) { .size = GSize(w*.5, h*.5) };
    grect_align(&day_center_rect, &layer_bounds, GAlignCenter, false);
    GRect day_frame = grect_centered_from_polar(day_center_rect, GOvalScaleModeFitCircle,
                                                DEG_TO_TRIGANGLE(clock_state.minute_angle+55),
                                                GSize(w*.17, h*.17));
    // day dial background
    graphics_context_set_fill_color(ctx, enamel_get_clock_bg_color());
    graphics_fill_radial(ctx, day_frame, GOvalScaleModeFitCircle, w*.09, 0, TRIG_MAX_ANGLE);
    // day dial markers
    int day;
    for (day = 0; day < 7; day = day+1 ) {
        int angle_from = day * 51;
	int angle_to = angle_from + 43;
	int fill;
	if (day > 4) {
	    fill = w*.025;
	    graphics_context_set_fill_color(ctx, enamel_get_subdial_highlight_color());
	}
	else {
	    fill = w*.012;
	    graphics_context_set_fill_color(ctx, enamel_get_clock_fg_color());
	}
	graphics_fill_radial(ctx, day_frame, GOvalScaleModeFitCircle, fill,
	                     DEG_TO_TRIGANGLE(angle_from), DEG_TO_TRIGANGLE(angle_to));
    }
    // day hand
    // end point
    GRect day_to_rect = grect_crop(day_frame, w*.03);
    GPoint day_to = gpoint_from_polar(day_to_rect, GOvalScaleModeFitCircle,
                                      DEG_TO_TRIGANGLE(clock_state.day_angle));
    // draw day hand
    graphics_context_set_stroke_width(ctx, 3);
    graphics_context_set_stroke_color(ctx, enamel_get_clock_fg_color());
    graphics_draw_line(ctx, grect_center_point(&day_frame), day_to);
}

ResHandle get_font_handle(void) {
    ResHandle resource;
    switch (PBL_PLATFORM_TYPE_CURRENT) {
        case PlatformTypeChalk:
            resource = resource_get_handle(RESOURCE_ID_LECO_9);
        break;
        default:
            resource = resource_get_handle(RESOURCE_ID_LECO_8);
    }
    return resource;
}

static void draw_marks(Layer *layer, GContext *ctx) {
    GRect layer_bounds = layer_get_bounds(layer);
    // screen background
    if (PBL_PLATFORM_TYPE_CURRENT == PlatformTypeChalk)
        graphics_context_set_fill_color(ctx, GColorBlack);
    else
        graphics_context_set_fill_color(ctx, enamel_get_screen_color());
    graphics_fill_rect(ctx, layer_bounds, 0, (GCornerMask)NULL);
    int w = layer_bounds.size.w;
    int h = layer_bounds.size.h;
    int angle_from;
    static char s_min_string[5];
    int min;
    GRect text_frame = (GRect) { .size = GSize(w*.805, h*.805) };
    grect_align(&text_frame, &layer_bounds, GAlignCenter, false);
    GRect circle_frame = (GRect) { .size = GSize(w*.98, h*.98) };
    grect_align(&circle_frame, &layer_bounds, GAlignCenter, false);
    // minute dial background
    graphics_context_set_fill_color(ctx, enamel_get_clock_bg_color());
    graphics_fill_radial(ctx, circle_frame, GOvalScaleModeFitCircle, w*.49, 0, TRIG_MAX_ANGLE);
    GRect inner_frame = (GRect) { .size = GSize(w*.9, h*.9) };
    grect_align(&inner_frame, &layer_bounds, GAlignCenter, false);
    // minute dial markers
    graphics_context_set_stroke_width(ctx, 1);
    graphics_context_set_stroke_color(ctx, enamel_get_clock_fg_color());
    graphics_context_set_text_color(ctx, enamel_get_clock_fg_color());
    for (min = 60; min > 0; min = min - 1) {
        angle_from = min * 6;
        if ((min % 5) == 0) {
	    // minute text
	    snprintf(s_min_string, sizeof(s_min_string), "%02d", min);
            GSize text_size = graphics_text_layout_get_content_size(s_min_string, digital_font,
                                                                    layer_bounds,
                                                                    GTextOverflowModeFill,
                                                                    GTextAlignmentCenter);
	    GRect text_box = grect_centered_from_polar(text_frame, GOvalScaleModeFitCircle,
                                                       DEG_TO_TRIGANGLE(angle_from), text_size);
            graphics_draw_text(ctx, s_min_string, digital_font, text_box,
                               GTextOverflowModeFill, GTextAlignmentCenter, NULL);
	}
        // minute marks
	GPoint mark_from = gpoint_from_polar(inner_frame, GOvalScaleModeFitCircle,
			                     DEG_TO_TRIGANGLE(angle_from));
	GPoint mark_to = gpoint_from_polar(circle_frame, GOvalScaleModeFitCircle,
		                          DEG_TO_TRIGANGLE(angle_from));
	graphics_draw_line(ctx, mark_from, mark_to);
    }
}

static void draw_clock(Layer *layer, GContext *ctx) {
    GRect layer_bounds = layer_get_bounds(layer);
    int w = layer_bounds.size.w;
    int h = layer_bounds.size.h;
    // minute hand
    // start point
    GRect rect_min_from = (GRect) { .size = GSize(w*.22, h*.22) };
    grect_align(&rect_min_from, &layer_bounds, GAlignCenter, false);
    GPoint min_from = gpoint_from_polar(rect_min_from, GOvalScaleModeFitCircle,
                                        DEG_TO_TRIGANGLE(clock_state.minute_angle));
    // end point
    GRect rect_min_to = (GRect) { .size = GSize(w*.8, h*.8) };
    grect_align(&rect_min_to, &layer_bounds, GAlignCenter, false);
    GPoint min_to = gpoint_from_polar(rect_min_to, GOvalScaleModeFitCircle,
                                      DEG_TO_TRIGANGLE(clock_state.minute_angle));
    // draw minute hand
    graphics_context_set_stroke_width(ctx, 3);
    graphics_context_set_stroke_color(ctx, enamel_get_minute_hand_color());
    graphics_draw_line(ctx, min_from, min_to);
    // hour dial center
    GRect rect_hour_center = (GRect) { .size = GSize(w*.27, h*.27) };
    grect_align(&rect_hour_center, &layer_bounds, GAlignCenter, false);
    GPoint hour_center = gpoint_from_polar(rect_hour_center, GOvalScaleModeFitCircle,
		                           DEG_TO_TRIGANGLE(clock_state.minute_angle + 180));
    // hour dial
    GRect hour_rect = GRect(hour_center.x - w*.18, hour_center.y - h*.18, w*.36, h*.36);
    graphics_context_set_fill_color(ctx, enamel_get_clock_bg_color());
    graphics_fill_radial(ctx, hour_rect, GOvalScaleModeFitCircle, w*.18, 0, TRIG_MAX_ANGLE);
    int hour;
    char s_hour_string[5];
    graphics_context_set_text_color(ctx, enamel_get_clock_fg_color());
    for (hour = 12; hour > 0; hour = hour-1) {
        int hour_angle = hour * 30;
        snprintf(s_hour_string, sizeof(s_hour_string), "%d", hour);
        GSize hour_size = graphics_text_layout_get_content_size(s_hour_string, digital_font,
                                                                layer_bounds,
                                                                GTextOverflowModeFill,
                                                                GTextAlignmentCenter);
        GRect hour_box = grect_centered_from_polar(hour_rect, GOvalScaleModeFitCircle,
                                                   DEG_TO_TRIGANGLE(hour_angle), hour_size);
        graphics_draw_text(ctx, s_hour_string, digital_font, hour_box,
                           GTextOverflowModeFill, GTextAlignmentCenter, NULL);
    }
    // hour hand
    GRect rect_hour_from = (GRect) { .size = GSize(hour_rect.size.w*.1,
		                                   hour_rect.size.h*.1) };
    grect_align(&rect_hour_from, &hour_rect, GAlignCenter, false);
    GPoint hour_from = gpoint_from_polar(rect_hour_from, GOvalScaleModeFitCircle,
		                         DEG_TO_TRIGANGLE(clock_state.hour_angle + 180));
    // end point
    GRect hour_hand_rect = GRect(hour_center.x - w*.13, hour_center.y - h*.13, w*.26, h*.26);
    GPoint hour_to = gpoint_from_polar(hour_hand_rect, GOvalScaleModeFitCircle,
		                       DEG_TO_TRIGANGLE(clock_state.hour_angle + clock_state.minute_angle*.08));
    // draw hour hand
    graphics_context_set_stroke_width(ctx, 3);
    graphics_context_set_stroke_color(ctx, enamel_get_hour_hand_color());
    graphics_draw_line(ctx, hour_from, hour_to);
}

static void prv_app_did_focus(bool did_focus) {
  if (!did_focus) {
    return;
  }
  app_focus_service_unsubscribe();
  watch_model_init();
  watch_model_start_intro();
}

static void window_load(Window *window) {
  Layer *const window_layer = window_get_root_layer(window);
  const GRect bounds = layer_get_bounds(window_layer);
  // minute marks layer
  marks_layer = layer_create(bounds);
  layer_set_update_proc(marks_layer, draw_marks);
  layer_add_child(window_layer, marks_layer);
  // clock layer (hour dial, minute hand)
  clock_layer = layer_create(bounds);
  layer_set_update_proc(clock_layer, draw_clock);
  layer_add_child(window_layer, clock_layer);
  // day layer
  day_layer = layer_create(bounds);
  layer_set_update_proc(day_layer, draw_day);
  layer_add_child(window_layer, day_layer);
  // seconds/day layer
  seconds_date_layer = layer_create(bounds);
  layer_set_update_proc(seconds_date_layer, draw_date_seconds);
  layer_add_child(window_layer, seconds_date_layer);
  // load font
  digital_font = fonts_load_custom_font(get_font_handle());
}

static void window_unload(Window *window) {
  fonts_unload_custom_font(digital_font);
  layer_destroy(clock_layer);
  layer_destroy(seconds_date_layer);
}

static void init(void) {
  enamel_init(0, 0);
  window = window_create();
  window_set_window_handlers(window, (WindowHandlers) {
    .load = window_load,
    .unload = window_unload,
  });
  window_stack_push(window, true /* animated */);
  app_focus_service_subscribe_handlers((AppFocusHandlers) {
    .did_focus = prv_app_did_focus,
  });
  events_app_message_open();
}

static void deinit(void) {
  enamel_deinit();
  watch_model_deinit();
  window_destroy(window);
}

int main(void) {
  init();
  app_event_loop();
  deinit();
}
