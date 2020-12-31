#include "color_spaces.h"
#include <stdio.h>
#include "esp_log.h"

static inline float min2(float a, float b) {
    return (a<b)? a : b;
}


static inline float max2(float a, float b) {
    return (b<a)? a : b;
}

static inline float min3(float a, float b, float c) {
    return min2(min2(a,b), c);
}

static inline float max3(float a, float b, float c) {
    return max2(max2(a,b), c);
}

void rgb2hsl(const RGB_color_t *rgb, HSL_color_t *hsl) {
    float r = rgb->r / 255.0;
    float g = rgb->g / 255.0;
    float b = rgb->b / 255.0;

    float max = max3(r, g, b);
    float min = min3(r, g, b);
    hsl->l = (max + min) / 2.0;
//     printf("min: %f, max: %f, l: %f\n", min, max, hsl->l);

    if (max == min) {
        hsl->h = hsl->s = 0.0; // achromatic
//         printf("achromatic\n");
    } else {
        float d = max - min;
        hsl->s = (hsl->l > 0.5) ? d / (2.0 - max - min)
                 :
                 d / (max + min);

        if (max == r) {
            hsl->h = (g - b) / d + (g < b ? 6.0 : 0.0);
        } else if (max == g) {
            hsl->h = (b - r) / d + 2.0;
        } else if (max == b) {
            hsl->h = (r - g) / d + 4.0;
        }

        hsl->h /= 6.0;
    }
}


static float  hue2rgb(float p, float q, float t) {
    while (t < 0.0) t += 1.0;
    while (t > 1.0) t -= 1.0;
    if (t < 1.0/6.0) return p + (q - p) * 6.0 * t;
    if (t < 1.0/2.0) return q;
    if (t < 2.0/3.0) return p + (q - p) * (2.0/3.0 - t) * 6.0;
    return p;
}


void hsl2rgb(const HSL_color_t *hsl, RGB_color_t *rgb) {
    float r, g, b;

    if (hsl->s == 0.0) {
        r = g = b = hsl->l; // achromatic
    } else {
        float q = (hsl->l < 0.5) ?
                    hsl->l * (1.0 + hsl->s)
                    :
                    hsl->l + hsl->s - hsl->l * hsl->s;
        float p = 2.0 * hsl->l - q;

        r = hue2rgb(p, q, hsl->h + 1.0/3.0);
        g = hue2rgb(p, q, hsl->h);
        b = hue2rgb(p, q, hsl->h - 1.0/3.0);
    }

    rgb->r = (uint8_t)(r*255);
    rgb->g = (uint8_t)(g*255);
    rgb->b = (uint8_t)(b*255);
}


void rgb2hsv(const RGB_color_t *rgb, HSV_color_t *hsv) {
    float r = rgb->r / 255.0;
    float g = rgb->g / 255.0;
    float b = rgb->b / 255.0;

    float max = max3(r, g, b);
    float min = min3(r, g, b);
    hsv->v = max;

    float d = max - min;
    hsv->s = (max == 0.0) ? 0.0 : d / max;

    if(max == min) {
        hsv->h = 0.0; // achromatic
    } else {
        if(max == r) {
            hsv->h = (g - b) / d + (g < b ? 6.0 : 0.0);
        } else if (max == g) {
            hsv->h = (b - r) / d + 2.0;
        } else if (max == b) {
            hsv->h = (r - g) / d + 4.0;
        }
        hsv->h /= 6.0;
    }
}

void hsv2rgb(const HSV_color_t *hsv, RGB_color_t *rgb) {
    float h = hsv->h;
    float s = hsv->s;
    float v = hsv->v;
    float r=0, g=0, b=0;

    int i = (int)(h * 6);
    float f = h * 6 - i;
    float p = v * (1 - s);
    float q = v * (1 - f * s);
    float t = v * (1 - (1 - f) * s);

    switch(i % 6) {
        case 0: r = v, g = t, b = p; break;
        case 1: r = q, g = v, b = p; break;
        case 2: r = p, g = v, b = t; break;
        case 3: r = p, g = q, b = v; break;
        case 4: r = t, g = p, b = v; break;
        case 5: r = v, g = p, b = q; break;
    }
    rgb->r = (uint8_t)(r*255);
    rgb->g = (uint8_t)(g*255);
    rgb->b = (uint8_t)(b*255);

    ESP_LOGD("hsv2rgb", "(%f,%f,%f) -> (%d,%d,%d)", hsv->h, hsv->s, hsv->v, rgb->r, rgb->g, rgb->b);
}





// Unit test

RGB_color_t rgb1 = {0x80, 0x80, 0x80};
RGB_color_t rgb2 = {0xFF, 0x00, 0x00};
RGB_color_t rgb3 = {0x00, 0xFF, 0x80};
RGB_color_t rgb4 = {0x00, 0x00, 0xFF};
RGB_color_t rgb5 = {0xFF, 0xFF, 0x00};
RGB_color_t rgb6 = {0x00, 0xFF, 0xFF};
RGB_color_t rgb7 = {0xFF, 0x00, 0xFF};
RGB_color_t rgb8 = {0xFF, 0xFF, 0xFF};
RGB_color_t rgb9 = {0x12, 0x34, 0x56};
RGB_color_t rgb10 = {0xCB, 0xA9, 0x87};
RGB_color_t rgb11 = {0xF1, 0x2E, 0xD3};
RGB_color_t rgb12 = {0x42, 0x42, 0x42};

const unsigned int nb_test_rgb_colors = 12;

RGB_color_t *test_rgb_colors[] = {
    &rgb1, &rgb2, &rgb3, &rgb4,
    &rgb5, &rgb6, &rgb7, &rgb8,
    &rgb9, &rgb10, &rgb11, &rgb12//,
//     &rgb13, &rgb14, &rgb3, &rgb1,
};



void test_color_spaces() {
    RGB_color_t rgb;
    HSV_color_t hsv;
    HSL_color_t hsl;

    for(unsigned int i=0; i<nb_test_rgb_colors; i++) {
        rgb2hsl(test_rgb_colors[i], &hsl);
        rgb2hsv(test_rgb_colors[i], &hsv);
        printf("Color %d : RGB(%d, %d, %d)\n", i,
               test_rgb_colors[i]->r,
               test_rgb_colors[i]->g,
               test_rgb_colors[i]->b);
        hsl2rgb(&hsl, &rgb);
        printf("   HSL => RGB: (h:%f, s:%f, l:%f) => (%d, %d, %d) %s\n",
               hsl.h, hsl.s, hsl.l, rgb.r, rgb.g, rgb.b,
               (   (test_rgb_colors[i]->r) == rgb.r
                && (test_rgb_colors[i]->g) == rgb.g
                && (test_rgb_colors[i]->b) == rgb.b ) ? "OK" : "EROOR !!");
        hsv2rgb(&hsv, &rgb);
        printf("   HSV => RGB: (h:%f, s:%f, v:%f) => (%d, %d, %d) %s\n\n",
               hsv.h, hsv.s, hsv.v, rgb.r, rgb.g, rgb.b,
               (   (test_rgb_colors[i]->r) == rgb.r
                && (test_rgb_colors[i]->g) == rgb.g
                && (test_rgb_colors[i]->b) == rgb.b ) ? "OK" : "EROOR !!");
    }
}




// ___________________ main.c (unit test call) __________________

/*
 * extern void test_color_spaces();
 * int main(void) [
 *     test_color_spaces();
 * ]
 */

