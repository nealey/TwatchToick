#include <pebble.h>

#define HAND_OUT 50
#define HAND_IN 27
#define MINUTE_WIDTH 8
#define HOUR_WIDTH 12
#define BORDER_WIDTH PBL_IF_ROUND_ELSE(16, 12)
#define HBW (BORDER_WIDTH / 2)


static Window *window;
static Layer *s_hr_layer, *s_min_layer, *s_sec_layer;
static GBitmap *oi_bitmap, *io_bitmap, *tck_bitmap;
static BitmapLayer *oi_layer, *tck_layer;
static TextLayer *s_bt_label;

GRect display_bounds;
GPoint center, mincenter;

int32_t min_angle;
bool bt_vibe = true;
int hour, min;

bool bt_connected;


static GPoint point_of_polar(int32_t theta, int r) {
  GPoint ret = {
    .x = (int16_t)(sin_lookup(theta) * r / TRIG_MAX_RATIO) + mincenter.x,
    .y = (int16_t)(-cos_lookup(theta) * r / TRIG_MAX_RATIO) + mincenter.y,
  };
  
  return ret;
}

static void min_update_proc(Layer *layer, GContext *ctx) {
  graphics_context_set_stroke_width(ctx, MINUTE_WIDTH);
  graphics_context_set_stroke_color(ctx, COLOR_FALLBACK(GColorLightGray, GColorWhite));
  graphics_draw_line(ctx,
                     point_of_polar(0, 0),
                     point_of_polar(TRIG_MAX_ANGLE * min / 60, HAND_OUT));
}

static void hr_update_proc(Layer *layer, GContext *ctx) {
  graphics_context_set_stroke_width(ctx, HOUR_WIDTH);
  graphics_context_set_stroke_color(ctx, COLOR_FALLBACK(GColorLightGray, GColorWhite));
  graphics_draw_line(ctx,
                     point_of_polar(TRIG_MAX_ANGLE * (hour % 12) / 12, HAND_IN),
                     point_of_polar(TRIG_MAX_ANGLE * (hour % 12) / 12, HAND_OUT));
}

static void handle_tick(struct tm *tick_time, TimeUnits units_changed) {
  hour = tick_time->tm_hour;
  min = tick_time->tm_min;
  
  if (units_changed & HOUR_UNIT) {
    layer_mark_dirty(s_hr_layer);
  }
  
  if (units_changed & MINUTE_UNIT) {
    layer_mark_dirty(s_min_layer);
  }
  
  if (units_changed & SECOND_UNIT) {
    if (tick_time->tm_sec % 2 == 0) {
      bitmap_layer_set_bitmap(oi_layer, oi_bitmap);
    } else {
      bitmap_layer_set_bitmap(oi_layer, io_bitmap);
    }
  }
}

static void window_load(Window *window) {
  Layer *window_layer = window_get_root_layer(window);
  
  display_bounds = layer_get_bounds(window_layer);
  center = grect_center_point(&display_bounds);
  
  // Seconds
  s_sec_layer = layer_create(display_bounds);
  
  // oioioioi
  oi_bitmap = gbitmap_create_with_resource(RESOURCE_ID_IMAGE_OI);
  io_bitmap = gbitmap_create_with_resource(RESOURCE_ID_IMAGE_IO);
  GRect bm_bounds = gbitmap_get_bounds(oi_bitmap);
  GRect oi_bounds = {
    .origin = {
      .x = (display_bounds.size.w - bm_bounds.size.w) / 2,
      .y = 40,
    },
    .size = {
      .w = bm_bounds.size.w,
      .h = bm_bounds.size.h,
    }
  };
  oi_layer = bitmap_layer_create(oi_bounds);
  bitmap_layer_set_compositing_mode(oi_layer, GCompOpSet);
  layer_add_child(s_sec_layer, bitmap_layer_get_layer(oi_layer));
  
  // t_ck
  tck_bitmap = gbitmap_create_with_resource(RESOURCE_ID_IMAGE_TCK);
  GRect tck_bounds = gbitmap_get_bounds(tck_bitmap);
  tck_bounds.origin.x = center.x - (tck_bounds.size.w / 2);
  tck_bounds.origin.y = 5;
  tck_layer = bitmap_layer_create(tck_bounds);
  bitmap_layer_set_compositing_mode(tck_layer, GCompOpSet);
  bitmap_layer_set_bitmap(tck_layer, tck_bitmap);
  layer_add_child(s_sec_layer, bitmap_layer_get_layer(tck_layer));
  
  // Minutes
  s_min_layer = layer_create(oi_bounds);
  layer_set_update_proc(s_min_layer, min_update_proc);
  {
    GRect b = layer_get_bounds(s_min_layer);
    mincenter = grect_center_point(&b);
  }
  
  // Hours
  s_hr_layer = layer_create(oi_bounds);
  layer_set_update_proc(s_hr_layer, hr_update_proc);
  
  // Missing phone
#ifdef PBL_ROUND
  s_bt_label = text_layer_create(GRect(26, 5, 52, 64));
#else
  s_bt_label = text_layer_create(GRect(0, -5, 52, 64));
#endif
  text_layer_set_text_alignment(s_bt_label, GTextAlignmentCenter);
  text_layer_set_text(s_bt_label, "");
  text_layer_set_background_color(s_bt_label, GColorClear);
  text_layer_set_text_color(s_bt_label, COLOR_FALLBACK(GColorRajah, GColorBlack));
  text_layer_set_font(s_bt_label, fonts_load_custom_font(resource_get_handle(RESOURCE_ID_SYMBOLS_50)));

  layer_add_child(window_layer, s_min_layer);
  layer_add_child(window_layer, s_hr_layer);
  layer_add_child(window_layer, s_sec_layer);
  layer_add_child(window_layer, text_layer_get_layer(s_bt_label));
}

static void window_unload(Window *window) {
  layer_destroy(s_hr_layer);
  layer_destroy(s_min_layer);
  layer_destroy(s_sec_layer);
  bitmap_layer_destroy(oi_layer);
}

static void bt_handler(bool connected) {
  bt_connected = connected;
  if (bt_vibe && (! connected)) {
    vibes_double_pulse();
  }

  if (bt_connected) {
    text_layer_set_text(s_bt_label, "");
  } else {
    text_layer_set_text(s_bt_label, "");
  }
}

static void init() {  
  window = window_create();
  window_set_window_handlers(window, (WindowHandlers) {
    .load = window_load,
    .unload = window_unload,
  });
  window_stack_push(window, true);
  
  tick_timer_service_subscribe(HOUR_UNIT | MINUTE_UNIT | SECOND_UNIT, handle_tick);
  {
    time_t now = time(NULL);
    struct tm *tick_time = localtime(&now);
    handle_tick(tick_time, HOUR_UNIT | MINUTE_UNIT | SECOND_UNIT);
  }
  
  bluetooth_connection_service_subscribe(bt_handler);
  bt_handler(bluetooth_connection_service_peek());
}

static void deinit() {
  tick_timer_service_unsubscribe();
  window_destroy(window);
}

int main() {
  init();
  app_event_loop();
  deinit();
}
