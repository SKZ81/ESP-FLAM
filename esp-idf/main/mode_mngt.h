#ifndef _ESPFALM_MODE_MANAGEMENT_H_
#define _ESPFALM_MODE_MANAGEMENT_H_

#include <stdint.h>

#include "temp_config.h"

typedef enum e_modeid {
//     ERROR_3=-3,
//     ERROR_2=-2,
    ERROR_WIFI=-1, // can cannect to WiFI, provide credentials using BLE
    SLEEP=0, // LEDs off, sleep mode
    BOOTANIM,
    CONNECTING, // to WiFi
    FIXED,
    FIRE,
    CARROUSEL
} modeid_t;



//TODO  : remove/factorize pwm.h led_state_t
typedef struct s_RGB8_color {
    uint8_t red;
    uint8_t green;
    uint8_t blue;
} RGB8_color_t;

typedef struct s_leds_state {
    RGB8_color_t led[CONFIG_NB_LEDS];
} leds_state_t;

typedef struct s_mode_blink_cfg {
    RGB8_color_t color;
    uint16_t blink_ON_period; // ms
    uint16_t blink_OFF_period; // ms
    uint16_t blink_count;
    uint16_t pause_period; // ms

    /* example :
     * with blink_count == 3, blink_ON_period == blink_OFF_period == 500,
     * and inter_blink_period =+ 2000
     * The LEDs will blink 3 times in 3 seconds, then 'sleep' for 2 seconds,
     * and start over until mode is changed (error cleared, ...) */
} mode_blink_cfg_t;


typedef struct s_mode_fixed_cfg {
    leds_state_t leds_state;
    double brightness;
} mode_fixed_cfg_t;


typedef struct s_mode_fire_cfg {
    double brightness;  // overall brightness multiplication factor
    double flickering;   // intensity
    double speed;       // of flickering (period of update in ms)
} mode_fire_cfg_t;


typedef struct s_mode_carrousel_cfg {
    uint8_t nb_colors;
    RGB8_color_t * colors;
    uint8_t brightness;
    uint8_t speed;
    //float phase_offset;
} mode_carrousel_cfg_t;



typedef struct s_mode_cfg {
    modeid_t mode;
    union {
        mode_blink_cfg_t blink;
        mode_fixed_cfg_t fixed;
        mode_fire_cfg_t fire;
        mode_carrousel_cfg_t carrousel;
    } config;

    // if NULL mode is NOT running
    esp_timer_handle_t handle;

} mode_cfg_t;


extern mode_cfg_t mode_config;

#endif //_ESPFALM_MODE_MANAGEMENT_H_
