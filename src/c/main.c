#include <pebble.h>

#define SCREENSHOT_RUN false

// Ring radius scales with the display so the ring sits at the same relative
// position on every platform (basalt/diorite/flint 144x168, chalk 180x180,
// emery 200x228, gabbro 260x260 round).
#if defined(PBL_PLATFORM_EMERY)
  #define DOT_DISTANCE       82
#elif defined(PBL_PLATFORM_GABBRO)
  #define DOT_DISTANCE       87
#else
  #define DOT_DISTANCE       60
#endif
#define DOT_SPACING          6
#define EXTRA_DOT_THRESHOLD  11
#define DOT_STEP_COUNT       30
#define DOT_SIZE_DEFAULT     1
#define DOT_SIZE_BOLD        2

// Vertical positions of the centered text layers, for the platforms that use
// the 42px system font. The time text is centered at ~0.46 of screen height
// (above center, to leave room for the date below) with absolute gaps of
// step->time ~15px and time->date 45px. Gabbro must come before the generic
// PBL_ROUND check since it's also a round display. Flint (144x168, like
// diorite) falls through to the rectangular default.
//
// emery and gabbro no longer use these: they size the time font to the space
// inside the ring instead, so their positions are computed in getTextLayout()
// from the chosen font height.
#if defined(PBL_PLATFORM_EMERY)
  #define TIME_Y  83
  #define STEP_Y  68
  #define DATE_Y  128
#elif defined(PBL_PLATFORM_GABBRO)
  #define TIME_Y  98
  #define STEP_Y  82
  #define DATE_Y  143
#elif defined(PBL_ROUND)
  #define TIME_Y  61
  #define STEP_Y  45
  #define DATE_Y  106
#else
  #define TIME_Y  55
  #define STEP_Y  40
  #define DATE_Y  100
#endif

// Persist
#define PERSIST_DEFAULTS_SET 228483

#define PERSIST_KEY_DATE        0
#define PERSIST_KEY_STEPS       1
#define PERSIST_KEY_CLR_BW      2
#define PERSIST_KEY_CLR_ORANGE  3
#define PERSIST_KEY_CLR_GREEN   4
#define PERSIST_KEY_CLR_BLUE    5
#define PERSIST_KEY_WEATHER     6
#define PERSIST_KEY_BOLD_TEXT   7
#define PERSIST_KEY_BOLD_DOTS   8
#define PERSIST_KEY_CLR_PURPLE  9
#define PERSIST_KEY_CLR_RED     10
#define PERSIST_KEY_CLR_TEAL    11
// Keys 12 (DAILY), 13 (INVERTED), 14 (BT) retired — left unused.
#define PERSIST_KEY_MINMARKS    15
#define PERSIST_KEY_CLR_CUSTOM  16   // bool: custom theme on
#define PERSIST_KEY_FITDOTS     17   // bool: emery-only, pull ring in
// Custom theme colors: packed 0xRRGGBB ints, read separately from the bool cache.
#define PERSIST_KEY_CUSTOM_BG         18
#define PERSIST_KEY_CUSTOM_TIME       19
#define PERSIST_KEY_CUSTOM_DOT_ACTIVE 20
#define PERSIST_KEY_CUSTOM_DOT_DIM    21
#define PERSIST_KEY_CUSTOM_STEPS      22
#define PERSIST_KEY_CUSTOM_DATE       23
#define PERSIST_KEY_BATTERY     24   // bool: thin out dots as the battery drains
// Clock font choice. Radio-style, matching how the color presets work: at most
// one is true, and all-false means the Bitham system font.
#define PERSIST_KEY_FONT_ROBOTO 25   // bool: Roboto time font
#define PERSIST_KEY_FONT_MONT   26   // bool: Montserrat time font
// s_arr spans keys 0..26. Keys 12-14 are retired and 18-23 hold the color ints,
// so those slots are dead weight in the bool cache — never read via config_get().
#define NUM_SETTINGS            27

// Battery indication: dot i (0-based, outward) stays bold only while the charge
// is at or above (i+1)*10 percent — under 50% the 5th dot thins, under 40% the
// 4th follows, and so on down to a fully thin ring under 10%.
#define BATTERY_STEP_PER_DOT    10

// Defaults for the custom theme (a legible dark scheme).
#define CUSTOM_BG_DEFAULT         0x000000
#define CUSTOM_TIME_DEFAULT       0xFFFFFF
#define CUSTOM_DOT_ACTIVE_DEFAULT 0xFF6A00
#define CUSTOM_DOT_DIM_DEFAULT    0x555555
#define CUSTOM_STEPS_DEFAULT      0xAAAAAA
#define CUSTOM_DATE_DEFAULT       0xAAAAAA

#define KEY_TEMPERATURE 101
#define KEY_JSREADY     102


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

static bool hasWeather = false;
static int weatherTemp;

static int s_dotSize = DOT_SIZE_DEFAULT;

// Current charge percent, kept fresh by battery_handler().
static int s_batteryLevel = 100;

static bool s_arr[NUM_SETTINGS];

// Custom theme colors (packed 0xRRGGBB), loaded in config_init().
static int s_customBg     = CUSTOM_BG_DEFAULT;
static int s_customTime   = CUSTOM_TIME_DEFAULT;
static int s_customActive = CUSTOM_DOT_ACTIVE_DEFAULT;
static int s_customDim    = CUSTOM_DOT_DIM_DEFAULT;
static int s_customSteps  = CUSTOM_STEPS_DEFAULT;
static int s_customDate   = CUSTOM_DATE_DEFAULT;

/* Config */

// Convert a packed 0xRRGGBB value to the nearest Pebble color (auto-quantizes
// to the 64-color palette, and to black/white on B&W watches like diorite).
static GColor8 hexToGColor(int hex) {
  return GColorFromRGB((hex >> 16) & 0xFF, (hex >> 8) & 0xFF, hex & 0xFF);
}

// Read a persisted custom color, falling back to its default if unset.
static int readCustomColor(int key, int fallback) {
  return persist_exists(key) ? persist_read_int(key) : fallback;
}

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

    // Fresh installs get the fully-featured look: Orange theme, bold text and
    // dots, hour marks, battery indication, date and steps all on. Weather is
    // the deliberate exception — it would prompt for location before the user
    // has asked for anything.
    persist_write_bool(PERSIST_KEY_DATE, true);
    persist_write_bool(PERSIST_KEY_STEPS, true);
    persist_write_bool(PERSIST_KEY_CLR_BW, false);
    persist_write_bool(PERSIST_KEY_CLR_ORANGE, true);
    persist_write_bool(PERSIST_KEY_CLR_GREEN, false);
    persist_write_bool(PERSIST_KEY_CLR_BLUE, false);
    persist_write_bool(PERSIST_KEY_WEATHER, false);
    persist_write_bool(PERSIST_KEY_BOLD_TEXT, true);
    persist_write_bool(PERSIST_KEY_BOLD_DOTS, true);
    persist_write_bool(PERSIST_KEY_MINMARKS, true);
    persist_write_bool(PERSIST_KEY_FITDOTS, true);
    persist_write_bool(PERSIST_KEY_BATTERY, true);
    // Montserrat is the closest of the bundled faces to the classic Bitham
    // look, so it's the default; Roboto stays a preset away.
    persist_write_bool(PERSIST_KEY_FONT_ROBOTO, false);
    persist_write_bool(PERSIST_KEY_FONT_MONT, true);
  }

  for(int i = 0; i < NUM_SETTINGS; i++) {
    s_arr[i] = persist_read_bool(i);
  }

  s_customBg     = readCustomColor(PERSIST_KEY_CUSTOM_BG,         CUSTOM_BG_DEFAULT);
  s_customTime   = readCustomColor(PERSIST_KEY_CUSTOM_TIME,       CUSTOM_TIME_DEFAULT);
  s_customActive = readCustomColor(PERSIST_KEY_CUSTOM_DOT_ACTIVE, CUSTOM_DOT_ACTIVE_DEFAULT);
  s_customDim    = readCustomColor(PERSIST_KEY_CUSTOM_DOT_DIM,    CUSTOM_DOT_DIM_DEFAULT);
  s_customSteps  = readCustomColor(PERSIST_KEY_CUSTOM_STEPS,      CUSTOM_STEPS_DEFAULT);
  s_customDate   = readCustomColor(PERSIST_KEY_CUSTOM_DATE,       CUSTOM_DATE_DEFAULT);

  if (config_get(PERSIST_KEY_BOLD_DOTS)) {
    s_dotSize = DOT_SIZE_BOLD;
  } else {
    s_dotSize = DOT_SIZE_DEFAULT;
  }
}

static GColor8 getBackgroundColor() {
  if (config_get(PERSIST_KEY_CLR_CUSTOM)) {
    return hexToGColor(s_customBg);
  }
  return GColorBlack;
}

static GColor8 getTimeColor() {
  if (config_get(PERSIST_KEY_CLR_CUSTOM)) {
    return hexToGColor(s_customTime);
  } else if (config_get(PERSIST_KEY_CLR_ORANGE)) {
    return GColorOrange;
  } else if (config_get(PERSIST_KEY_CLR_GREEN)) {
    return GColorChromeYellow;
  } else if (config_get(PERSIST_KEY_CLR_BLUE)) {
    return GColorWhite;
  } else if (config_get(PERSIST_KEY_CLR_PURPLE)) {
    return GColorVividViolet;
  } else if (config_get(PERSIST_KEY_CLR_RED)) {
    return GColorRed;
  } else if (config_get(PERSIST_KEY_CLR_TEAL)) {
    return GColorCyan;
  }
  // BW
  return GColorWhite;
}

static GColor8 getDotMainColor() {
  if (config_get(PERSIST_KEY_CLR_CUSTOM)) {
    return hexToGColor(s_customActive);
  } else if (config_get(PERSIST_KEY_CLR_ORANGE)) {
    return GColorOrange;
  } else if (config_get(PERSIST_KEY_CLR_GREEN)) {
    return GColorGreen;
  } else if (config_get(PERSIST_KEY_CLR_BLUE)) {
    return GColorCyan;
  } else if (config_get(PERSIST_KEY_CLR_PURPLE)) {
    return GColorVividViolet;
  } else if (config_get(PERSIST_KEY_CLR_RED)) {
    return GColorRed;
  } else if (config_get(PERSIST_KEY_CLR_TEAL)) {
    return GColorTiffanyBlue;
  }
  // BW
  return GColorWhite;
}

static GColor8 getDotDarkColor() {
  if (config_get(PERSIST_KEY_CLR_CUSTOM)) {
    return hexToGColor(s_customDim);
  } else if (config_get(PERSIST_KEY_CLR_ORANGE)) {
    return GColorDarkGray;
  } else if (config_get(PERSIST_KEY_CLR_GREEN)) {
    return GColorDarkGreen;
  } else if (config_get(PERSIST_KEY_CLR_BLUE)) {
    return GColorBlueMoon;
  } else if (config_get(PERSIST_KEY_CLR_PURPLE)) {
    return GColorImperialPurple;
  } else if (config_get(PERSIST_KEY_CLR_RED)) {
    return GColorDarkCandyAppleRed;
  } else if (config_get(PERSIST_KEY_CLR_TEAL)) {
    return GColorMidnightGreen;
  }
  // BW
  return GColorDarkGray;
}

static GColor8 getStepCountColor() {
  if (config_get(PERSIST_KEY_CLR_CUSTOM)) {
    return hexToGColor(s_customSteps);
  } else if (config_get(PERSIST_KEY_CLR_ORANGE)) {
    return GColorRajah;
  } else if (config_get(PERSIST_KEY_CLR_GREEN)) {
    return GColorGreen;
  } else if (config_get(PERSIST_KEY_CLR_BLUE)) {
    return GColorPictonBlue;
  } else if (config_get(PERSIST_KEY_CLR_PURPLE)) {
    return GColorBabyBlueEyes;
  } else if (config_get(PERSIST_KEY_CLR_RED)) {
    return GColorMelon;
  } else if (config_get(PERSIST_KEY_CLR_TEAL)) {
    return GColorMediumAquamarine;
  }
  // BW
  return GColorLightGray;
}

static GColor8 getDateColor() {
  if (config_get(PERSIST_KEY_CLR_CUSTOM)) {
    return hexToGColor(s_customDate);
  } else if (config_get(PERSIST_KEY_CLR_ORANGE)) {
    return GColorRajah;
  } else if (config_get(PERSIST_KEY_CLR_GREEN)) {
    return GColorGreen;
  } else if (config_get(PERSIST_KEY_CLR_BLUE)) {
    return GColorPictonBlue;
  } else if (config_get(PERSIST_KEY_CLR_PURPLE)) {
    return GColorBabyBlueEyes;
  } else if (config_get(PERSIST_KEY_CLR_RED)) {
    return GColorMelon;
  } else if (config_get(PERSIST_KEY_CLR_TEAL)) {
    return GColorMediumAquamarine;
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
  }
}

static void setLayerTextColors() {
  window_set_background_color(s_main_window, getBackgroundColor());
  text_layer_set_text_color(s_time_layer, getTimeColor());
  text_layer_set_text_color(s_step_count_layer, getStepCountColor());
  text_layer_set_text_color(s_dayt_layer, getDateColor());
}

// Which face draws the clock. Bitham is a system font and only exists at 42px;
// the other two are bundled resources, sized to the room inside the ring.
typedef enum {
  CLOCK_FONT_BITHAM = 0,
  CLOCK_FONT_ROBOTO,
  CLOCK_FONT_MONT
} ClockFont;

static ClockFont getClockFont() {
  if (config_get(PERSIST_KEY_FONT_MONT)) {
    return CLOCK_FONT_MONT;
  }
  if (config_get(PERSIST_KEY_FONT_ROBOTO)) {
    return CLOCK_FONT_ROBOTO;
  }
  return CLOCK_FONT_BITHAM;
}

// Where the three text layers sit, and how tall the time font is. Bitham always
// uses the compile-time positions; the bundled faces are sized per layout, so
// on emery and gabbro they get larger text and their own positions.
typedef struct {
  int timeY;
  int stepY;
  int dateY;
  int timeH;
} TextLayout;

static TextLayout getTextLayout() {
  TextLayout l;
  ClockFont font = getClockFont();

  if (font == CLOCK_FONT_BITHAM) {
    // 42px on every platform, original layout.
    l.timeH = 42; l.timeY = TIME_Y; l.stepY = STEP_Y; l.dateY = DATE_Y;
    return l;
  }

  // Montserrat's round digits are noticeably wider than Roboto's at the same
  // size (167 vs 148 at 58px), so it lands a size or two smaller everywhere.
#if defined(PBL_PLATFORM_EMERY)
  if (config_get(PERSIST_KEY_FITDOTS)) {
    // Ring pulled in to DOT_DISTANCE 70 — less room inside it.
    if (font == CLOCK_FONT_MONT) {
      l.timeH = 42; l.timeY = 84; l.stepY = 69; l.dateY = 129;
    } else {
      l.timeH = 48; l.timeY = 81; l.stepY = 66; l.dateY = 132;
    }
  } else {
    // Ring left at DOT_DISTANCE 82 — the edge dots crop, but the wider gap
    // inside the ring buys a much larger time.
    if (font == CLOCK_FONT_MONT) {
      l.timeH = 52; l.timeY = 77; l.stepY = 62; l.dateY = 132;
    } else {
      l.timeH = 58; l.timeY = 74; l.stepY = 61; l.dateY = 137;
    }
  }
#elif defined(PBL_PLATFORM_GABBRO)
  if (font == CLOCK_FONT_MONT) {
    l.timeH = 54; l.timeY = 93; l.stepY = 78; l.dateY = 150;
  } else {
    l.timeH = 60; l.timeY = 90; l.stepY = 75; l.dateY = 153;
  }
#else
  // 144x168 and chalk have no headroom: Roboto 42 measures 108 against a 108
  // budget, so 40 is the largest that clears the ring (Montserrat 36).
  l.timeH = (font == CLOCK_FONT_MONT) ? 36 : 40;
  l.timeY = TIME_Y; l.stepY = STEP_Y; l.dateY = DATE_Y;
#endif
  return l;
}

static void applyTextLayout() {
  GRect bounds = layer_get_bounds(window_get_root_layer(s_main_window));
  TextLayout l = getTextLayout();

  layer_set_frame(text_layer_get_layer(s_time_layer),
                  GRect(0, l.timeY, bounds.size.w, l.timeH + 8));
  layer_set_frame(text_layer_get_layer(s_step_count_layer),
                  GRect(0, l.stepY, bounds.size.w, 40));
  layer_set_frame(text_layer_get_layer(s_dayt_layer),
                  GRect(0, l.dateY, bounds.size.w, 40));
}

// Bundled face currently loaded, or NULL when the time is using system Bitham.
// Held so it can be released when the weight, layout or font choice changes.
static GFont s_timeFont = NULL;

static uint32_t timeFontResource() {
  bool bold = config_get(PERSIST_KEY_BOLD_TEXT);
  bool mont = (getClockFont() == CLOCK_FONT_MONT);
#if defined(PBL_PLATFORM_EMERY)
  if (config_get(PERSIST_KEY_FITDOTS)) {
    if (mont) {
      return bold ? RESOURCE_ID_FONT_MONT_B_42 : RESOURCE_ID_FONT_MONT_L_42;
    }
    return bold ? RESOURCE_ID_FONT_TIME_B_48 : RESOURCE_ID_FONT_TIME_L_48;
  }
  if (mont) {
    return bold ? RESOURCE_ID_FONT_MONT_B_52 : RESOURCE_ID_FONT_MONT_L_52;
  }
  return bold ? RESOURCE_ID_FONT_TIME_B_58 : RESOURCE_ID_FONT_TIME_L_58;
#elif defined(PBL_PLATFORM_GABBRO)
  if (mont) {
    return bold ? RESOURCE_ID_FONT_MONT_B_54 : RESOURCE_ID_FONT_MONT_L_54;
  }
  return bold ? RESOURCE_ID_FONT_TIME_B_60 : RESOURCE_ID_FONT_TIME_L_60;
#else
  if (mont) {
    return bold ? RESOURCE_ID_FONT_MONT_B_36 : RESOURCE_ID_FONT_MONT_L_36;
  }
  return bold ? RESOURCE_ID_FONT_TIME_B_40 : RESOURCE_ID_FONT_TIME_L_40;
#endif
}

static void setLayerFonts() {
  bool bold = config_get(PERSIST_KEY_BOLD_TEXT);
  GFont textFont = fonts_get_system_font(bold ? FONT_KEY_GOTHIC_18_BOLD
                                              : FONT_KEY_GOTHIC_18);
  text_layer_set_font(s_step_count_layer, textFont);
  text_layer_set_font(s_dayt_layer, textFont);

  // In both branches the layer is pointed at the new font before the old one
  // is released, so it never references freed memory.
  GFont previous = s_timeFont;
  if (getClockFont() != CLOCK_FONT_BITHAM) {
    s_timeFont = fonts_load_custom_font(resource_get_handle(timeFontResource()));
    text_layer_set_font(s_time_layer, s_timeFont);
  } else {
    s_timeFont = NULL;
    text_layer_set_font(s_time_layer, fonts_get_system_font(bold ? FONT_KEY_BITHAM_42_BOLD
                                                                 : FONT_KEY_BITHAM_42_LIGHT));
  }
  if (previous != NULL) {
    fonts_unload_custom_font(previous);
  }

  applyTextLayout();
}

void send_initial_js_message() {
  if (config_get(PERSIST_KEY_WEATHER)) {
    // Begin dictionary
    DictionaryIterator *iter;
    app_message_outbox_begin(&iter);
  
    // Add a key-value pair
    int val = config_get(PERSIST_KEY_WEATHER) ? 1 : 0;
    dict_write_uint8(iter, PERSIST_KEY_WEATHER, val);
  
    // Send the message!
    app_message_outbox_send();
  }
}

static void in_recv_handler(DictionaryIterator *iter, void *context) {
  // Read tuple for data
  Tuple *temp_tuple = dict_find(iter, KEY_TEMPERATURE);
  Tuple *jsr_tuple = dict_find(iter, KEY_JSREADY);

  if (temp_tuple) {
    weatherTemp = (int)temp_tuple->value->int32;
    hasWeather = true;
    layer_mark_dirty(s_canvas_layer);
  } else if (jsr_tuple) {
    // Send weather pref to js
    send_initial_js_message();
  } else {
    Tuple *t = dict_read_first(iter);
    while(t) {
      if (t->key >= PERSIST_KEY_CUSTOM_BG && t->key <= PERSIST_KEY_CUSTOM_DATE) {
        // Custom theme colors arrive as packed 0xRRGGBB integers, not bool strings
        persist_write_int(t->key, (int)t->value->int32);
      } else {
        persist_write_bool(t->key, strcmp(t->value->cstring, "true") == 0 ? true : false);
      }
      t = dict_read_next(iter);
    }
  
    // Refresh live store
    config_init();
    vibes_short_pulse();
  }
  
  
  /* Update display based on new config data */
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
  
  send_initial_js_message();
  
  setLayerTextColors();
  setLayerFonts();
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
  
  // Print the number of steps for each minute
  for(uint32_t i = 0; i < num_records; i++) {
    int idx = ((int)i + 1 + currentMinute) % 60;
    if (minute_data[i].is_invalid) {
      // Watch was off / not worn this minute: .steps is undefined garbage,
      // so show the baseline dot only — no phantom activity.
      s_dotArray[idx] = 1;
    } else {
      s_dotArray[idx] = calculateDotsFromMinuteSteps((int)minute_data[i].steps);
    }
  }
  
  for (int i = ((int)num_records + 1 + currentMinute) % 60; i < currentMinute; i++) {
    s_dotArray[i] = 1;
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
  }
  
  layer_mark_dirty(s_canvas_layer);
  
  update_time();
  
  if (config_get(PERSIST_KEY_WEATHER)) {
    // Get weather update every 30 minutes
    if(tick_time->tm_min % 30 == 0) {
      // Begin dictionary
      DictionaryIterator *iter;
      app_message_outbox_begin(&iter);

      // Ask JS to refresh the weather. Use PERSIST_KEY_WEATHER so app.js
      // recognizes this as a weather-refresh signal (key 0 was ignored).
      dict_write_uint8(iter, PERSIST_KEY_WEATHER, 1);

      // Send the message!
      app_message_outbox_send();

      APP_LOG(APP_LOG_LEVEL_DEBUG, "Sent message to get weather...");
    }
  }
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

  // Base ring radius. On emery, the "Fit dots" option pulls the ring in so a
  // full 5-dot minute clears the screen edge instead of being clipped.
  int baseDist = DOT_DISTANCE;
#if defined(PBL_PLATFORM_EMERY)
  if (config_get(PERSIST_KEY_FITDOTS)) {
    int halfMin = (bounds.size.w < bounds.size.h ? bounds.size.w : bounds.size.h) / 2;
    int stackReach = 4 * DOT_SPACING + s_dotSize;  // outer edge beyond baseDist
    int fit = halfMin - stackReach - 4;            // 4px breathing margin
    if (fit > 0 && fit < baseDist) {
      baseDist = fit;
    }
  }
#endif

  for (int m = 0; m <= 59; m++) {
    GColor8 dotColor = (m <= lastMin) ? getDotMainColor() : getDotDarkColor();
    graphics_context_set_fill_color(ctx, dotColor);

    // Hour marks: with bold dots on, the base dot at each clock-hour position
    // (every 5 minutes = the 12 ticks) is drawn a size smaller than the bold dots.
    bool hourMark = config_get(PERSIST_KEY_BOLD_DOTS)
                 && config_get(PERSIST_KEY_MINMARKS)
                 && (m % 5 == 0);

    // Like hour marks, battery indication works by drawing a dot a size
    // smaller, so it only has a visible effect while bold dots are on.
    bool batteryInd = config_get(PERSIST_KEY_BOLD_DOTS)
                   && config_get(PERSIST_KEY_BATTERY);

    int v = baseDist;

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
          int radius = s_dotSize;
          if (hourMark && i == 0) {
            radius = s_dotSize - 1;
          }
          if (batteryInd && s_batteryLevel < (i + 1) * BATTERY_STEP_PER_DOT) {
            radius = s_dotSize - 1;
          }
          graphics_fill_circle(ctx, GPoint(point.x + x, point.y + y), radius);
        }
      }
      
      // Draw next dot farther away
      v += DOT_SPACING;
    }
  }
  
  if (config_get(PERSIST_KEY_WEATHER) && hasWeather) {
    // Get weather "minute"
    int m = weatherTemp % 60;
    
    // Get weather dot color
    if (weatherTemp < 0) {
      graphics_context_set_fill_color(ctx, GColorCeleste);
    } else if (weatherTemp < 60) {
      graphics_context_set_fill_color(ctx, GColorMediumAquamarine);
    } else {
      graphics_context_set_fill_color(ctx, GColorOrange);
    }
    
    // Draw dot (just inside the ring, tracking the fit-adjusted radius)
    int v = baseDist - DOT_SPACING - 1;
    for(int y = 0; y < 1; y++) {
      for(int x = 0; x < 1; x++) {
        GPoint point = (GPoint) {
          .x = (int16_t)(sin_lookup(TRIG_MAX_ANGLE * m / 60) * (int32_t)(v) / TRIG_MAX_RATIO) + center.x,
          .y = (int16_t)(-cos_lookup(TRIG_MAX_ANGLE * m / 60) * (int32_t)(v) / TRIG_MAX_RATIO) + center.y,
        };
        graphics_fill_circle(ctx, GPoint(point.x + x, point.y + y), s_dotSize);
      }
    }
  }
}

static void battery_handler(BatteryChargeState state) {
  s_batteryLevel = state.charge_percent;
  layer_mark_dirty(s_canvas_layer);
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
    case HealthEventMetricAlert:
      // Not used by this watchface
      break;
    case HealthEventHeartRateUpdate:
      // Not used by this watchface
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

  s_time_layer = text_layer_create(GRect(0, TIME_Y, bounds.size.w, 50));
  text_layer_set_background_color(s_time_layer, GColorClear);
  text_layer_set_text(s_time_layer, "00:00");
  text_layer_set_text_alignment(s_time_layer, GTextAlignmentCenter);
  layer_add_child(window_layer, text_layer_get_layer(s_time_layer));

  s_step_count_layer = text_layer_create(GRect(0, STEP_Y, bounds.size.w, 40));
  text_layer_set_text_alignment(s_step_count_layer, GTextAlignmentCenter);
  text_layer_set_background_color(s_step_count_layer, GColorClear);
  layer_add_child(window_layer, text_layer_get_layer(s_step_count_layer));

  s_dayt_layer = text_layer_create(GRect(0, DATE_Y, bounds.size.w, 40));
  text_layer_set_text_alignment(s_dayt_layer, GTextAlignmentCenter);
  text_layer_set_background_color(s_dayt_layer, GColorClear);
  layer_add_child(window_layer, text_layer_get_layer(s_dayt_layer));
  
  setLayerTextColors();
  setLayerFonts();
  
  s_canvas_layer = layer_create(bounds);
  layer_set_update_proc(s_canvas_layer, draw_proc);
  layer_add_child(window_layer, s_canvas_layer);

}

static void main_window_unload(Window *window) {
  text_layer_destroy(s_time_layer);
  text_layer_destroy(s_dayt_layer);
  text_layer_destroy(s_step_count_layer);

  if (s_timeFont != NULL) {
    fonts_unload_custom_font(s_timeFont);
    s_timeFont = NULL;
  }

  layer_destroy(s_canvas_layer);
}


static void init() {
  comm_init();
  config_init();
  
  // Create main Window element and assign to pointer
  s_main_window = window_create();
  window_set_background_color(s_main_window, getBackgroundColor());

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
  tick_timer_service_subscribe(MINUTE_UNIT, tick_handler);  // For real
  
  for (int i = 0; i < 60; i++) {
    s_dotArray[i] = 0;
  }
  
  #if defined(PBL_HEALTH)
  if(!health_service_events_subscribe(health_handler, NULL)) {
    APP_LOG(APP_LOG_LEVEL_ERROR, "Health not available!");
  }
  #else
  APP_LOG(APP_LOG_LEVEL_ERROR, "Health not available!");
  #endif
  
  s_lastStepTotal = getTotalStepsToday();

  // Seed the charge level before subscribing so the first draw is accurate.
  // Safe here: the window is already pushed, so s_canvas_layer exists by now.
  s_batteryLevel = battery_state_service_peek().charge_percent;
  battery_state_service_subscribe(battery_handler);

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