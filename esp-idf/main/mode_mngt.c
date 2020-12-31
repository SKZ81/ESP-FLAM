#include <stdbool.h>
#include <math.h>
#include <string.h>
#include <esp_log.h>
#include <esp_system.h>
#include <esp_timer.h>


#include "color_spaces.h"
#include "mode_mngt.h"
#include "pwm.h"


mode_cfg_t mode_config;

static const char *TAG = "ModeMngt";


static inline bool checkmode(const char *caller_mode_name,
                             modeid_t expected_mode,
                             const char* expected_mode_name) {
    if (mode_config.mode != expected_mode) {
        ESP_LOGE(TAG, "In %s but mode (%d) != %s (%d)", caller_mode_name,
                 mode_config.mode, expected_mode_name, mode_config.mode);
        ESP_LOGI(TAG, "Cowardly return without doing nothing...");
        return false;
    }
    return true;
}


static inline bool checkmodes(const char *function_name,
                              modeid_t accepted_modes[],
                              const char *accepted_modes_names[], // NULL => ignore
                              uint8_t accepted_modes_size) {
    uint8_t index = 0;
    while (index < accepted_modes_size && accepted_modes[index] != mode_config.mode) {
        index++;
    }
    if (index == accepted_modes_size) {
        ESP_LOGE(TAG, "In cb_mode_blink but mode (%d)", mode_config.mode);
        ESP_LOGE(TAG, "Not in accepted_modes list :");
        for(uint8_t i=0; i<accepted_modes_size; i++) {
            if (accepted_modes_names) {
                ESP_LOGE(TAG, " - %s (%d)", accepted_modes_names[i], accepted_modes[i]);
            } else {
                ESP_LOGE(TAG, " - %d", accepted_modes[i]);
            }
        }
        ESP_LOGI(TAG, "Cowardly return without doing nothing...");
        return false;
    }
    return true;
}




typedef enum e_bootanim_step {
    BOOTANIM_INIT=0,
    DIMMUP,
    FADE,
    ROTATION,
    DIMMDOWN
} bootanim_step_t;

typedef struct s_cb_bootanim_data {
    bootanim_step_t step;
    HSV_color_t led1_hsv;
    HSV_color_t led2_hsv;
    HSV_color_t led3_hsv;
    uint64_t ref_time;
} cb_bootanim_data_t;

static const float dimmup_duration = 5*1000000.0;   // µsecond
static const float fade_duration = 2*1000000.0;     // µsecond
static const float rotation_speed = (1/3.0)*1000000.0;   // Hz
static const float rotation_duration = 10*1000000.0;   // µsecond
static const float dimmdown_duration = 5*1000000.0; // µsecond

float cie1931(float L) {
    L = L*100.0;
    if (L <= 8) {
        return (L/903.3);
    }
    float x = ((L+16.0)/119.0);
    return x*x*x;
}

float dimm(float value) {
    return 0.7 * cie1931(value);
}

float fade(float value) {
    return expf(value/2)-1.0;
}

#define TAG_REF_TIME() (data->ref_time = esp_timer_get_time())

void cb_mode_bootanim(void* arg) {
    static uint8_t accepted_modes[] = { BOOTANIM };

    cb_bootanim_data_t *data = (cb_bootanim_data_t*)arg;
    uint64_t current_time = esp_timer_get_time(); // µs

    if ( ! checkmodes(accepted_modes, sizeof(accepted_modes))) return;

    switch(data->step) {
        case BOOTANIM_INIT:
            init_hsv(&data->led1_hsv, 0.0/3.0, 0.0, 0.0);
            init_hsv(&data->led2_hsv, 1.0/3.0, 0.0, 0.0);
            init_hsv(&data->led3_hsv, 2.0/3.0, 0.0, 0.0);
            data->step = DIMMUP;
            TAG_REF_TIME();
        break;
        case DIMMUP:
        {
            float value = dimm((current_time - data->ref_time)/dimmup_duration);
            data->led1_hsv.v = value;
            data->led2_hsv.v = value;
            data->led3_hsv.v = value;

            if (current_time > data->ref_time + dimmup_duration) {
                TAG_REF_TIME();
                data->step = FADE;
            }
        }
        break;
        case FADE:
        {
            float saturation = fade((current_time - data->ref_time)/fade_duration);
            data->led1_hsv.s = saturation;
            data->led2_hsv.s = saturation;
            data->led3_hsv.s = saturation;

            if (current_time > data->ref_time + fade_duration) {
                TAG_REF_TIME();
                data->step = ROTATION;
            }
        }
        break;
        case ROTATION:
        {
            float angle = rotation_speed*(current_time - data->ref_time)/1000000.0;
            data->led1_hsv.s = angle;
            data->led2_hsv.s = angle+1.0/3.0;
            data->led3_hsv.s = angle+2.0/3.0;

            if (current_time > data->ref_time + rotation_duration) {
                TAG_REF_TIME();
                data->step = DIMMDOWN;
            }
        }
        break;
        case DIMMDOWN:
        {
            float value = dimm(1 - (current_time - data->ref_time)/dimmup_duration);
            float angle = rotation_speed*(current_time - data->ref_time)/1000000.0;
            data->led1_hsv.s = angle;
            data->led1_hsv.v = value;
            data->led2_hsv.s = angle+1.0/3.0;
            data->led2_hsv.v = value;
            data->led3_hsv.s = angle+2.0/3.0;
            data->led3_hsv.v = value;

            if (current_time > data->ref_time + dimmdown_duration) {
                TAG_REF_TIME();
                data->step = DIMMDOWN;
            }
        }
        break;
        default:
            ESP_LOGE(TAG, "BOOTANIM : Unknown step kind %d. Expected: BOOTANIM_INIT, DIMMUP, FADE, ROTATION, DIMMDOWN.", data->step);
    }

    RGB_color_t rgb;
    hsv2rgb(&(data->led1_hsv), &rgb);
    espflam_set_led_RGB(1, rgb.r, rgb.g, rgb.b);
    hsv2rgb(&(data->led2_hsv), &rgb);
    espflam_set_led_RGB(2, rgb.r, rgb.g, rgb.b);
    hsv2rgb(&(data->led3_hsv), &rgb);
    espflam_set_led_RGB(3, rgb.r, rgb.g, rgb.b);
}






// To be used for errors, connection...
typedef enum e_blink_step {
    OFF=0,
    ON,
    PAUSE
} blink_step_t;

typedef struct s_cb_blink_data {
    uint64_t next_timestamp;
    blink_step_t next_step;
    uint8_t blink_count;
} cb_blink_data_t;


void cb_mode_blink(void* arg) {
    cb_blink_data_t *data = (cb_blink_data_t*)arg;
    const mode_blink_cfg_t *conf = &(mode_config.config.blink);
    uint64_t current_time = esp_timer_get_time(); // µs

    static modeid_t accepted_modes[] = { ERROR_WIFI,
                                         CONNECTING};
    if ( ! checkmodes("cb_mode_blink", accepted_modes, NULL, sizeof(accepted_modes))) return;


    if (current_time > data->next_timestamp) {
        switch(data->next_step) {
            case OFF:
                for (int i=0; i<CONFIG_NB_LEDS; i++) {
                    espflam_set_led_RGB(i, conf->color.red,
                                           conf->color.green,
                                           conf->color.blue);
                }
                data->next_timestamp = current_time + conf->blink_ON_period*1000;
                data->blink_count++;
                if (data->blink_count < conf->blink_count) {
                    data->next_step = ON;
                } else {
                    data->next_step = PAUSE;
                }
                break;
            case ON:
                for (int i=0; i<CONFIG_NB_LEDS; i++) {
                    espflam_set_led_RGB(i, 0, 0, 0);
                }
                data->next_timestamp = current_time + conf->blink_ON_period*1000;
                data->next_step = OFF;
                break;
            case PAUSE:
                data->next_timestamp = current_time + conf->pause_period*1000;
                data->blink_count = 0;
                data->next_step = ON;
                break;
            default:
                ESP_LOGE(TAG, "BLINK : Unknown step kind %d. Expected ON, OFF or PAUSE...", data->next_step);
        }
    }
}





// Fire mode callback
typedef struct s_candle {
    float yellowishity;
    float intensity;
} candle_t;

typedef struct s_cb_fire_data {
    uint64_t next_timestamp;
    candle_t candle[CONFIG_NB_LEDS];
} cb_fire_data_t;

void cb_mode_fire( void * arg ) {
    cb_fire_data_t *data = (cb_fire_data_t*)arg;
    const mode_fire_cfg_t *conf = &(mode_config.config.fire);
    uint64_t current_time = esp_timer_get_time();
//     static uint64_t last_time = 0;

    if ( ! checkmode("cb_mode_fire", FIRE, "FIRE")) return;

//     if(current_time < last_time) {
//         // time counter has overflown, reset next_timestamp.
//         // Not accurate calculation, this may cause a little glitch in the
//         // animation, but this occurs only every 584942 years or so...
//         data->next_timestamp = 0;
//         return;
//     }
//
//     if (current_time > data->next_timestamp) {
//         data->next_timestamp = current_time + conf->speed*1000;

    for (int i=0; i<CONFIG_NB_LEDS; i++) {
        candle_t *candle = data->candle + i;

        // candle's delta in yellowishity / intensity
        float y_delta = ( (int)(esp_random()&0xFFFF) - 0X8000) / (float)0X10000;
        float i_delta = ( (int)(esp_random()&0xFFFF) - 0X8000) / (float)0X10000;

        candle->yellowishity += (y_delta * conf->flickering) * conf->speed;
        candle->intensity += (i_delta) * conf->speed;

        // flooring / ceiling
        if (candle->yellowishity < CONFIG_FIRE_YELLOW_MIN)
            candle->yellowishity = CONFIG_FIRE_YELLOW_MIN;
        if (candle->yellowishity > CONFIG_FIRE_YELLOW_MAX)
            candle->yellowishity = CONFIG_FIRE_YELLOW_MAX;

        if (candle->intensity < CONFIG_FIRE_BRIGHT_MIN)
            candle->intensity = CONFIG_FIRE_YELLOW_MIN;
        if (candle->intensity > CONFIG_FIRE_BRIGHT_MAX)
            candle->intensity = CONFIG_FIRE_YELLOW_MAX;

        // convert to RGB and send values to LED
        uint8_t r = 255 * candle->intensity * conf->brightness;
        uint8_t g = 255 * candle->intensity * candle->yellowishity * conf->brightness;
        espflam_set_led_RGB(i+1, r, g, 0);
#if DEBUG_FIRE == 1
        #warning "Fire FX Debug mode"
        ESP_LOGI(TAG, "[%lld] %d (I: %.4f, Y: %.4f, dI: %.4f, dY: %.4f)", current_time, i, candle->intensity, candle->yellowishity, i_delta, y_delta);
//         }
#endif
    }

    //     ESP_LOGW(TAG, "Next : %lld  (curr: %lld, last: %lld)", data->next_timestamp, current_time, last_time);
//     last_time = current_time;
}

/*
// Carrousel mode callback
void cb_mode_carrousel(void* arg) {
    if ( ! checkmode(CARROUSEL, "CARROUSEL")) return;

    cb_carrousel_data_t *data = (cb_carrousel_data_t*)arg;
}*/
