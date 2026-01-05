#include "utils.h"
#include "config.h"

#include <stdlib.h>

#include <math.h>

#define brightness_adjust_func(FN_NAME, ADJUSTMENT, MAX)        \
	ws2811_led_t FN_NAME(int no, ws2811_led_t led, int lvl) { \
		led_to_rgbw(led);                                       \
		int val = (ADJUSTMENT), amax = (MAX);                   \
		int ri = (r * val) / amax, gi = (g * val) / amax;       \
		int bi = (b * val) / amax, wi = (w * val) / amax;       \
		r = clamp255(r - ri);                                   \
		g = clamp255(g - gi);                                   \
		b = clamp255(b - bi);                                   \
		w = clamp255(w - wi);                                   \
		return rgbw_to_led(r, g, b, w);                         \
	}

int clamp255(int n) {
	if(n < 0) return 0;
	if(n > 255) return 255;
	return n;
}
int rand_range(int min, int max) {
	return min + (rand() % (max - min + 1));
}
ws2811_led_t sine_noise(int no, ws2811_led_t led, int lvl) {
	led_to_rgbw(led);
	/* TODO */
	return led;
}
ws2811_led_t cloud_noise(int no, ws2811_led_t led, int lvl) {
	led_to_rgbw(led);
	/* TODO */
	return led;
}
ws2811_led_t rand_noise(int no, ws2811_led_t led, int lvl) {
	led_to_rgbw(led);
	int ri = (r * lvl) / 100, gi = (g * lvl) / 100;
	int bi = (b * lvl) / 100, wi = (w * lvl) / 100;
	r = clamp255(r + rand_range(-ri, ri));
	g = clamp255(g + rand_range(-gi, gi));
	b = clamp255(b + rand_range(-bi, bi));
	w = clamp255(w + rand_range(-wi, wi));
	return rgbw_to_led(r, g, b, w);
}

brightness_adjust_func(line_fade,  lvl * no, 1000)
brightness_adjust_func(brightness, 255 - lvl, 255)
