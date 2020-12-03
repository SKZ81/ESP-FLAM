#ifndef _ESPFALM_MODE_MANAGEMENT_H_
#define _ESPFALM_MODE_MANAGEMENT_H_

#include <stdint.h>

typedef enum e_modeid {
//     ERROR_3=-3,
//     ERROR_2=-2,
    ERROR_1=-1,
    OFF=0,
    FIXED,
    FIRE,
    CARROUSEL
} modeid_t;


typedef s_RGB8_color {
    uint8_t red;
    uint8_t green;
    uint8_t blue;
} RGB8_color_t;


typedef s_mode_error_cfg {
    RGB8_color_t blink_color;
    uint16_t blink_ON_period; // ms
    uint16_t blink_OFF_period; // ms
    uint16_t blink_count;
    uint16_t inter_blink_period; // ms

    /* example :
     * with blink_cout == 3, blink_ON_period == blink_OFF_period == 500,
     * and inter_blink_period =+ 2000
     * The LEDs will blink 3 times in 3 seconds, then 'sleep' for 2 seconds,
     * and start over until error condition is cleared */
} mode_error_cfg_t;


typedef struct s_mode_fixed_cfg {
    RGB8_color_t led1;
    RGB8_color_t led2;
    RGB8_color_t led3;
    uint8_t brightness;
} mode_fixed_cfg_t;


typedef struct s_mode_fire_cfg {
    uint8_t brightness;  // overall brightness factor
    uint8_t intensity;   // of flickering
    uint8_t speed;       // of flickering
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
        mode_error_cfg_t error;
        mode_fixed_cfg_t fixed;
        mode_fire_cfg_t fire;
        mode_carrousel_cfg_t carrousel;
    } config;
} mode_cfg_t;


#endif //_ESPFALM_MODE_MANAGEMENT_H_
