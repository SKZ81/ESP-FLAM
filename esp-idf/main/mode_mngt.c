#include <stdbool.h>
#include <esp_log.h>
#include <esp_system.h>
#include <esp_timer.h>


#include "mode_mngt.h"
#include "pwm.h"

mode_cfg_t mode_config;

static const char *TAG = "ModeMngt";


static inline bool checkmode(uint8_t expected_mode, const char* expected_mode_name) {
    if (mode_config.mode != FIRE) {
        ESP_LOGE(TAG, "In cb_mode_fire but mode != FIRE (%d)", mode_config.mode);
        ESP_LOGI(TAG, "Cowardly return without doing nothing...");
        return false;
    }
    return true;
}


static inline bool checkmodes(uint8_t accepted_modes[], uint8_t accepted_modes_size) {
    uint8_t index = 0;
    while (index < accepted_modes_size && accepted_modes[index] != mode_config.mode) {
        index++;
    }
    if (index == accepted_modes_size) {
        ESP_LOGE(TAG, "In cb_mode_blink but mode (%d)", mode_config.mode);
        ESP_LOGE(TAG, "Not in accepted_modes list :");
        for(uint8_t i=0; i<accepted_modes_size; i++) {
            ESP_LOGE(TAG, " - %d", accepted_modes[i]);
        }
        ESP_LOGI(TAG, "Cowardly return without doing nothing...");
        return false;
    }
    return true;
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
    uint64_t current_time = esp_timer_get_time();

    static uint8_t accepted_modes[] = { ERROR_WIFI,
                                        CONNECTING};
    if ( ! checkmodes(accepted_modes, sizeof(accepted_modes))) return;


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

    if ( ! checkmode(FIRE, "FIRE")) return;

    if(current_time < data->next_timestamp) {
        // time counter has overflown, reset next_timestamp.
        // Not accurate calculation, this may cause a little glitch in the
        // animation, but this occurs only every 584942 years or so...
        data->next_timestamp = 0;
        return;
    }

    if (current_time > data->next_timestamp) {
        data->next_timestamp = current_time + conf->speed*1000;

        for (int i=0; i<CONFIG_NB_LEDS; i++) {
            candle_t *candle = data->candle + i;

            // candle's delta in yellowishity / intensity
            float y_delta = ( (esp_random()&0xFFFF) - 0X800) / (float)0X10000;
            float i_delta = ( (esp_random()&0xFFFF) - 0X800) / (float)0X10000;

            candle->yellowishity += (y_delta * conf->flickering);
            candle->intensity += (i_delta);

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
            espflam_set_led_RGB(i, r, g, 0);
            ESP_LOGI(TAG, "FIRE candle %d (I: %.4f, Y: %.4f)  ========> rgb(%d, %d, 0)", i, candle->yellowishity, candle->intensity, r, g);
        }
    }
}

/*
// Carrousel mode callback
void cb_mode_carrousel(void* arg) {
    if ( ! checkmode(CARROUSEL, "CARROUSEL")) return;

    cb_carrousel_data_t *data = (cb_carrousel_data_t*)arg;
}*/
