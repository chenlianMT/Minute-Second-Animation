#include "pebble.h"

static int temp_int = 1;
static int CURRENT_TIME_HOUR = -1;
static int CURRENT_TIME_MINUTE = -1;
static int CURRENT_TIME_SECOND = -1;// when number window is toggled, every number is set to 0

#define BAR_DENSITY 0.25
#define ACCEL_RATIO 0.007
#define ACCEL_STEP_MS 50
static int DIM_RANGE[12] = {7, 7, 7, 7, 7, 7, 7, 7, 8, 9, 9, 7};

typedef struct Vec2d {
  double  x;
  double  y;
} Vec2d;

typedef struct Vec4d {
  double  top;
  double  down;
  double  left;
  double  right;
} Vec4d;

typedef struct hour_bar {
  Vec2d pos; // left-top corner
  Vec2d vel; // velocity
  double  mass;
  double  dim; // dimension of the bar
  Vec4d outer_square; // the outer lines to detect collision
} hour_bar;

static hour_bar hour_bars[12];

static Window *window;

static InverterLayer *s_inverterlayers[12];
static TextLayer *s_background_layer;

static AppTimer *timer;

void ftoa(char* str, double val, int precision) {
  //  start with positive/negative
  if (val < 0) {
    *(str++) = '-';
    val = -val;
  }
  //  integer value
  snprintf(str, 12, "%d", (int) val);
  str += strlen(str);
  val -= (int) val;
  //  decimals
  if ((precision > 0) && (val >= .00001)) {
    //  add period
    *(str++) = '.';
    //  loop through precision
    for (int i = 0;  i < precision;  i++)
      if (val > 0) {
        val *= 10;
        *(str++) = '0' + (int) (val + ((i == precision - 1) ? .5 : 0));
        val -= (int) val;
      } else
        break;
  }
  //  terminate
  *str = '\0';
}

static double absolut(double input){
    if (input < 0){
        input = -input;
    }
    return input;
}

static void get_current_time() {
    // Get a tm structure
    time_t temp = time(NULL); 
    struct tm *tick_time = localtime(&temp);
    struct tm current_time = *tick_time;

    CURRENT_TIME_MINUTE = current_time.tm_min;
    CURRENT_TIME_SECOND = current_time.tm_sec;
    CURRENT_TIME_HOUR = current_time.tm_hour;
    
    // change hour range to 1-12 type
    if(clock_is_24h_style() == true) {
        //Use 24 hour format
        CURRENT_TIME_HOUR = CURRENT_TIME_HOUR % 12;
    }
    if (CURRENT_TIME_HOUR == 0){
        CURRENT_TIME_HOUR = 12;
    }
}

static int update_collision(hour_bar bar1, hour_bar bar2){
    int result = 0;
    //double decay_coe = 3;
    if ((bar2.outer_square.top <= bar1.outer_square.down) && (bar1.outer_square.top <= bar2.outer_square.down) && 
            (bar2.outer_square.left <= bar1.outer_square.left) && (bar1.outer_square.left <= bar2.outer_square.right)) {
//         bar1.vel.x = -bar1.vel.x * decay_coe;
//         bar1.vel.y = -bar1.vel.y * decay_coe;
        result += 1; // left-side inside
    }
    if ((bar2.outer_square.top <= bar1.outer_square.down) && (bar1.outer_square.top <= bar2.outer_square.down) && 
            (bar2.outer_square.left <= bar1.outer_square.right) && (bar1.outer_square.right <= bar2.outer_square.right)) {
//         bar1.vel.x = -bar1.vel.x * decay_coe;
//         bar1.vel.y = -bar1.vel.y * decay_coe;
        result += 10; // right-side inside
    }
    if ((bar2.outer_square.top <= bar1.outer_square.down) && (bar1.outer_square.down <= bar2.outer_square.down) && 
            (bar2.outer_square.left <= bar1.outer_square.right) && (bar1.outer_square.left <= bar2.outer_square.right)) {
//         bar1.vel.x = -bar1.vel.x * decay_coe;
//         bar1.vel.y = -bar1.vel.y * decay_coe;
        result += 100; // down-side inside
    }
    if ((bar2.outer_square.top <= bar1.outer_square.top) && (bar1.outer_square.top <= bar2.outer_square.down) && 
            (bar2.outer_square.left <= bar1.outer_square.right) && (bar1.outer_square.left <= bar2.outer_square.right)){
//         bar1.vel.x = -bar1.vel.x * decay_coe;
//         bar1.vel.y = -bar1.vel.y * decay_coe;
        result += 1000; // top-side inside
    }
    return result;
}

static double bar_calc_mass(hour_bar bar) {
  return bar.dim * bar.dim * BAR_DENSITY;
}

static void bars_init() {
    hour_bars[0].pos.x = 1;
    hour_bars[0].pos.y = 1;
    hour_bars[0].dim = rand() % DIM_RANGE[0] + 10;
    hour_bars[0].outer_square.top = hour_bars[0].pos.y;
    hour_bars[0].outer_square.down = hour_bars[0].outer_square.top + hour_bars[0].dim + 1;
    hour_bars[0].outer_square.left = hour_bars[0].pos.x;
    hour_bars[0].outer_square.right = hour_bars[0].outer_square.left + hour_bars[0].dim + 1;
    hour_bars[0].vel.x = 1;
    hour_bars[0].vel.y = 1;
    hour_bars[0].mass = bar_calc_mass(hour_bars[0]);
    
    for(int i=1;i<12;i++){
        hour_bars[i].pos.x = hour_bars[i-1].outer_square.right+1;
        hour_bars[i].pos.y = 1;
        hour_bars[i].dim = rand() % DIM_RANGE[i] + 8;
        hour_bars[i].outer_square.top = hour_bars[i].pos.y;
        hour_bars[i].outer_square.down = hour_bars[i].outer_square.top + hour_bars[i].dim + 1;
        hour_bars[i].outer_square.left = hour_bars[i].pos.x;
        hour_bars[i].outer_square.right = hour_bars[i].outer_square.left + hour_bars[i].dim + 1;
        hour_bars[i].vel.x = 1;
        hour_bars[i].vel.y = 1;
        hour_bars[i].mass = bar_calc_mass(hour_bars[i]);
    }
}

static void bars_apply_force(Vec2d force) {
    for (int i=0;i<CURRENT_TIME_HOUR;i++){
        hour_bars[i].vel.x += force.x/hour_bars[i].mass;
        hour_bars[i].vel.y += force.y/hour_bars[i].mass;
    }
}

static void bars_apply_accel(AccelData accel) {
  Vec2d force;
  force.x = accel.x * ACCEL_RATIO;
  force.y = -accel.y * ACCEL_RATIO;
  bars_apply_force(force);
}

static void bars_update() {
    double decay_coe = 0.8;
    for (int i=0;i<CURRENT_TIME_HOUR;i++){
        bool collide_on_walls = false;
        bool collide_on_ceil_or_floor = false;

        // Detect collision and update velocity
        if ((hour_bars[i].outer_square.left < 0 && hour_bars[i].vel.x < 0)
            || (hour_bars[i].outer_square.right > 144 && hour_bars[i].vel.x > 0)) {
            hour_bars[i].vel.x = -hour_bars[i].vel.x * decay_coe;
            collide_on_walls = true;
        }
        if ((hour_bars[i].outer_square.top < 0 && hour_bars[i].vel.y < 0)
            || (hour_bars[i].outer_square.down > 168 && hour_bars[i].vel.y > 0)) {
            hour_bars[i].vel.y = -hour_bars[i].vel.y * decay_coe;
            collide_on_ceil_or_floor = true;
        }

        int collision_of_all_bars = 0;
        for (int j=0;j<CURRENT_TIME_HOUR;j++){
            // collision parameter:
            // 0: no collision
            // 1: left side of bar1
            // 10: right side of bar1
            // 100: down side of bar1
            // 1000: top side of bar1
            if (i != j){
                int collision_para = update_collision(hour_bars[i], hour_bars[j]);
                collision_of_all_bars += collision_para;
                if (collision_para > 0){
                    if (collision_para % 10 == 1){
                        hour_bars[i].vel.x = absolut(hour_bars[i].vel.x) * 1.01;
                    } if (collision_para % 100 > 1){
                        hour_bars[i].vel.x = -absolut(hour_bars[i].vel.x) * 1.01;
                    } if (collision_para % 1000 > 11){
                        hour_bars[i].vel.y = -absolut(hour_bars[i].vel.y) * 1.01;
                    } if (collision_para % 10000 > 111){
                        hour_bars[i].vel.y = absolut(hour_bars[i].vel.y) * 1.01;
                    }
                    
                    if (collide_on_walls){
                        hour_bars[i].vel.x = 0;
                    }
                    if (collide_on_ceil_or_floor){
                        hour_bars[i].vel.y = 0;
                    }
                }
            }
        } if (collision_of_all_bars == 0){
            hour_bars[i].vel.x *= 0.999999;
            hour_bars[i].vel.y *= 0.999999;
        }
        if (i == 0){
        char* stringForSelectedHour = "";
        ftoa(stringForSelectedHour, collision_of_all_bars, 3);
        text_layer_set_text(s_background_layer, stringForSelectedHour);}
        
        // Update all parameters basing on new location
        hour_bars[i].pos.x += hour_bars[i].vel.x;
        hour_bars[i].pos.y += hour_bars[i].vel.y;
        hour_bars[i].outer_square.top = hour_bars[i].pos.y;
        hour_bars[i].outer_square.down = hour_bars[i].outer_square.top + hour_bars[i].dim + 1;
        hour_bars[i].outer_square.left = hour_bars[i].pos.x;
        hour_bars[i].outer_square.right = hour_bars[i].outer_square.left + hour_bars[i].dim + 1;
    }
}

static void inverterlayers_draw() {
    for (int i=0;i<12;i++){
        inverter_layer_destroy(s_inverterlayers[i]);
        s_inverterlayers[i] = inverter_layer_create(GRect(hour_bars[i].outer_square.left, hour_bars[i].outer_square.top, hour_bars[i].dim, hour_bars[i].dim));
    }
    
    for (int i=0;i<CURRENT_TIME_HOUR;i++){
        layer_add_child(window_get_root_layer(window), inverter_layer_get_layer(s_inverterlayers[i]));
    }
}

static void timer_callback(void *data) {
    AccelData accel = (AccelData) { .x = 0, .y = 0, .z = 0 };

    accel_service_peek(&accel);

    bars_apply_accel(accel);

    bars_update();
    inverterlayers_draw();
    
    timer = app_timer_register(ACCEL_STEP_MS, timer_callback, NULL);
}

static void window_load(Window *window) {
    Layer *window_layer = window_get_root_layer(window);
    GRect frame = layer_get_bounds(window_layer);
    s_background_layer = text_layer_create(frame);
    text_layer_set_background_color(s_background_layer, GColorWhite);
    text_layer_set_text_color(s_background_layer, GColorBlack);
    text_layer_set_text(s_background_layer, "9999");
    text_layer_set_font(s_background_layer, fonts_get_system_font(FONT_KEY_BITHAM_42_BOLD));
    text_layer_set_text_alignment(s_background_layer, GTextAlignmentCenter);
    layer_add_child(window_layer, text_layer_get_layer(s_background_layer));
    
    // set global time
    get_current_time();
    bars_init();

    // Mass with inverter layers
    for (int i=0;i<12;i++){
        s_inverterlayers[i] = inverter_layer_create(GRect(hour_bars[i].outer_square.left, hour_bars[i].outer_square.top, hour_bars[i].dim, hour_bars[i].dim));
    }
 
    //CURRENT_TIME_HOUR = 2;
    for (int i=0;i<CURRENT_TIME_HOUR;i++){
        layer_add_child(window_get_root_layer(window), inverter_layer_get_layer(s_inverterlayers[i]));
    }
    
    timer = app_timer_register(ACCEL_STEP_MS, timer_callback, NULL);
}

static void window_unload(Window *window) {
    text_layer_destroy(s_background_layer);
    
    for (int i=0;i<12;i++){
        inverter_layer_destroy(s_inverterlayers[i]);
    }
    
    app_timer_cancel(timer);
}

static void init(void) {
  window = window_create();
  window_set_window_handlers(window, (WindowHandlers) {
    .load = window_load,
    .unload = window_unload
  });
    
    // Set Full Screen
    window_set_fullscreen(window, true);
    
  window_stack_push(window, true /* Animated */);
  window_set_background_color(window, GColorBlack);
    
    

  accel_data_service_subscribe(0, NULL);

  //timer = app_timer_register(ACCEL_STEP_MS, timer_callback, NULL);
}

static void deinit(void) {
  accel_data_service_unsubscribe();

  window_destroy(window);
}

int main(void) {
  init();
  app_event_loop();
  deinit();
}
