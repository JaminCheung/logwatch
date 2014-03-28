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

#include "logwatch.h"
#include "configure.h"

#include <cutils/log.h>
#ifdef LOG_TAG
#undef LOG_TAG
#define LOG_TAG "logwatch--->logwatch"
#endif

const char* const logcat_priority[7] = {"S", "F", "E", "W", "I", "D", "V"};

static void dump_logwatch_data(struct logwatch_data* logwatch) {
	LOGD("===================================");
	LOGD("Dump logwatch data.");
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
	char* log_path = logwatch->log_path;
	int *log_array = NULL;
	int log_count = 0;
	int i = 0;
	int j = 0;
	int temp = 0;
	char *buf = NULL;

	log_array = (int *)malloc(sizeof(int) * logwatch->log_num);
	memset(log_array, 0 , sizeof(int) * logwatch->log_num);

	/* create system log folder */
	chdir(log_path);
	if (!stat("ingenic-log", &sb) && S_ISDIR(sb.st_mode)) {
		LOGW("\"%s/ingenic-log\" already exist.", logwatch->log_path);
	} else {
		retval = mkdir("ingenic-log", S_IRUSR | S_IWUSR | S_IXUSR | S_IRGRP | S_IWGRP \
				 | S_IXGRP | S_IROTH | S_IWOTH | S_IXOTH);
		if (retval < 0) {
			LOGE("Failed to create system log folder: %s.", strerror(errno));
			return -1;
		}
	}

	/* delete system log */
	stream = opendir("ingenic-log");
	if (!stream) {
		LOGE("Failed to open folder %s: %s.", log_path, strerror(errno));
		return -1;
	}

	while ((entry = readdir(stream)) != NULL) {
		lstat(entry->d_name, &sb);
		if (S_ISDIR(sb.st_mode)) {
			if (!strcmp(".", entry->d_name) || !strcmp("..", entry->d_name)) {
				continue;
			} else {
				log_array[log_count++] = atoi(entry->d_name);
				continue;
			}
		}
	}
	closedir(stream);
#if DEBUG
	LOGD("===================================");
	LOGD("Dump exist log.");
	for (i = 0; i < log_count; i++)
		LOGD("%d\n", log_array[i]);
	LOGD("===================================");
#endif
	i = log_count;
	while (i > 0) {
		for (j = 0; j < log_count -1; j++) {
			if (log_array[j] < log_array[j + 1]) {
				temp = log_array[j];
				log_array[j] = log_array[j + 1];
				log_array[j + 1] = temp;
			}
		}
		i--;
	}
#if DEBUG
	LOGD("===================================");
	LOGD("Dump sorted log.");
	for (i = 0; i < log_count; i++)
		LOGD("%d\n", log_array[i]);
	LOGD("===================================");
#endif
	chdir("ingenic-log");
	for (i = logwatch->log_num - 1; i < log_count; i++) {
		asprintf(&buf, "%d", log_array[i]);
		delete_folder(buf);
		free(buf);
	}

	/* create new system log */
	asprintf(&buf, "%d", log_array[0] + 1);
	retval = mkdir(buf, S_IRUSR | S_IWUSR | S_IXUSR | S_IRGRP | S_IWGRP \
			 | S_IXGRP | S_IROTH | S_IWOTH | S_IXOTH);
	if (retval < 0) {
		LOGE("Failed to create log %s: %s.", buf, strerror(errno));
		return -1;
	}
	logwatch->cur_log_path = strdup(buf);
	free(buf);

	free(log_array);
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

	asprintf(&buf, "logcat -v time *:%s > logcat", logcat_priority[logwatch->logcat_prior]);

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

	retval = pthread_attr_setdetachstate(&logwatch->attr, PTHREAD_CREATE_DETACHED);
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
		retval = pthread_create(&logwatch->logcat_pid, &logwatch->attr, start_watch_logcat, (void *)logwatch);
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
	dump_logwatch_data(logwatch_data);
#endif

	retval = init_work(logwatch_data);
	if (retval < 0) {
		LOGE("Abort.");
		abort();
	}

	retval = do_work(logwatch_data);
	if (retval < 0) {
		LOGE("Abort.");
		abort();
	}

	while (1) {
		sleep(1000);
	}
	return 0;
}
