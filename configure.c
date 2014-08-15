/*
 *  Copyright (C) 2013, Zhang YanMing <jamincheung@126.com>
 *
 *  ingenic log record tool
 *
 *  This program is free software; you can redistribute it and/or modify it
 *  under  the terms of the GNU General  Public License as published by the
 *  Free Software Foundation;  either version 2 of the License, or (at your
 *  option) any later version.
 *
 *  You should have received a copy of the GNU General Public License along
 *  with this program; if not, write to the Free Software Foundation, Inc.,
 *  675 Mass Ave, Cambridge, MA 02139, USA.
 *
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>
#include <errno.h>
#include <sys/stat.h>
#include <dirent.h>
#include <sys/types.h>
#include <fcntl.h>
#include <pthread.h>
#include <android/log.h>
#include <cutils/log.h>
#ifdef LOG_TAG
#undef LOG_TAG
#ifdef COLOR
#define LOG_TAG "\e[0;91mlogwatch--->configure\e[0m"
#else
#define LOG_TAG "logwatch--->configure"
#endif
#endif
#include "logwatch.h"

static void dump_config(struct config *config) {
	LOGD("===================================");
	LOGD("Dump config.");
	LOGD("%s: %s", config->misc->enable.name, config->misc->enable.value);
	LOGD("%s: %s", config->misc->log_path.name, config->misc->log_path.value);
	LOGD("%s: %s", config->misc->log_num.name, config->misc->log_num.value);
	LOGD("%s: %s", config->kmsg->is_enable.name, config->kmsg->is_enable.value);
	LOGD("%s: %s", config->kmsg->prior.name, config->kmsg->prior.value);
	LOGD("%s: %s", config->kmsg->fifo_size.name, config->kmsg->fifo_size.value);
	LOGD("%s: %s", config->logcat->is_enable.name, config->logcat->is_enable.value);
	LOGD("%s: %s", config->logcat->fifo_size.name, config->logcat->fifo_size.value);
	LOGD("%s: %s", config->logcat->prior.name, config->logcat->prior.value);
	LOGD("===================================");
}

static char* get_line (char *s, int size, FILE* stream, int* line, char** _pos) {
	char *pos = NULL;
	char *end = NULL;
	char *sstart = NULL;

	while (fgets(s, size, stream)) {
		(*line)++;
		s[size - 1] = '\0';
		pos = s;

		/* Skip white space from the beginning of line. */
		while (*pos == ' ' || *pos == '\t' || *pos == '\r')
			pos++;

		/* Skip comment lines and empty lines */
		if (*pos == '#' || *pos == '\n' || *pos == '\0')
			continue;

		/*
		 * Remove # comments unless they are within a double quoted
		 * string.
		 */
		sstart = strchr(pos, '"');
		if (sstart)
			sstart = strrchr(sstart + 1, '"');
		if (!sstart)
			sstart = pos;
		end = strchr(sstart, '#');
		if (end)
			*end-- = '\0';
		else
			end = pos + strlen(pos) - 1;

		/* Remove trailing white space. */
		while (end > pos && (*end == '\n' || *end == ' ' || *end == '\t'
				|| *end == '\r'))
			*end-- = '\0';

		if (*pos == '\0')
			continue;

		if (_pos)
			*_pos = pos;

#ifdef DEBUG
		LOGD("Line %d: %s", *line, *_pos);
#endif

		return pos;
	}

	if (_pos)
		*_pos = NULL;
	return NULL;
}

const char* get_value(const char* line) {
	char* sstart = strchr(line, '=');
	if (sstart)
		return sstart[1] == '\0' ? NULL : sstart + 1;
	else
		return NULL;
}

static struct misc* read_misc_config(FILE* stream, int* line) {
	struct misc *config = NULL;
	struct stat sb;
	int permit = 0;
	char buf[256] = {0};
	char* pos = NULL;
	int errors = 0;
	int end = 0;

	char* enable = NULL;
	char* log_path = NULL;
	char* log_num = NULL;

	while(get_line(buf, sizeof(buf), stream, line, &pos)) {
		if (!strcmp(pos, "}")) {
			end = 1;
			break;
		} else if (!strncmp(pos, "enable=", 7)) {
			enable = strdup(get_value(pos));
			if (!enable) {
				LOGE("Failed to parase line: %d: enable=?", *line);
				errors++;
				break;
			}
		} else if (!strncmp(pos, "log_path=", 9)) {
			log_path = strdup(get_value(pos));
			if (!log_path) {
				LOGE("Failed to parse line: %d: log_path=?", *line);
				errors++;
				break;
			}
		} else if (!strncmp(pos, "log_num=", 8)) {
			log_num = strdup(get_value(pos));
			if (!log_num) {
				LOGE("Failed to parse line: %d: log_num=?", *line);
				errors++;
				break;
			}
		} else {
			LOGE("Failed to parse Line: %d: Unrecognized line.", *line);
			errors++;
			break;
		}
	}

	if (!end) {
		LOGE( "Failed to parse Line: %d: lost \"}\".", *line);
		goto error;
	}

	if (errors)
		goto error;

	if (lstat(log_path, &sb)) {
    	LOGE("Failed to get info about \"%s\": %s.", log_path, strerror(errno));
		goto error;
	}

    if (access(log_path, W_OK)) {
    	LOGE("Failed to access \"%s\": %s.", log_path, strerror(errno));
    	goto error;
    }

	if (atol(log_num) < 0) {
		LOGE("Invalid argument.");
		goto error;
	}

	config = (struct misc *)malloc(sizeof(struct misc));
	config->enable.name = strdup("Enable logwatch");
	config->enable.value = enable;

	config->log_path.name = strdup("Log path");
	config->log_path.value = log_path;

	config->log_num.name = strdup("Log max number");
	config->log_num.value = log_num;
	return config;

error:
	if (enable)
		free(enable);
	if (log_path)
		free(log_path);
	if (log_num)
		free(log_num);
	return NULL;
}

static struct kmsg* read_kmsg_config(FILE* stream, int* line) {
	struct kmsg *config = NULL;
	char buf[256] = {0};
	char* pos = NULL;
	int errors = 0;
	int end = 0;

	char* is_enable = NULL;
	char* fifo_size = NULL;
	char* prior = NULL;

	while(get_line(buf, sizeof(buf), stream, line, &pos)) {
		if (!strcmp(pos, "}")) {
			end = 1;
			break;
		} else if (!strncmp(pos, "is_enable=", 10)) {
			is_enable = strdup(get_value(pos));
			if (!is_enable) {
				LOGE("Failed to parse line: %d: is_enable=?", *line);
				errors++;
				break;
			}
		} else if (!strncmp(pos, "fifo_size=", 10)) {
			fifo_size = strdup(get_value(pos));
			if (!fifo_size) {
				LOGE("Failed to parse line: %d: fifo_size=?", *line);
				errors++;
				break;
			}
		} else if (!strncmp(pos, "prior=", 6)) {
			prior = strdup(get_value(pos));
			if (!prior) {
				LOGE("Failed to parse line: %d: prior=?", *line);
				errors++;
				break;
			}
		} else {
			LOGE("Failed to parse Line: %d: Unrecognized line.", *line);
			errors++;
			break;
		}
	}

	if (!end) {
		LOGE("Failed to parse Line: %d: lost \"}\".", *line);
		goto error;
	}

	if (errors)
		goto error;

	if ((strcmp(is_enable, "yes") && strcmp(is_enable, "no"))
			|| (atol(fifo_size) < 0)
			|| (atol(prior) < 0 || atol(prior) > 8)) {
		LOGE("Invalid argument.");
		goto error;
	}


	config = (struct kmsg *)malloc(sizeof(struct kmsg));
	config->is_enable.name = strdup("Enable kernel log");
	config->is_enable.value = is_enable;

	config->fifo_size.name = strdup("Kernel log buffer size");
	config->fifo_size.value = fifo_size;

	config->prior.name = strdup("Kernel log output level");
	config->prior.value = prior;
	return config;

error:
	if (is_enable)
		free(is_enable);
	if (fifo_size)
		free(fifo_size);
	if (prior)
		free(prior);
	return NULL;

}

static struct logcat* read_logcat_config(FILE* stream, int* line) {
	struct logcat *config = NULL;
	char buf[256] = {0};
	char* pos = NULL;
	int errors = 0;
	int end = 0;

	char* is_enable = NULL;
	char* fifo_size = NULL;
	char* prior = NULL;

	while(get_line(buf, sizeof(buf), stream, line, &pos)) {
		if (!strcmp(pos, "}")) {
			end = 1;
			break;
		} else if (!strncmp(pos, "is_enable=", 10)) {
			is_enable = strdup(get_value(pos));
			if (!is_enable) {
				LOGE("Failed to parse line: %d: is_enable_=?", *line);
				errors++;
				break;
			}
		} else if (!strncmp(pos, "fifo_size=", 10)) {
			fifo_size = strdup(get_value(pos));
			if (!fifo_size) {
				LOGE("Failed to parse line: %d: fifo_size=?", *line);
				errors++;
				break;
			}
		} else if (!strncmp(pos, "prior=", 6)) {
			prior = strdup(get_value(pos));
			if (!prior) {
				LOGE("Failed to parse line: %d: prior=?", *line);
				errors++;
				break;
			}
		} else {
			LOGE("Failed to parse Line: %d: Unrecognized line.", *line);
			errors++;
			break;
		}
	}

	if (!end) {
		LOGE("Failed to parse Line: %d: lost \"}\".", *line);
		goto error;
	}

	if (errors)
		goto error;

	if ((strcmp(is_enable, "yes") && strcmp(is_enable, "no"))
			|| (atol(fifo_size) < 0)
			|| (atol(prior) < 0 || atol(prior) > 6)) {
		LOGE("Invalid argument.");
		goto error;
	}

	config = (struct logcat *)malloc(sizeof(struct logcat));
	config->is_enable.name = strdup("Enable logcat");
	config->is_enable.value = is_enable;

	config->fifo_size.name = strdup("Logcat log buffer size");
	config->fifo_size.value = fifo_size;

	config->prior.name = strdup("Logcat log output level");
	config->prior.value = prior;
	return config;

error:
	if (is_enable)
		free(is_enable);
	if (fifo_size)
		free(fifo_size);
	if (prior)
		free(prior);
	return NULL;
}

static void install_config(struct config* config, struct logwatch_data* logwatch) {
	/* install misc configs */
	if (!strcmp(config->misc->enable.value, "yes"))
		logwatch->is_enable_logwatch = 1;
	else
		logwatch->is_enable_logwatch = 0;

	logwatch->log_path = config->misc->log_path.value;
	logwatch->log_num = atol(config->misc->log_num.value);

	/* install kmsg configs */
	if (!strcmp(config->kmsg->is_enable.value, "yes"))
		logwatch->is_enable_kmsg = 1;
	else
		logwatch->is_enable_kmsg = 0;

	logwatch->kmsg_size = atol(config->kmsg->fifo_size.value);
	logwatch->kmsg_prior = atol(config->kmsg->prior.value);

	/* install logcat configs*/
	if (!strcmp(config->logcat->is_enable.value, "yes"))
		logwatch->is_enable_logcat = 1;
	else
		logwatch->is_enable_logcat = 0;

	logwatch->logcat_size = atol(config->logcat->fifo_size.value);
	logwatch->logcat_prior = atoi(config->logcat->prior.value);
}


static void install_default_config(struct logwatch_data* logwatch) {
	LOGW("Installing default configuration.");

	logwatch->is_enable_logwatch = LOGWATCH_ENABLE_DEF;

	logwatch->log_path = strdup(LOG_PATH_DEF);
	logwatch->log_num = LOG_NUM_DEF;

	logwatch->is_enable_kmsg = KMSG_ENABLE_DEF;
	logwatch->kmsg_size = KMSG_SIZE_DEF;
	logwatch->kmsg_prior = KMSG_PRIOR_DEF;

	logwatch->is_enable_logcat = LOGCAT_ENABLE_DEF;
	logwatch->logcat_size = LOGCAT_SIZE_DEF;
	logwatch->logcat_prior = LOGCAT_PRIOR_DEF;
}


static int parse_configure(FILE * stream, struct config *config) {
	char buf[256] = {0};
	char *pos = NULL;
	int line = 0;
	int errors = 0;

	while (get_line(buf, sizeof(buf), stream, &line, &pos)) {
		if (!strcmp(pos, "misc={")) {
			config->misc = read_misc_config(stream, &line);
			if (!config->misc) {
				LOGE("Failed to read misc configure.");
				errors++;
				break;
			}
		} else if (!strcmp(pos, "kmsg={")) {
			config->kmsg = read_kmsg_config(stream, &line);
			if (!config->kmsg) {
				LOGE("Failed to read kmsg configure.");
				errors++;
				break;
			}
		} else if (!strcmp(pos, "logcat={")) {
			config->logcat = read_logcat_config(stream, &line);
			if (!config->logcat) {
				LOGE("Failed to read logcat configure.");
				errors++;
				break;
			}
		} else {
			LOGE("Failed to parse line: %d: Unrecognized line.", line);
			break;
		}
	}

	if (errors)
		return -1;

	return 0;
}

static FILE* open_file(const char* file_name) {
	if (!file_name)
		return NULL;

	return fopen(file_name, "r");
}

int load_configure(const char* file_name, struct logwatch_data* logwatch) {
	FILE* stream = NULL;
	int retval = 0;
	struct config *config = NULL;

	config = (struct config *)malloc(sizeof(struct config));
	memset(config, 0, sizeof(struct config));

	if (!(stream = open_file(file_name))) {
		LOGE("Failed to open file: %s :%s.\n", file_name, strerror(errno));
		goto error;
	}

	retval = parse_configure(stream, config);
	if (retval < 0) {
		LOGE("Failed to parse configuration.");
		goto error;
	}

#ifdef DEBUG
	dump_config(config);
#endif

	install_config(config, logwatch);

	if (config)
		free(config);

	return fclose(stream);

error:
	if (config)
		free(config);
	install_default_config(logwatch);
	if (stream)
		return fclose(stream);
	else
		return -1;
}
