#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <libconfig.h>

#include "config.h"

int try_load_config(alarm_config_t *acfg) {
	config_t cfg;
	int val, ret;

	const char *cfg_fname = "/etc/led-alarm.conf";

	memset(acfg, '\0', sizeof(alarm_config_t));

	printf("Loading config from %s\n", cfg_fname);

	config_init(&cfg);

	if(config_read_file(&cfg, cfg_fname) == CONFIG_TRUE) {
		config_lookup_int(&cfg, "test", &val);
		printf("Loaded config from %s, test=%d\n", cfg_fname, val);
		ret = 0;
	} else {
		fprintf(stderr, "Failed to load config from %s\nError: %s\n",
		                cfg_fname, config_error_text(&cfg));
		ret = 1;
	}

	config_destroy(&cfg);
	return ret;
}
