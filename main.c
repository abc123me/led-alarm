#include <fcntl.h>
#include <math.h>
#include <signal.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <time.h>
#include <unistd.h>
#include <pthread.h>

#include "ws2811.h"

#define ARRAY_SIZE(stuff) (sizeof(stuff) / sizeof(stuff[0]))

// defaults for cmdline options
#define TARGET_FREQ        WS2811_TARGET_FREQ
#define GPIO_PIN           18
#define DMA                10
#define STRIP_TYPE         WS2811_STRIP_RGB
#define LED_COUNT          80

#define BEGIN_RAMP_UP_TIME 360 // 6AM - ramp up start time in hours
#define RAMP_UP_DURATION_M 120 // 2HR - how long should it ramp up
#define KEEP_ON_DURATION_M 60  // 1HR - how long after ramp up to keep on
#define BEGIN_SHUTOFF_TIME (BEGIN_RAMP_UP_TIME + RAMP_UP_DURATION_M + KEEP_ON_DURATION_M)
// and now off

#define FLAG_RUN_MAIN_LOOP 0b00000001
#define FLAG_RELOAD_CONFIG 0b00000010
#define FLAGS_DEFAULT      0b00000011

#define FAKE_TIME_WINDOW   20

ws2811_t ledstring = {
	.freq = TARGET_FREQ,
	.dmanum = DMA,
	.channel =
		{
			[0] =
				{
					.gpionum = GPIO_PIN,
					.invert = 0,
					.count = LED_COUNT,
					.strip_type = STRIP_TYPE,
					.brightness = 255,
				},
			[1] =
				{
					.gpionum = 0,
					.invert = 0,
					.count = 0,
					.brightness = 0,
				},
		},
};

static uint8_t flags = FLAGS_DEFAULT;
static pthread_mutex_t flags_mutex;

int parseargs(int argc, char** argv, ws2811_t* ws2811);
int calc_color(int min);
static void on_interrupt(int signum);

int main(int argc, char* argv[]) {
	int i, ret, cur_min;
	struct tm tms;
	time_t now;

	pthread_mutex_init(&flags_mutex, NULL);

	ret = parseargs(argc, argv, &ledstring);
	if(ret) {
		fprintf(stderr, "Failed to parse arguments!\n");
		return ret;
	}

	ws2811_led_t* leds = malloc(sizeof(ws2811_led_t) * ledstring.channel[0].count);
	if (!leds)
		return 1;

	signal(SIGINT,  on_interrupt); /* Terminates gracefully */
	signal(SIGUSR1, on_interrupt); /* Reloads configuration */

	if ((ret = ws2811_init(&ledstring)) != WS2811_SUCCESS) {
		fprintf(stderr, "ws2811_init failed: %s\n",
		        ws2811_get_return_t_str(ret));
		return ret;
	}

	while (1) {
		int color = 0;

		pthread_mutex_lock(&flags_mutex);
		/* exit main loop if ordered */
		if(flags & FLAG_RUN_MAIN_LOOP == 0)
			break;

		/* check if the config file needs loaded */
		if(flags & FLAG_RELOAD_CONFIG) {
			printf("Loading config from %s\n", "/etc/led-alarm.conf");
			puts("TODO");
			printf("Loaded config from %s\n", "/etc/led-alarm.conf");
			flags &= ~FLAG_RELOAD_CONFIG;
		}
		pthread_mutex_unlock(&flags_mutex);

#ifdef FAKE_TIME
		/* make up a time */
		if(cur_min > BEGIN_SHUTOFF_TIME + FAKE_TIME_WINDOW || \
		   cur_min < BEGIN_RAMP_UP_TIME - FAKE_TIME_WINDOW)
			cur_min = BEGIN_RAMP_UP_TIME - FAKE_TIME_WINDOW;
		else cur_min++;
#else
		/* get the time */
		now = time(NULL);
		localtime_r(&now, &tms);
		cur_min = tms.tm_hour * 60 + tms.tm_min;
#endif

		/* update the leds, default to black */
		if (cur_min >= BEGIN_RAMP_UP_TIME && cur_min < BEGIN_SHUTOFF_TIME) {
			float w, wg, wb;

			/* seed the color calculation */
			w = (cur_min - BEGIN_RAMP_UP_TIME) / ((float) RAMP_UP_DURATION_M);

			/* make sure to clamp here for keep-on time */
			if (w > 1.0f)
				w = 1.0f;

			/* calculate the color using some polynomial bs */
			wg = 0.7 - ((1 - w) * 0.6f);
			wb = 0.3 - ((1 - w) * 0.25f);
			color |= (int)round(w * 1.0f * 255) << 16;
			color |= (int)round(w * wg * 255) << 8;
			color |= (int)round(w * wg * 255);
		}

		/* Fill the buffer with color */
		/* TODO: Throw in some sine wave / perlin noise bs here */
		for (i = 0; i < ledstring.channel[0].count; i++)
			leds[i] = color;

		/* Render whatever colors in the buffer */
		for (i = 0; i < ledstring.channel[0].count; i++)
			ledstring.channel[0].leds[i] = leds[i];
		if ((ret = ws2811_render(&ledstring)) != WS2811_SUCCESS) {
			fprintf(stderr, "ws2811_render failed: %s\n",
			        ws2811_get_return_t_str(ret));
			break;
		}

		/* and wait a bit */
#ifdef FAKE_TIME
		printf("%d:%d:%d %.1f %.1f #%08X\n", min / 60, min % 60, 0, wg, wb, color);
		usleep(25000);
#else
		usleep(1000000);
#endif
	}

	/* clear the leds on exit */
	for (int i = 0; i < ledstring.channel[0].count; i++)
		ledstring.channel[0].leds[i] = leds[i] = 0;
	ws2811_render(&ledstring);

	/* finalize */
	free(leds);
	ws2811_fini(&ledstring);
	printf("\n");
	return ret;
}

static void on_interrupt(int signum) {
	pthread_mutex_lock(&flags_mutex);
	switch(signum) {
		case SIGUSR1:
			flags |= FLAG_RELOAD_CONFIG;
			puts("Interrupt detected, ordering config reload!");
			break;
		default:
			flags &= ~FLAG_RUN_MAIN_LOOP;
			puts("Interrupt detected, exiting!");
			break;
	}
	pthread_mutex_unlock(&flags_mutex);
}
