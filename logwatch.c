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
#include <sys/klog.h>
#include <sys/wait.h>
#include <signal.h>
#include <android/log.h>
#include <cutils/log.h>
#ifdef LOG_TAG
#undef LOG_TAG
#ifdef COLOR
#define LOG_TAG "\e[0;91mlogwatch--->logwatch\e[0m"
#else
#define LOG_TAG "logwatch--->logwatch"
#endif
#endif
#include "logwatch.h"
#include "configure.h"

const char* const logcat_priority[7] = {"S", "F", "E", "W", "I", "D", "V"};

static void dump_logwatch_data(struct logwatch_data* logwatch) {
	LOGD("===================================");
	LOGD("Dump logwatch data.");
	LOGD("enable logwatch: %d", logwatch->is_enable_logwatch);
	LOGD("boot delay: %d", logwatch->boot_delay);
	LOGD("system log path: %s", logwatch->log_path);
	LOGD("system log number: %d", logwatch->log_num);
	LOGD("enable kernel message: %d", logwatch->is_enable_kmsg);
	LOGD("kernle message size: %ld", logwatch->kmsg_size);
	LOGD("kernel priority: %d", logwatch->kmsg_prior);
	LOGD("enable logcat: %d", logwatch->is_enable_logcat);
	LOGD("logcat size: %ld", logwatch->logcat_size);
	LOGD("logcat priority: %d", logwatch->logcat_prior);
	LOGD("===================================");
}

static void msleep(long long msec) {
	struct timespec ts;
	int err;

	ts.tv_sec = (msec / 1000);
	ts.tv_nsec = (msec % 1000) * 1000 * 1000;

	do {
		err = nanosleep(&ts, &ts);
	} while (err < 0 && errno == EINTR);
}

static int delete_folder(const char *path)
{
	DIR *stream;
	struct dirent *entry;
	struct stat sb;

	stream = opendir(path);
	if (!stream) {
		LOGE("Failed to open folder %s: %s", path, strerror(errno));
		return -1;
	}
	chdir(path);
	while ((entry = readdir(stream)) != NULL) {
		lstat(entry->d_name, &sb);
		if (S_ISDIR(sb.st_mode)) {
			if(!strcmp(".", entry->d_name) || !strcmp("..", entry->d_name))
				continue;
			else
				delete_folder(entry->d_name);
		}
		unlink(entry->d_name);
	}

	chdir("..");
	closedir(stream);
	rmdir(path);
	return 0;
}

static int init_work(struct logwatch_data* logwatch) {
	int retval = 0;
	DIR* stream;
	struct dirent *entry;
	struct stat sb;
	char** folder_array = NULL;
	char** log_array = NULL;
	char** serial_char_array = NULL;
	unsigned long* serial_int_array = NULL;
	unsigned long folder_count = 0;
	unsigned long log_count = 0;
	unsigned long i = 0;
	unsigned long j = 0;
	unsigned long temp = 0;
	char *buf = NULL;
	struct tm *ptime = NULL;
	time_t the_time;
	char* start = NULL;

	folder_array = (char**)malloc(sizeof(char*) * (logwatch->log_num + 200));
	memset(folder_array, 0 , sizeof(char*) * (logwatch->log_num + 200));

	log_array = (char**)malloc(sizeof(char*) * (logwatch->log_num + 100));
	memset(log_array, 0 , sizeof(char*) * (logwatch->log_num + 100));

	serial_char_array = (char**)malloc(sizeof(char*) *
			(logwatch->log_num + 100));
	memset(serial_char_array, 0, sizeof(char*) * (logwatch->log_num + 100));

	serial_int_array = (unsigned long*)malloc(sizeof(unsigned long) *
			(logwatch->log_num + 100));
	memset(serial_int_array, 0, sizeof(unsigned long) *
			(logwatch->log_num + 100));

	/*
	 * create folder to save log
	 */
	chdir(logwatch->log_path);
	if (!stat(LOG_FOLDER, &sb) && S_ISDIR(sb.st_mode)) {
		LOGW("\"%s/ingenic-log\" already exist.", logwatch->log_path);
	} else {
		retval = mkdir("ingenic-log", S_IRUSR | S_IWUSR | S_IXUSR | S_IRGRP \
				 | S_IROTH);
		if (retval < 0) {
			LOGE("Failed to create system log folder: %s.", strerror(errno));
			return -1;
		}
	}

	/*
	 * scan ingenic-log to get all log name
	 */
	stream = opendir("ingenic-log");
	if (!stream) {
		LOGE("Failed to open folder %s: %s.", logwatch->log_path,
				strerror(errno));
		return -1;
	}

	while ((entry = readdir(stream)) != NULL) {
		lstat(entry->d_name, &sb);
		if (S_ISDIR(sb.st_mode)) {
			if (!strcmp(".", entry->d_name) || !strcmp("..", entry->d_name)) {
				continue;
			} else {
				/*
				 * Note:
				 * 		file name like: 1(2014-06-19 23:18:00)
				 * 		"1": log serial number
				 * 		"(2014-06-19 23:18:00)": system boot time
				 */
				folder_array[folder_count++] = strdup(entry->d_name);
				continue;
			}
		}
	}
	closedir(stream);

	chdir(LOG_FOLDER);

	/*
	 * delete unrecognized folder
	 */
	for (i = 0; i < folder_count; i++) {
		char* tmp_start = NULL;
		char* tmp_end = NULL;
		int tmp_length = 0;

		tmp_start = strchr(folder_array[i], '(');
		tmp_end = strrchr(folder_array[i], ')');
		tmp_length = strrchr(folder_array[i], ')') -
				strchr(folder_array[i], '(');

		if (tmp_start == NULL || tmp_end == NULL || tmp_length != 20) {
			LOGD("Unrecognized folder: %s\n", folder_array[i]);
			delete_folder(folder_array[i]);
		} else {
			log_array[log_count++] = strdup(folder_array[i]);
		}
	}

	for (i = 0; i < folder_count; i++) {
		if (folder_array[i]) {
			free(folder_array[i]);
		}
	}
	if (folder_array)
		free(folder_array);

#if DEBUG
	LOGD("===================================");
	LOGD("Dump exist log.");
	for (i = 0; i < log_count; i++)
		LOGD("%s\n", log_array[i]);
	LOGD("===================================");
#endif

	/*
	 * get serial number from log file name
	 */
	for (i = 0; i < log_count; i++) {
		/*
		 * Note:
		 * tmp_array used to save log serial number
		 */
		start = strchr(log_array[i], '(');

		serial_char_array[i] = (char*)malloc(sizeof(char) *
				(start - log_array[i] + 1));

		memset(serial_char_array[i], 0, sizeof(char) *
				(start - log_array[i] + 1));

		strncpy(serial_char_array[i], log_array[i],
				(unsigned int)(start - log_array[i]));

		serial_char_array[i][start - log_array[i]] = '\0';
		serial_int_array[i] = strtoul(serial_char_array[i], NULL, 10);
	}

	/*
	 * sort log accord serial number
	 */
	i = log_count;
	while (i > 0) {
		for (j = 0; j < log_count -1; j++) {
			if (serial_int_array[j] < serial_int_array[j + 1]) {
				temp = serial_int_array[j];
				serial_int_array[j] = serial_int_array[j + 1];
				serial_int_array[j + 1] = temp;
			}
		}
		i--;
	}

#if DEBUG
	LOGD("===================================");
	LOGD("Dump sorted log.");
	for (i = 0; i < log_count; i++) {
		asprintf(&buf, "%lu", serial_int_array[i]);
		for (j = 0; j < log_count; j++) {
			if (!strncmp(buf, log_array[j], strlen(serial_char_array[j])))
				LOGD("%s\n", log_array[j]);
		}
		free(buf);
	}
	LOGD("===================================");
#endif

	/*
	 * delete old log base serial number
	 */
	for (i = logwatch->log_num - 1; i < log_count; i++) {
		asprintf(&buf, "%lu", serial_int_array[i]);
		for (j = 0; j < log_count; j++) {
			if (!strncmp(buf, log_array[j], strlen(serial_char_array[j]))) {
#if DEBUG
				LOGD("delete old log: %s\n", log_array[j]);
#endif
				delete_folder(log_array[j]);
			}
		}
		free(buf);
	}

	/*
	 * create new log base current system time
	 */
	time(&the_time);
	ptime = localtime(&the_time);

	asprintf(&buf, "%lu(%04d-%02d-%02d %02d:%02d:%02d)", serial_int_array[0] + 1,
		ptime->tm_year + 1900, ptime->tm_mon + 1, ptime->tm_mday,
		ptime->tm_hour, ptime->tm_min, ptime->tm_sec);

	retval = mkdir(buf, S_IRUSR | S_IWUSR | S_IXUSR | S_IRGRP | S_IROTH);
	if (retval < 0) {
		LOGE("Failed to create log %s: %s.", buf, strerror(errno));
		return -1;
	}
	logwatch->cur_log_path = strdup(buf);

#if DEBUG
	LOGD("create new log: %s\n", logwatch->cur_log_path);
#endif

	free(buf);

	/*
	 * clean work
	 */
	for (i = 0; i < log_count; i++) {
		if (log_array[i])
			free(log_array[i]);

		if (serial_char_array[i])
			free(serial_char_array[i]);
	}

	if (log_array)
		free(log_array);

	if (serial_char_array)
		free(serial_char_array);

	if (serial_int_array)
		free(serial_int_array);

	return 0;
}

/*
static void parse_kmsg_to_nand(char* buf, int size, const int fd, const int prior) {
	char* start_line = NULL;
	char* end_line = NULL;
	char* end_buf;
	int count = 0;
	int retval = 0;

	start_line = buf;

	while (1) {
		start_line = strchr(start_line, '<');
		if (start_line[0] && (start_line[1] - '0' < 8)
				&& (start_line[1] - '0' > 0) && start_line[2] == '>') {

			end_line = strchr(start_line, '\n');

			if (end_line) {
				if (start_line[1] - '0' < prior) {
					count = end_line - start_line + 1;

					write(fd, start_line, count);

					start_line = ++end_line;

					end_line = strchr(start_line, '\0');
					if (end_line) {
						write(fd, start_line, size - count);
						break;
					}

				} else {
					start_line = ++end_line;
					continue;
				}
			} else {
				write(fd, start_line, size - count);
				break;
			}
		} else {
			start_line = ++end_line;
			if (start_line[0] == '\n' || start_line[0] == '\0')
				break;
			else
				continue;
		}

	}
	LOGD("parase kernel message end.");
}

static void* start_watch_kmsg(void* param) {
	int retval;
	char* buf;
	int klog_buf_len;
	int fifo_size;
	int count;


	struct logwatch_data* logwatch = (struct logwatch_data *)param;

	fifo_size = (1 << logwatch->kmsg_size);

	 set kernel log buffer size
	klog_buf_len = klogctl(KLOG_WRITE, 0, 0);
	if (klog_buf_len < 1) {
		klog_buf_len = fifo_size;
	}

	LOGD("kernel log buffer size: %d.", klog_buf_len);
	buf = (char *)malloc(sizeof(char) * klog_buf_len);
	memset(buf, 0, sizeof(char) * klog_buf_len);


	for (;;) {
		 read some log to bufffer
		count = klogctl(KLOG_READ, buf, klog_buf_len);
		if (count < 0) {
			LOGE("Failed to read kernel message,tyr again...");
			continue;
		}
		buf[count] = '\0';
		LOGD("size = %d", count);
		LOGD("buffer:\n%s", buf);

		parse_kmsg_to_nand(buf, count, logwatch->kmsg_fd, logwatch->kmsg_prior);
	}
	return NULL;
}
*/

static void* start_watch_kmsg(void* param) {
	int retval;
	int read_count;
	char rw_buf[1024 * 10] = {0};
	char* buf;
	int src_fd;
	int print_fd;

	struct logwatch_data* logwatch = (struct logwatch_data *)param;

	/*
	 * set kernel printk output level
	 */
	print_fd = open("/proc/sys/kernel/printk", O_RDWR | O_SYNC);
	if (print_fd < 0) {
		LOGE( "Failed to open /proc/sys/kernel/printk: %s.", strerror(errno));
		//return NULL; just use default
	} else {
	    asprintf(&buf, "%d\n", logwatch->kmsg_prior);
	    retval = write(print_fd, buf, 2);
	    if (retval < 0)
	        LOGE("Failed to write /proc/sys/kernel/printk: %s.", strerror(errno));

	    close(print_fd);

	    free(buf);
	}

	src_fd = open("/proc/kmsg", O_RDONLY);
	if (src_fd < 0) {
		LOGE("Failed to open /proc/kmsg: %s.", strerror(errno));
		return NULL;
	}

	for (;;) {
		read_count = read(src_fd, rw_buf, sizeof(rw_buf));
		if (read_count < 0) {
			LOGE("Failed to read /proc/kmsg: %s.", strerror(errno));
			break;
		}

		retval = write(logwatch->kmsg_fd, rw_buf, read_count);
		if (retval < 0) {
			LOGE("Failed to write kmsg: %s.", strerror(errno));
			break;
		}
	}
	close(src_fd);
	return NULL;
}

static void* start_watch_logcat(void *param) {
	struct logwatch_data* logwatch = (struct logwatch_data *)param;
	char* buf;

	asprintf(&buf, "logcat -v time *:%s > logcat",
			logcat_priority[logwatch->logcat_prior]);

	for(;;) {
		if (system(buf) < 0) {
			LOGE("Failed to invoke logcat.");
			break;
		}
		msleep(100);
	}
	free(buf);
	return NULL;
}

static int do_work(struct logwatch_data* logwatch) {
	int retval = 0;

	retval = pthread_attr_init(&logwatch->attr);
	if (retval) {
		LOGE("Failed to initialize pthread attr.");
		return -1;
	}

	retval = pthread_attr_setdetachstate(&logwatch->attr,
			PTHREAD_CREATE_DETACHED);
	if (retval) {
		LOGE("Failed to set pthread attr.");
		return -1;
	}

	chdir(logwatch->cur_log_path);
	if (logwatch->is_enable_kmsg) {
		logwatch->kmsg_fd = open("kmsg", O_RDWR | O_SYNC | O_CREAT | O_APPEND,
				S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH);
		if (logwatch->kmsg_fd < 0) {
			LOGE("Failed to open kmsg: %s.", strerror(errno));
			return -1;
		}

		retval = pthread_create(&logwatch->kmsg_pid, &logwatch->attr, start_watch_kmsg, (void *)logwatch);
		if (retval < 0) {
			LOGE("Failed to create thread to watch kmsg.");
			return -1;
		}
	}

	if (logwatch->is_enable_logcat) {
		logwatch->logcat_fd = open("logcat", O_RDWR | O_SYNC | O_CREAT | O_APPEND,
				S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH);
		if (logwatch->logcat_fd < 0) {
			LOGE("Failed to open logcat: %s.", strerror(errno));
			return -1;
		}
		retval = pthread_create(&logwatch->logcat_pid, &logwatch->attr,
				start_watch_logcat, (void *)logwatch);
		if (retval < 0) {
			LOGE("Failed to create thread to watch logcat.");
			return -1;
		}
	}

	return 0;
}

static void signal_handler(int signum) {
	int errno_save = errno;
//TODO:fix me
	abort();
	errno = errno_save;
}

static void register_signal_handler() {
	struct sigaction action, old_action;

	action.sa_handler = signal_handler;
	sigemptyset(&action.sa_mask);
	action.sa_flags = 0;

	sigaction(SIGINT, NULL, &old_action);
	if (old_action.sa_handler != SIG_IGN)
	    sigaction(SIGINT, &action, NULL);
}

int main (int argc, char* argv[])
{
	int retval;

	struct logwatch_data *logwatch_data;

	logwatch_data = (struct logwatch_data *)malloc(sizeof(struct logwatch_data));

	register_signal_handler();

	load_configure("/system/etc/logwatch.conf", logwatch_data);

#ifdef DEBUG
	//dump_logwatch_data(logwatch_data);
#endif

	retval = init_work(logwatch_data);
	if (retval < 0) {
		LOGE("Failed to init work...Abort.");
		abort();
	}

	retval = do_work(logwatch_data);
	if (retval < 0) {
		LOGE("Faild to do_work...Abort.");
		abort();
	}

	LOGW("\e[0;91mI'm ready...Let's go!\e[0m");

	while (1) {
		sleep(1000);
	}
	return 0;
}
