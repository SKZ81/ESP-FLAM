#include "pwm.h"

#include <stdio.h>
#include "string.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
// #include "freertos/queue.h"
// #include "esp_attr.h"
// #include "soc/rtc.h"
#include "driver/mcpwm.h"
#include "soc/mcpwm_periph.h"
#include <esp_log.h>


led_state_t led_states[NB_LEDS];

static const char *TAG = "led_pwm";

void espflam_leds_init(led_conf_t leds_config[NB_LEDS]) {
    ESP_LOGI(TAG, "Set LED 0 red pin as GPIO %u", leds_config[0].red_pin);
    ESP_LOGI(TAG, "Set LED 0 green pin as GPIO %u", leds_config[0].green_pin);
    ESP_LOGI(TAG, "Set LED 0 blue pin as GPIO %u", leds_config[0].blue_pin);
    ESP_LOGI(TAG, "Set LED 1 red pin as GPIO %u", leds_config[1].red_pin);
    ESP_LOGI(TAG, "Set LED 1 green pin as GPIO %u", leds_config[1].green_pin);
    ESP_LOGI(TAG, "Set LED 1 blue pin as GPIO %u", leds_config[1].blue_pin);
    ESP_LOGI(TAG, "Set LED 2 red pin as GPIO %u", leds_config[2].red_pin);
    ESP_LOGI(TAG, "Set LED 2 green pin as GPIO %u", leds_config[2].green_pin);
    ESP_LOGI(TAG, "Set LED 2 blue pin as GPIO %u", leds_config[2].blue_pin);

    mcpwm_gpio_init(MCPWM_UNIT_0, MCPWM0A, leds_config[0].red_pin);
    mcpwm_gpio_init(MCPWM_UNIT_0, MCPWM0B, leds_config[0].green_pin);
    mcpwm_gpio_init(MCPWM_UNIT_0, MCPWM1A, leds_config[0].blue_pin);
    mcpwm_gpio_init(MCPWM_UNIT_0, MCPWM1B, leds_config[1].red_pin);
    mcpwm_gpio_init(MCPWM_UNIT_0, MCPWM2A, leds_config[1].green_pin);
    mcpwm_gpio_init(MCPWM_UNIT_0, MCPWM2B, leds_config[1].blue_pin);
    mcpwm_gpio_init(MCPWM_UNIT_1, MCPWM0A, leds_config[2].red_pin);
    mcpwm_gpio_init(MCPWM_UNIT_1, MCPWM0B, leds_config[2].green_pin);
    mcpwm_gpio_init(MCPWM_UNIT_1, MCPWM1A, leds_config[2].blue_pin);

    ESP_LOGI(TAG, "Configuring Initial Parameters of mcpwm...");
    mcpwm_config_t pwm_config;
    pwm_config.frequency = 1000;    //frequency = 1000Hz
    pwm_config.cmpr_a = 66.0;       //duty cycle of PWMxA = 00.0%
    pwm_config.cmpr_b = 33.0;       //duty cycle of PWMxb = 00.0%
    pwm_config.counter_mode = MCPWM_UP_DOWN_COUNTER;
    pwm_config.duty_mode = MCPWM_DUTY_MODE_0;

    mcpwm_init(MCPWM_UNIT_0, MCPWM_TIMER_0, &pwm_config);
    mcpwm_init(MCPWM_UNIT_0, MCPWM_TIMER_1, &pwm_config);
    mcpwm_init(MCPWM_UNIT_0, MCPWM_TIMER_2, &pwm_config);

    mcpwm_init(MCPWM_UNIT_1, MCPWM_TIMER_0, &pwm_config);
    mcpwm_init(MCPWM_UNIT_1, MCPWM_TIMER_1, &pwm_config);
//     mcpwm_init(MCPWM_UNIT_1, MCPWM_TIMER_2, &pwm_config);
}

void espflam_set_led_state(uint led_nb, led_state_t state) {
    int unit[3], timer[3], channel[3];
    float value[3];
    switch(led_nb) {
        case 1:
            unit[0] = MCPWM_UNIT_0; timer[0] = MCPWM_TIMER_0; channel[0] = MCPWM_OPR_A;
            unit[1] = MCPWM_UNIT_0; timer[1] = MCPWM_TIMER_0; channel[1] = MCPWM_OPR_B;
            unit[2] = MCPWM_UNIT_0; timer[2] = MCPWM_TIMER_1; channel[2] = MCPWM_OPR_A;
            break;
        case 2:
            unit[0] = MCPWM_UNIT_0; timer[0] = MCPWM_TIMER_1; channel[0] = MCPWM_OPR_B;
            unit[1] = MCPWM_UNIT_0; timer[1] = MCPWM_TIMER_2; channel[1] = MCPWM_OPR_A;
            unit[2] = MCPWM_UNIT_0; timer[2] = MCPWM_TIMER_2; channel[2] = MCPWM_OPR_B;
            break;
        case 3:
            unit[0] = MCPWM_UNIT_1; timer[0] = MCPWM_TIMER_0; channel[0] = MCPWM_OPR_A;
            unit[1] = MCPWM_UNIT_1; timer[1] = MCPWM_TIMER_0; channel[1] = MCPWM_OPR_B;
            unit[2] = MCPWM_UNIT_1; timer[2] = MCPWM_TIMER_1; channel[2] = MCPWM_OPR_A;
            break;
        default:
            ESP_LOGE(TAG, "Led #%d is not managed !! Ignoring..", led_nb);
            return;
    }
    value[0] = state.red / 2.55;
    value[1] = state.green / 2.55;
    value[2] = state.blue / 2.55;

    for (int i=0; i<3; i++) mcpwm_set_duty(unit[i], timer[i], channel[i], value[i]);
}

void espflam_set_led_RGB(unsigned int led_nb, uint8_t R, uint8_t G, uint8_t B) {
    led_state_t state = {
        .red = R,
        .green = G,
        .blue = B
    };
    espflam_set_led_state(led_nb, state);
}


void espflam_leds_fini() {
}
