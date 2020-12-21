#ifndef _ESPFLAM_COLOR_SPCAES_H_
#define _ESPFLAM_COLOR_SPCAES_H_

#include <stdint.h>

typedef struct s_RGB_color {
    uint8_t r;
    uint8_t g;
    uint8_t b;
} RGB_color_t;

typedef struct s_HSV_color {
    float h;
    float s;
    float v;
} HSV_color_t;

typedef struct s_HSL_color {
    float h;
    float s;
    float l;
} HSL_color_t;


void rgb2hsl(const RGB_color_t *rgb, HSL_color_t *hsl);
void hsl2rgb(const HSL_color_t *hsl, RGB_color_t *rgb);

void rgb2hsv(const RGB_color_t *rgb, HSV_color_t *hsv);
void hsv2rgb(const HSV_color_t *hsv, RGB_color_t *rgb);


inline void init_rgb(RGB_color_t *rgb, float r, float g, float b) {
    rgb->r = r;
    rgb->g = g;
    rgb->b = b;
}

inline void init_hsv(HSV_color_t *hsv, float h, float s, float v) {
    hsv->h = h;
    hsv->s = s;
    hsv->v = v;
}

inline void init_hsl(HSL_color_t *hsl, float h, float s, float l) {
    hsl->h = h;
    hsl->s = s;
    hsl->l = l;
}


#endif //_ESPFLAM_COLOR_SPCAES_H_
