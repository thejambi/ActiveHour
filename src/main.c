#include <pebble.h>

#define SCREENSHOT_RUN false

#define DOT_DISTANCE         60
#define DOT_SPACING          6
#define EXTRA_DOT_THRESHOLD  11
#define DOT_STEP_COUNT       35

typedef struct {
  int days;
  int hours;
  int minutes;
  int seconds;
} Time;

static Window *s_main_window;
static TextLayer *s_time_layer, *s_step_count_layer, *s_dayt_layer;
static Layer *s_canvas_layer;
static Time s_last_time;
static char s_step_count_buffer[5], s_dayt_buffer[12];

static int s_dotArray[60];
static int s_lastStepTotal = 0;
static int s_minuteActivityCount = 0;
static int s_lastMinSteps = 0;

static void update_time() {
  time_t temp = time(NULL); 
  struct tm *tick_time = localtime(&temp);

  // Time
  static char buffer[] = "00:00";
  if(clock_is_24h_style() == true){
    strftime(buffer, sizeof("00:00"), "%k:%M", tick_time);
  }else{
    strftime(buffer, sizeof("00:00"), "%l:%M", tick_time);
  }
  text_layer_set_text(s_time_layer, buffer);
  
  // Date
  strftime(s_dayt_buffer, sizeof(s_dayt_buffer), "%a, %b %e", tick_time);
  text_layer_set_text(s_dayt_layer, s_dayt_buffer);
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

/**
 * Return number of dots that correspond to given step count. 
 * Between 1 and 5. 
 */
static int calculateDotsFromMinuteSteps(int numSteps) {
  int dots = 1;
  
  if (numSteps > 0) {
    dots = 2 + (numSteps / DOT_STEP_COUNT);
  }
  
  if (dots > 5) {
    dots = 5;
  }
  
  return dots;
}

static void fetchPastMinuteSteps() {
  APP_LOG(APP_LOG_LEVEL_INFO, "Getting minute steps");
  
  time_t temp = time(NULL); 
  struct tm *tick_time = localtime(&temp);
  
  int currentMinute = tick_time->tm_min;
  
  // Create an array to store data
  const uint32_t max_records = 120;
  HealthMinuteData *minute_data = (HealthMinuteData*)
                                malloc(max_records * sizeof(HealthMinuteData));
  
  // Make a timestamp for an hour ago and now
  time_t end = time(NULL);
  time_t start = end - SECONDS_PER_HOUR - SECONDS_PER_HOUR;
  
  // Obtain the minute-by-minute records
  uint32_t num_records = health_service_get_minute_history(minute_data, 
                                                    max_records, &start, &end);
  APP_LOG(APP_LOG_LEVEL_INFO, "num_records: %d", (int)num_records);
  
  // Print the number of steps for each minute
  for(uint32_t i = 0; i < num_records; i++) {
    int numSteps = (int)minute_data[i].steps;
//     APP_LOG(APP_LOG_LEVEL_INFO, "Item %d steps: %d", (int)i, numSteps);
    s_dotArray[((int)i + 1 + currentMinute) % 60] = calculateDotsFromMinuteSteps(numSteps);
    APP_LOG(APP_LOG_LEVEL_INFO, "i is: %d", ((int)i + 1 + currentMinute) % 60);
  }
  
  for (int i = ((int)num_records + 1 + currentMinute) % 60; i < currentMinute; i++) {
    s_dotArray[i] = 0;
    APP_LOG(APP_LOG_LEVEL_INFO, "i is: %d\tcleanup", i % 60);
  }
  
  // Free the array
  free(minute_data);
  
  layer_mark_dirty(s_canvas_layer);
}

static void tick_handler(struct tm *tick_time, TimeUnits units_changed) {
  s_last_time.days = tick_time->tm_mday;
  s_last_time.hours = tick_time->tm_hour;
  s_last_time.minutes = tick_time->tm_min;
  s_last_time.seconds = tick_time->tm_sec;
  
  // Save total step count
  s_lastStepTotal = getTotalStepsToday();
  
  // Start next minute fresh 
  s_dotArray[s_last_time.minutes] = 0;
  s_minuteActivityCount = 0;
  s_lastMinSteps = 0;
  
  layer_mark_dirty(s_canvas_layer);
  
  update_time();
}

/* Return number of dots to show for previous minute. Between 1 and 5. */
static int getNumDots() {
  
  int totalSteps = getTotalStepsToday();
  
  int lastSteps = totalSteps - s_lastStepTotal;
  s_lastMinSteps += lastSteps;
  APP_LOG(APP_LOG_LEVEL_INFO, "last steps: %d", lastSteps);
  APP_LOG(APP_LOG_LEVEL_INFO, "steps in last minute: %d", s_lastMinSteps);
  
  // Save new total step count
  s_lastStepTotal = totalSteps;
  
  /* The old way
  int dots = s_dotArray[s_last_time.minutes];
  
  // Add dots three activities (1, 4, 7...) or if activity is extra active
  if (s_minuteActivityCount % 3 == 1 || lastSteps > EXTRA_DOT_THRESHOLD) {
    dots++;
  }*/
  
  int dots = calculateDotsFromMinuteSteps(s_lastMinSteps);
  
  int returnDots = dots;
  /* The old way
  if (dots < 1) {
    returnDots = 1;
  } else if (dots > 5) {
    returnDots = 5;
  }
  */
  
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
    
    if (m == lastMin || SCREENSHOT_RUN) {
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
  
  // Update step count text
  snprintf(s_step_count_buffer, sizeof(s_step_count_buffer), "%d", s_lastStepTotal);
  text_layer_set_text(s_step_count_layer, s_step_count_buffer);
}

static void health_handler(HealthEventType event, void *context) {
  // Which type of event occured?
  switch(event) {
    case HealthEventSignificantUpdate:
//       APP_LOG(APP_LOG_LEVEL_INFO, "New HealthService HealthEventSignificantUpdate event");
      break;
    case HealthEventMovementUpdate:
      APP_LOG(APP_LOG_LEVEL_INFO, "New HealthService HealthEventMovementUpdate event");
    
      // Increment activity count
      s_minuteActivityCount++;
    
      // Mark layer dirty so it updates
      layer_mark_dirty(s_canvas_layer);
      break;
    case HealthEventSleepUpdate:
//       APP_LOG(APP_LOG_LEVEL_INFO, "New HealthService HealthEventSleepUpdate event");
      break;
  }
}

static void main_window_load(Window *window) {
  Layer *window_layer = window_get_root_layer(window);
  GRect bounds = layer_get_bounds(window_layer);

  s_time_layer = text_layer_create(GRect(0, PBL_IF_ROUND_ELSE(61, 55), bounds.size.w, 50));  
  text_layer_set_background_color(s_time_layer, GColorClear);
  text_layer_set_text_color(s_time_layer, GColorWhite);
  text_layer_set_text(s_time_layer, "00:00");
  text_layer_set_font(s_time_layer, fonts_get_system_font(FONT_KEY_BITHAM_42_LIGHT));
  text_layer_set_text_alignment(s_time_layer, GTextAlignmentCenter);
  layer_add_child(window_layer, text_layer_get_layer(s_time_layer));
  
  s_step_count_layer = text_layer_create(GRect(0, PBL_IF_ROUND_ELSE(45, 40), bounds.size.w, 40));
  text_layer_set_text_alignment(s_step_count_layer, GTextAlignmentCenter);
  text_layer_set_font(s_step_count_layer, fonts_get_system_font(FONT_KEY_GOTHIC_18));
  text_layer_set_text_color(s_step_count_layer, GColorLightGray);
  text_layer_set_background_color(s_step_count_layer, GColorClear);
  layer_add_child(window_layer, text_layer_get_layer(s_step_count_layer));
  
  s_dayt_layer = text_layer_create(GRect(0, PBL_IF_ROUND_ELSE(106, 100), bounds.size.w, 40));
  text_layer_set_text_alignment(s_dayt_layer, GTextAlignmentCenter);
  text_layer_set_font(s_dayt_layer, fonts_get_system_font(FONT_KEY_GOTHIC_18));
  text_layer_set_text_color(s_dayt_layer, GColorLightGray);
  text_layer_set_background_color(s_dayt_layer, GColorClear);
  layer_add_child(window_layer, text_layer_get_layer(s_dayt_layer));
  
  s_canvas_layer = layer_create(bounds);
  layer_set_update_proc(s_canvas_layer, draw_proc);
  layer_add_child(window_layer, s_canvas_layer);
}

static void main_window_unload(Window *window) {
  text_layer_destroy(s_time_layer);
  text_layer_destroy(s_dayt_layer);
  text_layer_destroy(s_step_count_layer);
  
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
  
  #if defined(PBL_HEALTH)
  // Attempt to subscribe 
  if(!health_service_events_subscribe(health_handler, NULL)) {
    APP_LOG(APP_LOG_LEVEL_ERROR, "Health not available!");
  }
  #else
  APP_LOG(APP_LOG_LEVEL_ERROR, "Health not available!");
  #endif
  
  s_lastStepTotal = getTotalStepsToday();
  
  fetchPastMinuteSteps();
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