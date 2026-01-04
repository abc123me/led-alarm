#include <stdio.h>
#include <stdlib.h>
#include <stddef.h>
#include <string.h>

#include <math.h>

#include <libconfig.h>

#include "config.h"

static int time_from_str(const char*, int*, int);

int load_alarm_config(alarm_config_t *acfg, char* cfg_fname) {
	config_t cfg;
	int val, ret;

	ret = CONFIG_FALSE;

	memset(acfg, '\0', sizeof(alarm_config_t));

	printf("Loading config from %s\n", cfg_fname);

	config_init(&cfg);

	if(config_read_file(&cfg, cfg_fname) == CONFIG_TRUE) {
		void   *acfgs[] = { &acfg->begin_time,     &acfg->ramp_up_time,   &acfg->keep_on_time,   &acfg->brightness,     &acfg->override_time,  &acfg->override_color,
		                    &acfg->begin_times[0], &acfg->begin_times[1], &acfg->begin_times[2], &acfg->begin_times[3], &acfg->begin_times[4], &acfg->begin_times[5], &acfg->begin_times[6],
		                    &acfg->fake_time,      &acfg->fake_day,       &acfg->verbosity,      &acfg->noise_type,     &acfg->noise_intensity, NULL };
		char   *names[] = { "normal-time",         "ramp-up-time",        "keep-on-time",        "brightness",          "override-time",       "override-color",
		                    "sunday-time",         "monday-time",         "tuesday-time",        "wednesday-time",      "thursday-time",       "friday-time",         "saturday-time",
		                    "fake-time",           "fake-day",            "verbosity",           "noise-type",          "noise-intensity",     NULL };
		int     types[] = { CFG_TYPE_TIME,         CFG_TYPE_INT,          CFG_TYPE_INT,          CFG_TYPE_INT,          CFG_TYPE_TIME,         CFG_TYPE_COLOR,
		                    CFG_TYPE_TIME,         CFG_TYPE_TIME,         CFG_TYPE_TIME,         CFG_TYPE_TIME,         CFG_TYPE_TIME,         CFG_TYPE_TIME,         CFG_TYPE_TIME,
		                    CFG_TYPE_DURATION,     CFG_TYPE_DAY,          CFG_TYPE_INT,          CFG_TYPE_INT,          CFG_TYPE_INT,          0 };
		int overrides[] = { 0,                     0,                     0,                     0,                     CFG_OVERRIDE_TIME,     CFG_OVERRIDE_COLOR,
                            CFG_OVERRIDE_SUNDAY,   CFG_OVERRIDE_MONDAY,   CFG_OVERRIDE_TUESDAY,  CFG_OVERRIDE_WEDNESDAY,CFG_OVERRIDE_THURSDAY, CFG_OVERRIDE_FRIDAY,   CFG_OVERRIDE_SATURDAY,
		                    CFG_OVERRIDE_FAKE,     0,                     0,                     0,                     0,                     0 };

		int *status = (int*) alloca(sizeof(overrides));
		const char *tmp_str = NULL;

		for(int i = 0; acfgs[i]; i++) {
			status[i] = -1;
			/* grab config */
			switch(types[i] & CFG_TYPE_LIBCFG_MASK) {
				case CFG_TYPE_INT:
					status[i] = config_lookup_int(&cfg, names[i], (int*) acfgs[i]);
					break;
				case CFG_TYPE_INT64:
					status[i] = config_lookup_int64(&cfg, names[i], (int64_t*) acfgs[i]);
					break;
				case CFG_TYPE_FLOAT:
					status[i] = config_lookup_float(&cfg, names[i], (double*) acfgs[i]);
					break;
				case CFG_TYPE_BOOL:
					status[i] = config_lookup_bool(&cfg, names[i], (int*) acfgs[i]);
					break;
				case CFG_TYPE_STRING:
					if(types[i] & CFG_TYPE_TMPSTR)
						status[i] = config_lookup_string(&cfg, names[i], &tmp_str);
					else
						status[i] = config_lookup_string(&cfg, names[i], (const char**) acfgs[i]);
					break;
				case CFG_TYPE_SETTING:
					config_setting_t *ptr = config_lookup(&cfg, names[i]);
					*((config_setting_t**) acfgs[i]) = ptr;
					status[i] = ptr ? CONFIG_TRUE : CONFIG_FALSE;
					break;
				default:
					ret = -1;
					fprintf(stderr, "\e[1;31mInvalid config type in config array, bailing from try_load_config!\e[0m\n");
					goto gtfo;
			}
			/* print status */
			switch(status[i]) {
				case CONFIG_TRUE:
					if((types[i] & CFG_TYPE_TIME_TYPE) && tmp_str && (status[i] = time_from_str(tmp_str, &val, types[i])) == CONFIG_TRUE)
						*((int*) acfgs[i]) = val;
					acfg->overrides |= overrides[i];
					//printf("\e[1;32mSuccessfully loaded config: %s!\e[0m\n", names[i]);
					break;
				case CONFIG_FALSE:
					//printf("\e[1;33mFailed to find and load config: %s (using default)!\e[0m\n", names[i]);
					break;
				default:
					printf("\e[1;31mUnknown status when loading config: %s!\e[0m\n", names[i]);
					goto gtfo;
			}
			/* sometimes, I wonder if I have peaked, then I write code like this */
		}
		printf("Config file loaded!\n");
		ret = CONFIG_TRUE;
	} else {
		fprintf(stderr, "\e[1;31mFailed to load config from %s\nError: %s\e[0m\n",
		                cfg_fname, config_error_text(&cfg));
		ret = CONFIG_FALSE;
	}

gtfo:
	config_destroy(&cfg);
	return ret;
}

static int time_from_str(const char* str, int* val, int type) {
	char* tstr = "err";
	int tmp;

	puts(str);

	if(type == CFG_TYPE_TIME) {
		tstr = "time";
		tmp = atoi(str);
	} else if(type == CFG_TYPE_DURATION) {
		tstr = "duration";
		tmp = atoi(str);
		if(tmp == 0) {
			fprintf(stderr, "\e[1;31mFailed to parse %s, zero is not valid, unset for default!\e[0m\n", tstr);
			return CONFIG_FALSE;
		}
	} else {
		fprintf(stderr, "\e[1;31mFailed to parse time/duration from string, unknown type (0x%08X)!\e[0m\n", type);
		return CONFIG_FALSE;
	}

	if(tmp > 1440) {
		fprintf(stderr, "\e[1;31mFailed to parse %s, Cannot be greater then a day!\e[0m\n", tstr);
		return CONFIG_FALSE;
	}
	if(tmp < 0) {
		fprintf(stderr, "\e[1;31mFailed to parse %s, Negative values are not valid!\e[0m\n", tstr);
		return CONFIG_FALSE;
	}

	*val = tmp;
	return CONFIG_TRUE;
}

void print_config(alarm_config_t* acfg, FILE* fp) {
	int overrides = acfg->overrides;
	char *days[] = { "Sun", "Mon", "Tues", "Wednes", "Thurs", "Fri", "Satur", NULL };

	fprintf(fp, "Alarm configuration:\n");
	fprintf(fp, "  Begin time:   %d:%d\n", acfg->begin_time / 60, acfg->begin_time % 60);
	fprintf(fp, "  Ramp up time: %d minutes\n", acfg->ramp_up_time);
	fprintf(fp, "  Keep on time: %d minutes\n", acfg->keep_on_time);
	fprintf(fp, "  Brightness:   %d%%\n", round(100.0f * (acfg->brightness / 255.0f)));
	fprintf(fp, "  Overrides:    0x%04X\n", overrides);
	fprintf(fp, "  Noise type:   %d @ %d\n", acfg->noise_type, acfg->noise_intensity);

	if(overrides & CFG_OVERRIDE_TIME)
		fprintf(fp, "  Custom time:  0\n", acfg->override_time);
	else
		fprintf(fp, "  Custom time:  N/A\n");

	if(overrides & CFG_OVERRIDE_COLOR)
		fprintf(fp, "  Custom color: #%06X\n", acfg->override_color);
	else
		fprintf(fp, "  Custom color: N/A\n");

	int bit = CFG_OVERRIDE_WEEKDAY_0;
	for (int i = 0; days[i]; i++) {
		if(overrides & bit)
			fprintf(fp, "  %sday time: %d:%d\n", days[i], acfg->begin_times[i] / 60, acfg->begin_times[i] % 60);
		else
			fprintf(fp, "  %sday time: N/A\n", days[i]);
		bit <<= 1;
	}
}

int get_begin_time(alarm_config_t *cfg, int day) {
	/* apply the global begin_time override */
	if(cfg->overrides & CFG_OVERRIDE_TIME)
		return cfg->override_time;

	/* apply begin_time overrides for the current day */
	if(cfg->overrides & CFG_OVERRIDE_DAY_MASK)
		if(cfg->overrides & (CFG_OVERRIDE_WEEKDAY_0 << day))
			return cfg->begin_times[day];

	return cfg->begin_time;
}
