#include <math.h>
#include <signal.h>
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <fcntl.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <time.h>
#include <unistd.h>
#include <pthread.h>

#include <ws2811.h>

#include <libconfig.h>

#include "config.h"

#define ARRAY_SIZE(stuff) (sizeof(stuff) / sizeof(stuff[0]))

// defaults for cmdline options
#define TARGET_FREQ          WS2811_TARGET_FREQ
#define GPIO_PIN             18
#define DMA                  10
#define STRIP_TYPE           WS2811_STRIP_RGB
#define LED_COUNT            80

#define FLAG_RUN_MAIN_LOOP   0b00000001
#define FLAG_RELOAD_CONFIG   0b00000010
#define FLAG_RESET_FAKE_TIME 0b00000100
#define FLAGS_DEFAULT        0b00000111

#define FAKE_TIME_WINDOW     20

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

static void on_interrupt(int signum);

int main(int argc, char* argv[]) {
	int i, ret, cur_min, cur_day;
	int on_time, off_time, was_on;
	int fake_min;
	alarm_config_t ld_cfg, cfg;
	struct tm tms;
	time_t now;

	ld_cfg.begin_time   = 6 * 60;
	ld_cfg.ramp_up_time = 2 * 60;
	ld_cfg.keep_on_time = 60;
	ld_cfg.brightness   = 255;
	ld_cfg.overrides    = 0;

	pthread_mutex_init(&flags_mutex, NULL);
	pthread_mutex_unlock(&flags_mutex);

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
	signal(SIGUSR2, on_interrupt); /* Resets fake time */

	if ((ret = ws2811_init(&ledstring)) != WS2811_SUCCESS) {
		fprintf(stderr, "ws2811_init failed: %s\n",
		        ws2811_get_return_t_str(ret));
		return ret;
	}

	was_on = cur_day = cur_min = 0;
	fake_min = 0;
	while (1) {
		int color = 0;

		pthread_mutex_lock(&flags_mutex);
		/* exit main loop if ordered */
		if((flags & FLAG_RUN_MAIN_LOOP) == 0)
			break;

		/* check if the config file needs loaded */
		if(flags & FLAG_RELOAD_CONFIG) {
			if(try_load_config("/etc/led-alarm.conf", &ld_cfg) == CONFIG_TRUE) {
				memcpy(&cfg, &ld_cfg, sizeof(alarm_config_t));
				if(ld_cfg.verbosity > 0)
					print_config(stdout, &cfg);
			}
			flags &= ~FLAG_RELOAD_CONFIG;
		}

		/* Reset fake time if asked */
		if(flags & FLAG_RESET_FAKE_TIME) {
			fake_min = 0;
			flags &= ~FLAG_RESET_FAKE_TIME;
		}

		pthread_mutex_unlock(&flags_mutex);

		/* check if make up a time */
		if(ld_cfg.fake_time > 0 && (ld_cfg.overrides & CFG_OVERRIDE_FAKE)) {
			cur_min = fake_min;
			cur_day = ld_cfg.fake_day;
		} else {
			now = time(NULL);
			/* get the time */
			localtime_r(&now, &tms);
			cur_min = tms.tm_hour * 60 + tms.tm_min;
			cur_day = tms.tm_wday;
		}

		/* figure out on / off times */
		on_time = ld_cfg.begin_time;
		if(ld_cfg.overrides) {
			if(ld_cfg.overrides & CFG_OVERRIDE_COLOR) {
				/* im gonna use goto for control flow, fuck you */
				color = ld_cfg.override_color;
				goto color_override;
			}

			if(ld_cfg.overrides & CFG_OVERRIDE_DAY_MASK)
				if(ld_cfg.overrides & (CFG_OVERRIDE_WEEKDAY_0 << cur_day))
					on_time = ld_cfg.begin_times[cur_day];

			if(ld_cfg.overrides & CFG_OVERRIDE_TIME)
				on_time = ld_cfg.override_time;
		}
		off_time = on_time + ld_cfg.ramp_up_time + ld_cfg.keep_on_time;

		/* update the leds, default to black */
		if (cur_min >= on_time && cur_min < off_time) {
			float w, wg, wb;

			/* seed the color calculation */
			w = (cur_min - on_time) / ((float) ld_cfg.ramp_up_time);

			/* make sure to clamp here for keep-on time */
			if (w > 1.0f)
				w = 1.0f;

			/* calculate the color using some polynomial bs */
			wg = 0.7 - ((1 - w) * 0.6f);
			wb = 0.3 - ((1 - w) * 0.25f);
			color |= (int)round(w * 1.0f * 255) << 16;
			color |= (int)round(w * wg * 255) << 8;
			color |= (int)round(w * wg * 255);
			was_on = 1;
		} else if(was_on && (ld_cfg.overrides & CFG_OVERRIDE_TIME)) {
			ld_cfg.overrides &= ~CFG_OVERRIDE_TIME;
			was_on = 0;
		}

		/* Fill the buffer with color */
		/* TODO: Throw in some sine wave / perlin noise bs here */
color_override:
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
		if(ld_cfg.verbosity > 0)
			printf("%d %02d:%02d %02d:%02d %02d:%02d #%08X\n",
			        cur_day, cur_min / 60, cur_min % 60,
			        on_time / 60, on_time % 60,
			        off_time / 60, off_time % 60,
					color);

		/* increment the fake time if applicable */
		if(ld_cfg.fake_time > 0 && (ld_cfg.overrides & CFG_OVERRIDE_FAKE)) {
			if(fake_min > (off_time + FAKE_TIME_WINDOW) || fake_min < (on_time - FAKE_TIME_WINDOW))
				fake_min = on_time - FAKE_TIME_WINDOW;
			else
				fake_min += ld_cfg.fake_time;
		}

		usleep(1000000);
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
		case SIGUSR2:
			flags |= FLAG_RESET_FAKE_TIME;
			puts("Interrupt detected, resetting fake time!");
			break;
		case SIGINT:
			puts("Interrupt detected, exiting!");
			goto sigstop;
		default:
			puts("Unknown interrupt detected, exiting!");
sigstop:
			flags &= ~FLAG_RUN_MAIN_LOOP;
			break;
	}
	pthread_mutex_unlock(&flags_mutex);
}
