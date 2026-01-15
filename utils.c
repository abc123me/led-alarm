#include "utils.h"
#include "config.h"

#include <stdlib.h>

#include <math.h>

/* Number of between-pixel brightness steps, for handling line fade */
#define FINE_ADJ_MULTIPLIER 64

#define FINE_ADJ_MAX (256 * FINE_ADJ_MULTIPLIER)

int clamp255(int n) {
	if(n < 0) return 0;
	if(n > 255) return 255;
	return n;
}
int rand_range(int min, int max) {
	return min + (rand() % (max - min + 1));
}

ws2811_led_t _brightness_fine_adj(ws2811_led_t led, int lvl) {
	led_to_rgbw(led);
	r = (lvl * r) / FINE_ADJ_MAX;
	g = (lvl * g) / FINE_ADJ_MAX;
	b = (lvl * b) / FINE_ADJ_MAX;
	w = (lvl * w) / FINE_ADJ_MAX;
	return rgbw_to_led(r, g, b, w);
}
inline ws2811_led_t _brightness_adj(ws2811_led_t led, int lvl) {
	return _brightness_fine_adj(led, lvl * FINE_ADJ_MULTIPLIER);
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
	r = ((256 - rand_range(0, lvl)) * r) / 256;
	g = ((256 - rand_range(0, lvl)) * g) / 256;
	b = ((256 - rand_range(0, lvl)) * b) / 256;
	w = ((256 - rand_range(0, lvl)) * w) / 256;
	return rgbw_to_led(r, g, b, w);
}
ws2811_led_t line_fade(int no, ws2811_led_t led, int lvl) {
	return _brightness_fine_adj(led, FINE_ADJ_MAX - (lvl * no));
}
ws2811_led_t brightness(int no, ws2811_led_t led, int lvl) {
	return _brightness_adj(led, lvl);
}
ws2811_led_t rgb_correct(int no, ws2811_led_t led, ws2811_led_t rgb) {
	led_to_rgbw(led); led_to_rgbw_named(lvl_, rgb);
	r = ((256 - lvl_r) * r) / 256;
	g = ((256 - lvl_g) * g) / 256;
	b = ((256 - lvl_b) * b) / 256;
	w = ((256 - lvl_w) * w) / 256;
	return rgbw_to_led(r, g, b, w);
}
