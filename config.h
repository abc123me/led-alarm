#ifndef _CONFIG_H
#define _CONFIG_H

#include <stdio.h>

/* Normal overrides */
#define CFG_OVERRIDE_TIME      0x0001 /* When set alarm time is set to this until the time is reached */
#define CFG_OVERRIDE_COLOR     0x0002 /* When set color is set to override_color */
#define CFG_OVERRIDE_FAKE      0x0004 /* When set time can be faked via the fake-time config */
#define CFG_OVERRIDE_MASK      0x00FF
/* Weekday overrides                     When these are set, the begin time for that day is overridden */
#define CFG_OVERRIDE_WEEKDAY_0 0x0100
#define CFG_OVERRIDE_SUNDAY    0x0100
#define CFG_OVERRIDE_MONDAY    0x0200
#define CFG_OVERRIDE_TUESDAY   0x0400
#define CFG_OVERRIDE_WEDNESDAY 0x0800
#define CFG_OVERRIDE_THURSDAY  0x1000
#define CFG_OVERRIDE_FRIDAY    0x2000
#define CFG_OVERRIDE_SATURDAY  0x4000
#define CFG_OVERRIDE_DAY_MASK  0xFF00

/* libconfig types */
#define CFG_TYPE_NONE          0x0000
#define CFG_TYPE_INT           0x0001
#define CFG_TYPE_INT64         0x0002
#define CFG_TYPE_FLOAT         0x0003
#define CFG_TYPE_BOOL          0x0004
#define CFG_TYPE_STRING        0x0005
#define CFG_TYPE_SETTING       0x0006
#define CFG_TYPE_LIBCFG_MASK   0x000F
/* our "special" types */
#define CFG_TYPE_TMPSTR        0x0010
#define CFG_TYPE_TIME_TYPE     0x0020
#define CFG_TYPE_TIME          0x0001 /* 0x1035 */
#define CFG_TYPE_DURATION      0x0001 /* 0x2035 */
#define CFG_TYPE_DAY           0x0001 /* todo */
#define CFG_TYPE_COLOR         0x0001 /* todo */

/* TODO */
#define NOISE_TYPE_NONE        0
#define NOISE_TYPE_RANDOM      1
#define NOISE_TYPE_SINE        2
#define NOISE_TYPE_CLOUDS      3

struct alarm_config_t {
	/* Time settings */
	int begin_time;              /* Begin time, if one for the day isn't specified */
	int begin_times[7];          /* Begin time per week day, if in use set flag in overrides */
	int ramp_up_time;            /* Ramp up time in minutes */
	int keep_on_time;            /* Keep on time in minutes */

	/* LED strip settings */
	int brightness;              /* Overall brightness from 0-255 */
	int noise_type;              /* Sets a "noise_type" when the strip is in normal operation */
	int noise_intensity;         /* Sets the intensity of the above noise type */
	int line_fade;               /* Sets an inverted intensity curve to handle voltage sag */

	/* Overrides */
	int overrides;               /* Which overrides are in use, zero for none */
	int override_time;           /* Override start time, this config will be erased after the time passes */
	ws2811_led_t override_color; /* When set to non-zero the color is forced to this state */

	/* Debug features */
	int fake_time;
	int fake_day;
	int verbosity;
};

typedef struct alarm_config_t alarm_config_t;

int load_alarm_config(alarm_config_t*, char*);
void print_config(alarm_config_t*, FILE*);
int get_begin_time(alarm_config_t*, int);

#endif
