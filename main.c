#include <fcntl.h>
#include <getopt.h>
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

#include "ws2811.h"

#define ARRAY_SIZE(stuff) (sizeof(stuff) / sizeof(stuff[0]))

// defaults for cmdline options
#define TARGET_FREQ        WS2811_TARGET_FREQ
#define GPIO_PIN           18
#define DMA                10
#define STRIP_TYPE         WS2811_STRIP_RGB
#define LED_COUNT          50

#define BEGIN_RAMP_UP_TIME 5
#define RAMP_UP_DURATION   3  // 5-7AM getting brighter
#define KEEP_ON_DURATION   2  // 8-10AM max brightness
// and now off

int led_count = LED_COUNT;
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
static uint8_t running = 1;
static void on_interrupt(int signum) {
	running = 0;
}
void parseargs(int argc, char** argv, ws2811_t* ws2811) {
	int index, c = 0;
	static struct option longopts[] = {{"help", no_argument, 0, 'h'},
	                                   {"dma", required_argument, 0, 'd'},
	                                   {"gpio", required_argument, 0, 'g'},
	                                   {"invert", no_argument, 0, 'i'},
	                                   {"strip", required_argument, 0, 's'},
	                                   {"count", required_argument, 0, 'y'},
	                                   {"version", no_argument, 0, 'v'},
	                                   {0, 0, 0, 0}};
	while (c = getopt_long(argc, argv, "c:d:g:his:v", longopts, &index) >= 0) {
		switch (c) {
			case 0:
				/* handle flag options (array's 3rd field non-0) */
				break;
			case 'h':
				fprintf(stderr,
				        "Usage: %s \n"
				        "-h (--help)    - this information\n"
				        "-s (--strip)   - strip type - rgb, grb, gbr, rgbw "
				        "(default rgb)\n"
				        "-c (--count)   - led count (default %d)\n"
				        "-d (--dma)     - dma channel to use (default %d)\n"
				        "-g (--gpio)    - GPIO to use\n"
				        "                 If omitted, default is 18 (PWM0)\n"
				        "-i (--invert)  - invert pin output (pulse %s)\n",
				        argv[0], ws2811->channel[0].count, ws2811->dmanum,
				        ws2811->channel[0].gpionum,
				        ws2811->channel[0].invert ? "LOW" : "HIGH");
				exit(-1);
			case 'D':
				break;
			case 'g':
				if (optarg)
					ws2811->channel[0].gpionum = atoi(optarg);
				break;
			case 'i':
				ws2811->channel[0].invert = 1;
				break;
			case 'd':
				if (optarg) {
					int dma = atoi(optarg);
					if (dma < 14) {
						ws2811->dmanum = dma;
					} else {
						printf("invalid dma %d\n", dma);
						exit(-1);
					}
				}
				break;
			case 'c':
				if (optarg) {
					led_count = atoi(optarg);
					if (led_count > 0) {
						ws2811->channel[0].count = led_count;
					} else {
						printf("invalid led count %d\n", led_count);
						exit(-1);
					}
				}
				break;
			case 's':
				if (optarg) {
					if (!strncasecmp("rgb", optarg, 4)) {
						ws2811->channel[0].strip_type = WS2811_STRIP_RGB;
					} else if (!strncasecmp("rbg", optarg, 4)) {
						ws2811->channel[0].strip_type = WS2811_STRIP_RBG;
					} else if (!strncasecmp("grb", optarg, 4)) {
						ws2811->channel[0].strip_type = WS2811_STRIP_GRB;
					} else if (!strncasecmp("gbr", optarg, 4)) {
						ws2811->channel[0].strip_type = WS2811_STRIP_GBR;
					} else if (!strncasecmp("brg", optarg, 4)) {
						ws2811->channel[0].strip_type = WS2811_STRIP_BRG;
					} else if (!strncasecmp("bgr", optarg, 4)) {
						ws2811->channel[0].strip_type = WS2811_STRIP_BGR;
					} else if (!strncasecmp("rgbw", optarg, 4)) {
						ws2811->channel[0].strip_type = SK6812_STRIP_RGBW;
					} else if (!strncasecmp("grbw", optarg, 4)) {
						ws2811->channel[0].strip_type = SK6812_STRIP_GRBW;
					} else {
						printf("invalid strip %s\n", optarg);
						exit(-1);
					}
				}
				break;
			case '?':
				/* getopt_long already reported error? */
				exit(-1);
			default:
				exit(-1);
		}
	}
}

int main(int argc, char* argv[]) {
	ws2811_return_t ret;

	parseargs(argc, argv, &ledstring);

	ws2811_led_t* leds = malloc(sizeof(ws2811_led_t) * LED_COUNT);
	if (!leds)
		return 1;

	signal(SIGINT, on_interrupt);

	if ((ret = ws2811_init(&ledstring)) != WS2811_SUCCESS) {
		fprintf(stderr, "ws2811_init failed: %s\n",
		        ws2811_get_return_t_str(ret));
		return ret;
	}

	struct tm tm_struct;
	int hour = BEGIN_RAMP_UP_TIME - 1, min = 50, sec = 0;
	while (running) {
		time_t now = time(NULL);
		localtime_r(&now, &tm_struct);
		// int hour = tm_struct.tm_hour;
		// int min = tm_struct.tm_min;
		// int sec = tm_struct.tm_sec;
		min++;
		if (min > 60) {
			hour++;
			min = 0;
		}
		if (hour > BEGIN_RAMP_UP_TIME + RAMP_UP_DURATION + KEEP_ON_DURATION) {
			hour = BEGIN_RAMP_UP_TIME - 1;
			min = 50;
		}
		int color = 0x000000;
		// hour = 6;
		// int minute = tm_struct.tm_min;
		color = 0;
		int oh = hour;
		if (hour >= BEGIN_RAMP_UP_TIME) {  // Start at 5AM
			hour -= BEGIN_RAMP_UP_TIME;

			int cur_mins = hour * 60 + min; /* minutes past 5AM */
			int max_mins = RAMP_UP_DURATION * 60;

			float w;
			if (cur_mins > max_mins + KEEP_ON_DURATION * 60)
				w = 0.0f;
			else if (cur_mins > max_mins)
				w = 1.0f;
			else
				w = cur_mins / ((float)max_mins);

			float wg = 0.7 - ((1 - w) * 0.6f);
			float wb = 0.3 - ((1 - w) * 0.25f);
			printf("%d:%d:%d %.1f %.1f\n", hour, min, sec, wg, wb);
			color |= (int)round(w * 1.0f * 255) << 16;
			color |= (int)round(w * wg * 255) << 8;
			color |= (int)round(w * wg * 255);
		}
		hour = oh;

		for (int i = 0; i < led_count; i++)
			leds[i] = color;
		for (int i = 0; i < led_count; i++)
			ledstring.channel[0].leds[i] = leds[i];
		if ((ret = ws2811_render(&ledstring)) != WS2811_SUCCESS) {
			fprintf(stderr, "ws2811_render failed: %s\n",
			        ws2811_get_return_t_str(ret));
			break;
		}

		for (int i = 0; i < 1; i++)
			usleep(50000);  // usleep(1000000);
	}

	/* clear the leds */
	for (int i = 0; i < led_count; i++)
		leds[i] = 0;
	for (int i = 0; i < led_count; i++)
		ledstring.channel[0].leds[i] = leds[i];
	ws2811_render(&ledstring);

	/* finalize */
	free(leds);
	ws2811_fini(&ledstring);
	printf("\n");
	return ret;
}
