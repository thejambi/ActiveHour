#include <pebble.h>

#define SCREENSHOT_RUN false
#define DOT_DISTANCE         60
#define DOT_SPACING          6
#define EXTRA_DOT_THRESHOLD  18

typedef struct {
  int days;
  int hours;
  int minutes;
  int seconds;
} Time;

static Window *s_main_window;
static TextLayer *s_time_layer;
static Layer *s_canvas_layer;
static Time s_last_time;

static int s_dotArray[60];
static int s_lastStepTotal = 0;


static void update_time() {
  time_t temp = time(NULL); 
  struct tm *tick_time = localtime(&temp);

  static char buffer[] = "00:00";
  if(clock_is_24h_style() == true){
    strftime(buffer, sizeof("00:00"), "%k:%M", tick_time);
  }else{
    strftime(buffer, sizeof("00:00"), "%l:%M", tick_time);
  }
  text_layer_set_text(s_time_layer, buffer);
}

static int getTotalStepsToday() {
  HealthMetric metric = HealthMetricStepCount;
  time_t end = time(NULL);
  time_t start = time_start_of_today();
  
  // Check the metric has data available for today
  HealthServiceAccessibilityMask mask = health_service_metric_accessible(metric, 
    start, end);
  
  if(mask & HealthServiceAccessibilityMaskAvailable) {
    int totalSteps = (int)health_service_sum_today(metric);
    APP_LOG(APP_LOG_LEVEL_INFO, "Steps today: %d", totalSteps);
    return totalSteps;
  } else {
    return 0;
  }
}

static void tick_handler(struct tm *tick_time, TimeUnits units_changed) {
  s_last_time.days = tick_time->tm_mday;
  s_last_time.hours = tick_time->tm_hour;
  s_last_time.minutes = tick_time->tm_min;
  s_last_time.seconds = tick_time->tm_sec;

  s_last_time.hours -= (s_last_time.hours > 12) ? 12 : 0;
  
  // Save total step count
  s_lastStepTotal = getTotalStepsToday();
  
  layer_mark_dirty(s_canvas_layer);
  
  update_time();
}

/* Return number of dots to show for previous minute. Between 1 and 5. */
static int getNumDots() {
  
  int totalSteps = getTotalStepsToday();
  
  // totalSteps is a thing
  int lastSteps = totalSteps - s_lastStepTotal;
  APP_LOG(APP_LOG_LEVEL_INFO, "last minute steps today: %d", lastSteps);
  
  // Save total step count
  s_lastStepTotal = getTotalStepsToday();
  
  int dots = s_dotArray[s_last_time.minutes];
  dots++;
  if (lastSteps > EXTRA_DOT_THRESHOLD) {
    dots++;
  }
  s_dotArray[s_last_time.minutes]++;
  
  int returnDots = dots;
  if (dots < 1) {
    returnDots = 1;
  } else if (dots > 5) {
    returnDots = 5;
  }
  
  if (SCREENSHOT_RUN) {
    return rand() % 5 + 1;  // For screenshots
  } else {
    return returnDots;  // For real
  }
}

static void draw_proc(Layer *layer, GContext *ctx) {
  srand(time(NULL));  // For screenshots
  GRect bounds = layer_get_bounds(layer);
  GPoint center = grect_center_point(&bounds);
  
  int lastMin = 0;
  if (SCREENSHOT_RUN) {
    lastMin = s_last_time.seconds;  // For screenshots
  } else {
    lastMin = s_last_time.minutes;  // For real
  }

  for (int m = 0; m <= 59; m++) {
    if (m <= lastMin) {
      graphics_context_set_fill_color(ctx, GColorWhite);
    } else {
      graphics_context_set_fill_color(ctx, GColorDarkGray);
    }
    
    int v = DOT_DISTANCE;
    
    int numDots = s_dotArray[m];
    
    if (m == lastMin) {
      numDots = getNumDots();
      s_dotArray[m] = numDots;
    } else if (numDots == 0 && m <= lastMin) {
      numDots = 1;
    }

    // For each dot
    for (int i = 0; i < numDots; i++) {

      // Draw dot
      for(int y = 0; y < 1; y++) {
        for(int x = 0; x < 1; x++) {
          GPoint point = (GPoint) {
            .x = (int16_t)(sin_lookup(TRIG_MAX_ANGLE * m / 60) * (int32_t)(v) / TRIG_MAX_RATIO) + center.x,
            .y = (int16_t)(-cos_lookup(TRIG_MAX_ANGLE * m / 60) * (int32_t)(v) / TRIG_MAX_RATIO) + center.y,
          };
          graphics_fill_circle(ctx, GPoint(point.x + x, point.y + y), 1);
        }
      }
      
      // Draw next dot farther away
      v += DOT_SPACING;
    }
  }
  
}

static void health_handler(HealthEventType event, void *context) {
  // Which type of event occured?
  switch(event) {
    case HealthEventSignificantUpdate:
//       APP_LOG(APP_LOG_LEVEL_INFO, "New HealthService HealthEventSignificantUpdate event");
      break;
    case HealthEventMovementUpdate:
      APP_LOG(APP_LOG_LEVEL_INFO, "New HealthService HealthEventMovementUpdate event");
      layer_mark_dirty(s_canvas_layer);
      break;
    case HealthEventSleepUpdate:
//       APP_LOG(APP_LOG_LEVEL_INFO, "New HealthService HealthEventSleepUpdate event");
      break;
  }
}

static void main_window_load(Window *window) {
  // Get information about the Window
  Layer *window_layer = window_get_root_layer(window);
  GRect bounds = layer_get_bounds(window_layer);

  // Create the TextLayer with specific bounds
  s_time_layer = text_layer_create(
      GRect(0, PBL_IF_ROUND_ELSE(63, 57), bounds.size.w, 50));
  
  // Improve the layout to be more like a watchface
  text_layer_set_background_color(s_time_layer, GColorClear);
  text_layer_set_text_color(s_time_layer, GColorWhite);
  text_layer_set_text(s_time_layer, "00:00");
  text_layer_set_font(s_time_layer, fonts_get_system_font(FONT_KEY_BITHAM_42_LIGHT));
  text_layer_set_text_alignment(s_time_layer, GTextAlignmentCenter);

  // Add it as a child layer to the Window's root layer
  layer_add_child(window_layer, text_layer_get_layer(s_time_layer));
  
  s_canvas_layer = layer_create(bounds);
  layer_set_update_proc(s_canvas_layer, draw_proc);
  layer_add_child(window_layer, s_canvas_layer);
}

static void main_window_unload(Window *window) {
  // Destroy TextLayer
  text_layer_destroy(s_time_layer);
  
  layer_destroy(s_canvas_layer);
}


static void init() {
  // Create main Window element and assign to pointer
  s_main_window = window_create();
  window_set_background_color(s_main_window, GColorBlack);

  // Set handlers to manage the elements inside the Window
  window_set_window_handlers(s_main_window, (WindowHandlers) {
    .load = main_window_load,
    .unload = main_window_unload
  });

  // Show the Window on the watch, with animated=true
  window_stack_push(s_main_window, true);

  // Make sure the time is displayed from the start
  update_time();
  
  layer_mark_dirty(s_canvas_layer);

  // Register with TickTimerService
  if (SCREENSHOT_RUN) {
    tick_timer_service_subscribe(SECOND_UNIT, tick_handler);  // For screenshots
  } else {
    tick_timer_service_subscribe(MINUTE_UNIT, tick_handler);  // For real
  }
  
  for (int i = 0; i < 60; i++) {
    s_dotArray[i] = 0;
  }
  
  s_lastStepTotal = getTotalStepsToday();
  
  #if defined(PBL_HEALTH)
  // Attempt to subscribe 
  if(!health_service_events_subscribe(health_handler, NULL)) {
    APP_LOG(APP_LOG_LEVEL_ERROR, "Health not available!");
  }
  #else
  APP_LOG(APP_LOG_LEVEL_ERROR, "Health not available!");
  #endif
}

static void deinit() {
  // Destroy Window
  window_destroy(s_main_window);
}

int main(void) {
  init();
  app_event_loop();
  deinit();
}