#ifndef _CONFIG_H
#define _CONFIG_H

struct alarm_config_t {
	int begin_times[7]; /* Begin time, One for every day of week, 0 = Sunday */
	int ramp_up_time;   /* Ramp up time in minutes */
	int keep_on_time;   /* Keep on time in minutes */
	int brightness;     /* Overall brightness from 0-255 */
	int override_color; /* When set to non-zero the color is forced to this state */
};

typedef struct alarm_config_t alarm_config_t;

int try_load_config(alarm_config_t* cfg);

#endif
