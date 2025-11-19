#include <errno.h>
#include <getopt.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "ws2811.h"

int parseargs(int argc, char** argv, ws2811_t* ws2811) {
	int tmp, index, c = 0;
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
				return EINVAL;
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
					tmp = atoi(optarg);
					if (tmp < 14) {
						ws2811->dmanum = tmp;
					} else {
						fprintf(stderr, "Invalid dma %d\n", tmp);
						return EINVAL;
					}
				}
				break;
			case 'c':
				if (optarg) {
					tmp = atoi(optarg);
					if (tmp > 0) {
						ws2811->channel[0].count = tmp;
					} else {
						fprintf(stderr, "Invalid led count %d\n", tmp);
						return EINVAL;
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
						fprintf(stderr, "Invalid strip %s\n", optarg);
						return EINVAL;
					}
				}
				break;
			case '?':
				/* getopt_long already reported error? */
				return -1;
			default:
				return -1;
		}
	}
	return 0;
}
