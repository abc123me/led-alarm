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
#include "utils.h"

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

static uint8_t flags;
static pthread_mutex_t flags_mutex;

int parseargs(int argc, char** argv, ws2811_t* ws2811, char **cfg_fname, char **pid_fname);

static void on_interrupt(int signum);

int handle_flags(char* cfg_fname, alarm_config_t *cfg, int *fake_min);
void set_led_colors(alarm_config_t *cfg, int count, ws2811_led_t *leds, ws2811_led_t led);

int main_loop(char* cfg_fname) {
	alarm_config_t cfg;
	int was_on = 0;
	int fake_min = 0;
	int toggle = 0;
	int i;

	cfg.begin_time   = 6 * 60;
	cfg.ramp_up_time = 2 * 60;
	cfg.keep_on_time = 60;
	cfg.brightness   = 255;
	cfg.overrides    = 0;

	while (1) {
		ws2811_led_t color;
		int cur_min, cur_day, cur_sec;
		int on_time, off_time;

		if (handle_flags(cfg_fname, &cfg, &fake_min))
			break;

		if(cfg.overrides & CFG_OVERRIDE_COLOR) {
			/* im gonna use goto for control flow, fuck you */
			color = cfg.override_color;
			goto color_override;
		}

		/* get the system time */
		if(cfg.fake_time > 0 && (cfg.overrides & CFG_OVERRIDE_FAKE)) {
			cur_min = fake_min;
			cur_day = cfg.fake_day;
			srand(0);
		} else {
			struct tm tms;
			time_t now = time(NULL);
			/* get the time */
			localtime_r(&now, &tms);
			cur_min = tms.tm_hour * 60 + tms.tm_min;
			cur_day = tms.tm_wday;
			srand(now);
		}

		/* if you can't read this, you're an idiot */
		toggle = toggle ? 0 : 1;

		/* figure out on / off times */
		on_time = get_begin_time(&cfg, cur_day);

		/* calculate the off_time from the on_time */
		off_time = on_time + cfg.ramp_up_time + cfg.keep_on_time;
		if(off_time > 1440) {
			/* flash red for error */
			color = toggle ? 0xFF0000 : 0x000000;
			goto color_override;
		}

		/* update the leds, default to black */
		if (cur_min >= on_time && cur_min < off_time) {
			float w, wg, wb;

			/* seed the color calculation */
			w = (cur_min - on_time) / ((float) cfg.ramp_up_time);

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
		} else {
			if(was_on && (cfg.overrides & CFG_OVERRIDE_TIME)) {
				cfg.overrides &= ~CFG_OVERRIDE_TIME;
				was_on = 0;
			}
			color = 0;
		}

color_override: /* Fill the buffer with color */
		if(color)
			set_led_colors(&cfg, ledstring.channel[0].count, ledstring.channel[0].leds, color);
		else for (i = 0; i < ledstring.channel[0].count; i++)
			ledstring.channel[0].leds[i] = 0;

		/* Render whatever colors in the buffer */
		if ((i = ws2811_render(&ledstring)) != WS2811_SUCCESS) {
			fprintf(stderr, "ws2811_render failed: %s\n",
					ws2811_get_return_t_str(i));
			break;
		}

		/* and wait a bit */
		if(cfg.verbosity > 0)
			printf("%d %02d:%02d %02d:%02d %02d:%02d #%08X\n",
				   cur_day, cur_min / 60, cur_min % 60,
				   on_time / 60, on_time % 60,
				   off_time / 60, off_time % 60,
				   color);

			/* increment the fake time if applicable */
			if(cfg.fake_time > 0 && (cfg.overrides & CFG_OVERRIDE_FAKE)) {
				if(fake_min > (off_time + FAKE_TIME_WINDOW) || fake_min < (on_time - FAKE_TIME_WINDOW))
					fake_min = on_time - FAKE_TIME_WINDOW;
				else
					fake_min += cfg.fake_time;
			}

			usleep(1000000);
	}
}

int main(int argc, char** argv) {
	int i, ret;
	char *cfg_fname;
	char *pid_fname;
	FILE *pid_fp;

	cfg_fname = "/etc/led-alarm.conf";
	pid_fname = "/var/run/led-alarm.pid";
	pid_fp = NULL;

	/* Parse input args */
	ret = parseargs(argc, argv, &ledstring, &cfg_fname, &pid_fname);
	if(ret) {
		fprintf(stderr, "Failed to parse arguments (%d)!\n", ret);
		return ret;
	}

	/* Initialize the LED strip */
	if ((ret = ws2811_init(&ledstring)) != WS2811_SUCCESS) {
		fprintf(stderr, "ws2811_init failed: %s\n",
		        ws2811_get_return_t_str(ret));
		return ret;
	}

	/* Initialize flags and setup signal handling */
	flags = FLAGS_DEFAULT;
	signal(SIGINT,  on_interrupt); /* Terminates gracefully */
	signal(SIGUSR1, on_interrupt); /* Reloads configuration */
	signal(SIGUSR2, on_interrupt); /* Resets fake time */
	pthread_mutex_init(&flags_mutex, NULL);
	pthread_mutex_unlock(&flags_mutex);

	/* Create a PID file if specified */
	if(pid_fname) {
		pid_fp = fopen(pid_fname, "w");
		if(pid_fp) {
			fprintf(pid_fp, "%d", getpid());
			fclose(pid_fp);
			printf("PID file created at %s\n", pid_fname);
		} else {
			ret = 1;
			fprintf(stderr, "Failed to make PID file at %s!\n", pid_fname);
			goto gtfo_pidf;
		}
	}

	/* Start main loop */
	main_loop(cfg_fname);

gtfo_pidf:
	/* clear the leds on exit */
	for (i = 0; i < ledstring.channel[0].count; i++)
		ledstring.channel[0].leds[i] = 0;
	ws2811_render(&ledstring);

	/* Delete the PID file if one exists */
	if(pid_fname && pid_fp && remove(pid_fname) != 0)
		fprintf(stderr, "Warning - Failed to remove PID file (%s)\n", pid_fname);

	/* finalize */
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

int handle_flags(char* cfg_fname, alarm_config_t *cfg, int *fake_min) {
	pthread_mutex_lock(&flags_mutex);
	/* exit main loop if ordered */
	if((flags & FLAG_RUN_MAIN_LOOP) == 0)
		return 1;

	/* check if the config file needs loaded */
	if(flags & FLAG_RELOAD_CONFIG) {
		alarm_config_t *tmp_cfg = (alarm_config_t*) alloca(sizeof(alarm_config_t));
		if(load_alarm_config(tmp_cfg, cfg_fname) == CONFIG_TRUE) {
			memcpy(cfg, tmp_cfg, sizeof(alarm_config_t));
			if(cfg->verbosity > 0)
				print_config(cfg, stdout);
		}
		flags &= ~FLAG_RELOAD_CONFIG;
	}

	/* Reset fake time if asked */
	if(flags & FLAG_RESET_FAKE_TIME) {
		*fake_min = 0;
		flags &= ~FLAG_RESET_FAKE_TIME;
	}
	pthread_mutex_unlock(&flags_mutex);
	return 0;
}

void set_led_colors(alarm_config_t *cfg, int count, ws2811_led_t *leds, ws2811_led_t led) {
	int noise_lvl = cfg->noise_intensity, bright = cfg->brightness;
	int noise = cfg->noise_type, fade = cfg->line_fade;
	for(int i = 0; i < count; i++) {
		leds[i] = led;

		switch(noise) {
			case NOISE_TYPE_RANDOM:
				leds[i] = rand_noise(i, leds[i], noise_lvl);
				break;
			case NOISE_TYPE_SINE:
				leds[i] = sine_noise(i, leds[i], noise_lvl);
				break;
			case NOISE_TYPE_CLOUDS:
				leds[i] = cloud_noise(i, leds[i], noise_lvl);
				break;
		}

		if(bright < 255)
			leds[i] = brightness(count - i, leds[i], fade);

		if(fade)
			leds[i] = line_fade(count - i, leds[i], fade);
	}
}
