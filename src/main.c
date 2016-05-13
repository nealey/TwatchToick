#include <pebble.h>

#define MINLEN 58
#define HRCIRCLER 30
#define NRINGS 17
#define MINCIRCLER 20

#define fatness 14
#define fat(x) (fatness * x)

static Window *window;
static Layer *s_hr_layer, *s_min_layer;
static GBitmap *oi_bitmap, *io_bitmap, *tck_bitmap;
static BitmapLayer *oi_layer, *tck_layer;

GRect display_bounds;
GPoint center, mincenter;

int32_t min_angle;
char hrstr[3], minstr[3], datestr[15];
bool bt_vibe = true;
bool shape = 0;

bool bt_connected;

#define nringcolors 5
GColor *rings[nringcolors] = {
  &GColorVividCerulean,
  &GColorChromeYellow,
  &GColorCobaltBlue,
  &GColorDarkGray,
  &GColorWhite,
};

static void min_update_proc(Layer *layer, GContext *ctx) {
  //
}

static void hr_update_proc(Layer *layer, GContext *ctx) {
  //
}

static void handle_tick(struct tm *tick_time, TimeUnits units_changed) {
  if (units_changed & HOUR_UNIT) {
    if (clock_is_24h_style()) {
      strftime(hrstr, sizeof(hrstr), "%H", tick_time);
    } else {
      strftime(hrstr, sizeof(hrstr), "%I", tick_time);
    }
    layer_mark_dirty(s_hr_layer);
  }
  
  if (units_changed & SECOND_UNIT) {
    strftime(minstr, sizeof(minstr), "%M", tick_time);
    if (tick_time->tm_sec % 2 == 0) {
      bitmap_layer_set_bitmap(oi_layer, oi_bitmap);
    } else {
      bitmap_layer_set_bitmap(oi_layer, io_bitmap);
    }
  
    layer_mark_dirty(s_min_layer);
  }
}

static void window_load(Window *window) {
  Layer *window_layer = window_get_root_layer(window);
  
  display_bounds = layer_get_bounds(window_layer);
  center = grect_center_point(&display_bounds);
  
  // Minutes
  s_min_layer = layer_create(display_bounds);
  layer_set_update_proc(s_min_layer, min_update_proc);
  
  // oioioioi
  oi_bitmap = gbitmap_create_with_resource(RESOURCE_ID_IMAGE_OI);
  io_bitmap = gbitmap_create_with_resource(RESOURCE_ID_IMAGE_IO);
  GRect bm_bounds = gbitmap_get_bounds(oi_bitmap);
  GRect min_bounds = {
    .origin = {
      .x = (display_bounds.size.w - bm_bounds.size.w) / 2,
      .y = 40,
    },
    .size = {
      .w = bm_bounds.size.w,
      .h = bm_bounds.size.h,
    }
  };
  oi_layer = bitmap_layer_create(min_bounds);
  bitmap_layer_set_compositing_mode(oi_layer, GCompOpSet);
  layer_add_child(s_min_layer, bitmap_layer_get_layer(oi_layer));
  
  // t_ck
  tck_bitmap = gbitmap_create_with_resource(RESOURCE_ID_IMAGE_TCK);
  GRect tck_bounds = gbitmap_get_bounds(tck_bitmap);
  tck_bounds.origin.x = center.x - (tck_bounds.size.w / 2);
  tck_bounds.origin.y = 5;
  tck_layer = bitmap_layer_create(tck_bounds);
  bitmap_layer_set_compositing_mode(tck_layer, GCompOpSet);
  bitmap_layer_set_bitmap(tck_layer, tck_bitmap);
  layer_add_child(s_min_layer, bitmap_layer_get_layer(tck_layer));
  
  // Hours
  s_hr_layer = layer_create(display_bounds);
  layer_set_update_proc(s_hr_layer, hr_update_proc);
  
  layer_add_child(window_layer, s_min_layer);
  layer_add_child(window_layer, s_hr_layer);
}

static void window_unload(Window *window) {
  layer_destroy(s_hr_layer);
  layer_destroy(s_min_layer);
  bitmap_layer_destroy(oi_layer);
}

static void bt_handler(bool connected) {
  bt_connected = connected;
  if (bt_vibe && (! connected)) {
    vibes_double_pulse();
  }
  layer_mark_dirty(s_min_layer);
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
  bt_connected = bluetooth_connection_service_peek();
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
