#ifndef _UTILS_H
#define _UTILS_H

#include <ws2811.h>

#define rgbw_to_led(r, g, b, w) \
	(r + (g << 8) + (b << 16) + (w << 24))
#define led_to_rgbw(led) \
	int r = led & 0xFF;         \
	int g = (led >> 8) & 0xFF;  \
	int b = (led >> 16) & 0xFF; \
	int w = (led >> 24) & 0xFF;

int clamp255(int n);
int rand_range(int min, int max);
ws2811_led_t rand_noise (int no, ws2811_led_t led, int lvl);
ws2811_led_t sine_noise (int no, ws2811_led_t led, int lvl);
ws2811_led_t cloud_noise(int no, ws2811_led_t led, int lvl);
ws2811_led_t line_fade  (int no, ws2811_led_t led, int lvl);
ws2811_led_t brightness (int no, ws2811_led_t led, int lvl);

#endif
