#ifndef _ESPFLAM_PWM_H_
#define _ESPFLAM_PWM_H_

#include <stdint.h>

#define NB_LEDS 3

typedef struct led_conf_s {
    unsigned int red_pin;
    unsigned int green_pin;
    unsigned int blue_pin;
} led_conf_t;

typedef struct led_state_s {
    uint8_t red;
    uint8_t green;
    uint8_t blue;
} led_state_t;

extern led_state_t led_states[NB_LEDS];


void espflam_leds_init(led_conf_t leds_config[NB_LEDS]);

void espflam_set_led_state(unsigned int led_nb, led_state_t state);
void espflam_set_led_RGB(unsigned int led_nb, uint8_t R, uint8_t G, uint8_t B);

void espflam_leds_fini();

#endif
