#include <pebble.h>

#define SCREENSHOT_RUN false

#define DOT_DISTANCE         60
#define DOT_SPACING          6
#define EXTRA_DOT_THRESHOLD  11
#define DOT_STEP_COUNT       30

#define BOLD_TIME   false

// Persist
#define PERSIST_DEFAULTS_SET 228483

#define PERSIST_KEY_DATE        0
#define PERSIST_KEY_STEPS       1
#define PERSIST_KEY_CLR_BW      2
#define PERSIST_KEY_CLR_ORANGE  3
#define PERSIST_KEY_CLR_GREEN   4
#define PERSIST_KEY_CLR_BLUE    5
#define NUM_SETTINGS            6


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
static char s_step_count_buffer[7], s_dayt_buffer[12];

static int s_dotArray[60];
static int s_lastStepTotal = 0;
static int s_lastMinSteps = 0;

static bool s_loadedWithMissingData = true;

/* Config */
static bool s_arr[NUM_SETTINGS];

bool config_get(int key) {
  if (SCREENSHOT_RUN) {
    if (key == PERSIST_KEY_CLR_BW) {
      return true;
    } else if (key == PERSIST_KEY_CLR_BLUE) {
      return false;
    } else if (key == PERSIST_KEY_CLR_GREEN) {
      return false;
    } else if (key == PERSIST_KEY_CLR_ORANGE) {
      return false;
    } else {
      return s_arr[key];
    }
  } else {
    return s_arr[key];  // For real
  }
}

void config_init() {
  // Set defaults
  if(!persist_exists(PERSIST_DEFAULTS_SET)) {
    persist_write_bool(PERSIST_DEFAULTS_SET, true);

    persist_write_bool(PERSIST_KEY_DATE, false);
    persist_write_bool(PERSIST_KEY_STEPS, false);
    persist_write_bool(PERSIST_KEY_CLR_BW, true);
    persist_write_bool(PERSIST_KEY_CLR_ORANGE, false);
    persist_write_bool(PERSIST_KEY_CLR_GREEN, false);
    persist_write_bool(PERSIST_KEY_CLR_BLUE, false);
  }

  for(int i = 0; i < NUM_SETTINGS; i++) {
    s_arr[i] = persist_read_bool(i);
  }
  
  APP_LOG(APP_LOG_LEVEL_DEBUG, "DATE : %d", config_get(PERSIST_KEY_DATE));
  APP_LOG(APP_LOG_LEVEL_DEBUG, "STEPS: %d", config_get(PERSIST_KEY_STEPS));
}

static GColor8 getTimeColor() {
  if (config_get(PERSIST_KEY_CLR_ORANGE)) {
    return GColorOrange;
  } else if (config_get(PERSIST_KEY_CLR_GREEN)) {
    return GColorChromeYellow;
  } else if (config_get(PERSIST_KEY_CLR_BLUE)) {
    return GColorWhite;
  }
  // BW
  return GColorWhite;
}

static GColor8 getDotMainColor() {
  if (config_get(PERSIST_KEY_CLR_ORANGE)) {
    return GColorOrange;
  } else if (config_get(PERSIST_KEY_CLR_GREEN)) {
    return GColorGreen;
  } else if (config_get(PERSIST_KEY_CLR_BLUE)) {
    return GColorCyan;
  }
  // BW
  return GColorWhite;
}

static GColor8 getDotDarkColor() {
  if (config_get(PERSIST_KEY_CLR_ORANGE)) {
    return GColorDarkGray;
  } else if (config_get(PERSIST_KEY_CLR_GREEN)) {
    return GColorDarkGreen;
  } else if (config_get(PERSIST_KEY_CLR_BLUE)) {
    return GColorBlueMoon;
  }
  // BW
  return GColorDarkGray;
}

static GColor8 getStepCountColor() {
  if (config_get(PERSIST_KEY_CLR_ORANGE)) {
    return GColorRajah;
  } else if (config_get(PERSIST_KEY_CLR_GREEN)) {
    return GColorGreen;
  } else if (config_get(PERSIST_KEY_CLR_BLUE)) {
    return GColorPictonBlue;
  }
  // BW
  return GColorLightGray;
}

static GColor8 getDateColor() {
  if (config_get(PERSIST_KEY_CLR_ORANGE)) {
    return GColorRajah;
  } else if (config_get(PERSIST_KEY_CLR_GREEN)) {
    return GColorGreen;
  } else if (config_get(PERSIST_KEY_CLR_BLUE)) {
    return GColorPictonBlue;
  }
  // BW
  return GColorLightGray;
}

static void clearDate() {
  snprintf(s_dayt_buffer, sizeof(s_dayt_buffer), "            ");
  text_layer_set_text(s_dayt_layer, s_dayt_buffer);
}

static void clearSteps() {
  snprintf(s_step_count_buffer, sizeof(s_step_count_buffer), "       ");
  text_layer_set_text(s_step_count_layer, s_step_count_buffer);
}

static int getSleepSeconds() {
  HealthMetric metric = HealthMetricSleepSeconds;
  time_t start = time_start_of_today();
  time_t end = time(NULL);
  
  // Check the metric has data available for today
  HealthServiceAccessibilityMask mask = health_service_metric_accessible(metric, 
    start, end);
  
  if(mask & HealthServiceAccessibilityMaskAvailable) {
    // Data is available!
    int sleeps = (int)health_service_sum_today(metric);
    APP_LOG(APP_LOG_LEVEL_INFO, "Sleep seconds data: %d", sleeps);
    return sleeps;
  } else {
    // No data recorded yet today
    APP_LOG(APP_LOG_LEVEL_ERROR, "Data unavailable!");
    return 0;
  }
}

static void updateStepsLabel() {
  if (config_get(PERSIST_KEY_STEPS)) {
    if (s_lastStepTotal > 500) {
      // Update step count text
      snprintf(s_step_count_buffer, sizeof(s_step_count_buffer), "%d", s_lastStepTotal);
      text_layer_set_text(s_step_count_layer, s_step_count_buffer);
    } else {
      // Show sleep time
      int secs = getSleepSeconds();
      int hrs = secs / 3600;
      int mins = (secs - (hrs*3600)) / 60;
      snprintf(s_step_count_buffer, sizeof(s_step_count_buffer), "%dh %dm", hrs, mins);
      text_layer_set_text(s_step_count_layer, s_step_count_buffer);
    }
  }
}

static void update_time() {
  time_t temp = time(NULL); 
  struct tm *tick_time = localtime(&temp);

  // Time
  static char buffer[] = "00:00";
  if(clock_is_24h_style() == true){
    strftime(buffer, sizeof("00:00"), "%k:%M", tick_time);
  }else{
    strftime(buffer, sizeof("00:00"), "%l:%M", tick_time);
    if (SCREENSHOT_RUN) {
      strftime(buffer, sizeof("00:00"), "%l:%S", tick_time);
    }
  }
  text_layer_set_text(s_time_layer, buffer);
  
  // Date
  if (config_get(PERSIST_KEY_DATE)) {
    strftime(s_dayt_buffer, sizeof(s_dayt_buffer), "%a, %b %e", tick_time);
    text_layer_set_text(s_dayt_layer, s_dayt_buffer);
  } else {
    APP_LOG(APP_LOG_LEVEL_DEBUG, "APPARENTLY NOT DATE: %d", config_get(PERSIST_KEY_DATE));
  }
}

static void setLayerTextColors() {
  text_layer_set_text_color(s_time_layer, getTimeColor());
  text_layer_set_text_color(s_step_count_layer, getStepCountColor());
  text_layer_set_text_color(s_dayt_layer, getDateColor());
}

static void in_recv_handler(DictionaryIterator *iter, void *context) {
    Tuple *t = dict_read_first(iter);
    while(t) {
      persist_write_bool(t->key, strcmp(t->value->cstring, "true") == 0 ? true : false);
      t = dict_read_next(iter);
    }
  
    // Refresh live store
    config_init();
    vibes_short_pulse();
  
  // Update display based on new config data
  if (!config_get(PERSIST_KEY_DATE)) {
    clearDate();
  } else {
    update_time();
  }
  if (!config_get(PERSIST_KEY_STEPS)) {
    clearSteps();
  } else {
    updateStepsLabel();
  }
  setLayerTextColors();
  layer_mark_dirty(s_canvas_layer);
}

static void inbox_dropped_callback(AppMessageResult reason, void *context) {
  APP_LOG(APP_LOG_LEVEL_ERROR, "Message dropped!");
}

static void outbox_failed_callback(DictionaryIterator *iterator, AppMessageResult reason, void *context) {
  APP_LOG(APP_LOG_LEVEL_ERROR, "Outbox send failed!");
}

static void outbox_sent_callback(DictionaryIterator *iterator, void *context) {
  APP_LOG(APP_LOG_LEVEL_INFO, "Outbox send success!");
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
  time_t temp = time(NULL); 
  struct tm *tick_time = localtime(&temp);
  
  int currentMinute = tick_time->tm_min;
  
  // Create an array to store data
  const uint32_t max_records = 60;//120;
  HealthMinuteData *minute_data = (HealthMinuteData*)
                                malloc(max_records * sizeof(HealthMinuteData));
  
  // Make a timestamp for an hour ago and now
  time_t end = time(NULL);
  time_t start = end - SECONDS_PER_HOUR;// - SECONDS_PER_HOUR;
  
  // Obtain the minute-by-minute records
  uint32_t num_records = health_service_get_minute_history(minute_data, 
                                                    max_records, &start, &end);
  APP_LOG(APP_LOG_LEVEL_INFO, "num_records: %d", (int)num_records);
  
  // Print the number of steps for each minute
  for(uint32_t i = 0; i < num_records; i++) {
    int numSteps = (int)minute_data[i].steps;
    s_dotArray[((int)i + 1 + currentMinute) % 60] = calculateDotsFromMinuteSteps(numSteps);
  }
  
  for (int i = ((int)num_records + 1 + currentMinute) % 60; i < currentMinute; i++) {
    s_dotArray[i] = 1;
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
  s_dotArray[s_last_time.minutes] = 1;
  s_lastMinSteps = 0;
  
  // If face was loaded with missing data and we can get that now, let's do it
  if (s_loadedWithMissingData && tick_time->tm_min % 15 == 1) {
    fetchPastMinuteSteps();
    s_loadedWithMissingData = false;
    APP_LOG(APP_LOG_LEVEL_DEBUG, "Loaded missing data, never doing it again!");
  }
  
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
  
  updateStepsLabel();
  
  int dots = calculateDotsFromMinuteSteps(s_lastMinSteps);
  
  int returnDots = dots;
  
  if (SCREENSHOT_RUN) {
    int randy = rand() % 5 + 1;
    if (randy != 1) {
      randy = rand() % 5 + 1;
    }
    if (randy == 5){
      randy = rand() % 5 + 1;
    }
    return randy;  // For screenshots
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
      graphics_context_set_fill_color(ctx, getDotMainColor());
    } else {
      graphics_context_set_fill_color(ctx, getDotDarkColor());
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
}

static void health_handler(HealthEventType event, void *context) {
  // Which type of event occured?
  switch(event) {
    case HealthEventSignificantUpdate:
//       APP_LOG(APP_LOG_LEVEL_INFO, "New HealthService HealthEventSignificantUpdate event");
      break;
    case HealthEventMovementUpdate:
      APP_LOG(APP_LOG_LEVEL_INFO, "New HealthService HealthEventMovementUpdate event");
    
      // Mark layer dirty so it updates
      layer_mark_dirty(s_canvas_layer);
      break;
    case HealthEventSleepUpdate:
//       APP_LOG(APP_LOG_LEVEL_INFO, "New HealthService HealthEventSleepUpdate event");
      break;
  }
}

void comm_init() {
  app_message_register_inbox_received(in_recv_handler);
  
  // Register callbacks
  app_message_register_inbox_dropped(inbox_dropped_callback);
  app_message_register_outbox_failed(outbox_failed_callback);
  app_message_register_outbox_sent(outbox_sent_callback);
  
  app_message_open(app_message_inbox_size_maximum(), app_message_outbox_size_maximum());
}

static void main_window_load(Window *window) {
  Layer *window_layer = window_get_root_layer(window);
  GRect bounds = layer_get_bounds(window_layer);

  s_time_layer = text_layer_create(GRect(0, PBL_IF_ROUND_ELSE(61, 55), bounds.size.w, 50));  
  text_layer_set_background_color(s_time_layer, GColorClear);
  text_layer_set_text(s_time_layer, "00:00");
  if (BOLD_TIME) {
    text_layer_set_font(s_time_layer, fonts_get_system_font(FONT_KEY_BITHAM_42_BOLD));
  } else {
    text_layer_set_font(s_time_layer, fonts_get_system_font(FONT_KEY_BITHAM_42_LIGHT));
  }
  text_layer_set_text_alignment(s_time_layer, GTextAlignmentCenter);
  layer_add_child(window_layer, text_layer_get_layer(s_time_layer));
  
  s_step_count_layer = text_layer_create(GRect(0, PBL_IF_ROUND_ELSE(45, 40), bounds.size.w, 40));
  text_layer_set_text_alignment(s_step_count_layer, GTextAlignmentCenter);
  text_layer_set_font(s_step_count_layer, fonts_get_system_font(FONT_KEY_GOTHIC_18));
  text_layer_set_background_color(s_step_count_layer, GColorClear);
  layer_add_child(window_layer, text_layer_get_layer(s_step_count_layer));
  
  s_dayt_layer = text_layer_create(GRect(0, PBL_IF_ROUND_ELSE(106, 100), bounds.size.w, 40));
  text_layer_set_text_alignment(s_dayt_layer, GTextAlignmentCenter);
  text_layer_set_font(s_dayt_layer, fonts_get_system_font(FONT_KEY_GOTHIC_18));
  text_layer_set_background_color(s_dayt_layer, GColorClear);
  layer_add_child(window_layer, text_layer_get_layer(s_dayt_layer));
  
  setLayerTextColors();
  
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
  comm_init();
  config_init();
  
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
  
  // Loaded with missing data?
  time_t temp = time(NULL); 
  struct tm *tick_time = localtime(&temp);
  
  if (tick_time->tm_min % 15 == 1) {
    s_loadedWithMissingData = false;
  } else {
    s_loadedWithMissingData = true;
  }
  
  layer_mark_dirty(s_canvas_layer);

  // Register with TickTimerService
//   if (SCREENSHOT_RUN) {
//     tick_timer_service_subscribe(SECOND_UNIT, tick_handler);  // For screenshots
//   } else {
    tick_timer_service_subscribe(MINUTE_UNIT, tick_handler);  // For real
//   }
  
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