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

#ifndef LOGWATCH_H_
#define LOGWATCH_H_

#ifdef DEBUG
#define DEBUG_TRACE 1
#else
#define DEBUG_TRACE 0
#endif

#ifdef SUPPORT_ANDROID

#ifndef LOGV
#define LOGV(...) __android_log_print(ANDROID_LOG_VERBOSE, LOG_TAG, __VA_ARGS__)
#endif

#ifdef DEBUG
#ifndef LOGD
#define LOGD(...) __android_log_print(ANDROID_LOG_DEBUG , LOG_TAG, __VA_ARGS__)
#endif
#else
#define LOGD(...)   \
    do {            \
    } while (0)
#endif

#ifndef LOGI
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO  , LOG_TAG, __VA_ARGS__)
#endif

#ifndef LOGW
#define LOGW(...) __android_log_print(ANDROID_LOG_WARN , LOG_TAG, __VA_ARGS__)
#endif

#ifndef LOGE
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR , LOG_TAG, __VA_ARGS__)
#endif

#else

#define LOGV(...)                                   \
    do {                                            \
      int save_errno = errno;                       \
      fprintf(stderr, "V/%s:", LOG_TAG);            \
      errno = save_errno;                           \
      fprintf(stderr, __VA_ARGS__);                 \
      fprintf(stderr, "\n");                        \
      fflush(stderr);                               \
      errno = save_errno;                           \
    } while (0)

#define LOGD(...)                                   \
    do {                                            \
      if (DEBUG_TRACE) {                            \
          int save_errno = errno;                   \
          fprintf(stderr, "D/%s:", LOG_TAG);        \
          errno = save_errno;                       \
          fprintf(stderr, __VA_ARGS__);             \
          fprintf(stderr, "\n");                    \
          fflush(stderr);                           \
          errno = save_errno;                       \
      }                                             \
    } while (0)

#define LOGI(...)                                   \
    do {                                            \
      int save_errno = errno;                       \
      fprintf(stderr, "I/%s:", LOG_TAG);            \
      errno = save_errno;                           \
      fprintf(stderr, __VA_ARGS__);                 \
      fprintf(stderr, "\n");                        \
      fflush(stderr);                               \
      errno = save_errno;                           \
    } while (0)

#define LOGW(...)                                   \
    do {                                            \
      int save_errno = errno;                       \
      fprintf(stderr, "W/%s:", LOG_TAG);            \
      errno = save_errno;                           \
      fprintf(stderr, __VA_ARGS__);                 \
      fprintf(stderr, "\n");                        \
      fflush(stderr);                               \
      errno = save_errno;                           \
    } while (0)

#define LOGE(...)                                   \
    do {                                            \
      int save_errno = errno;                       \
      fprintf(stderr, "W/%s:", LOG_TAG);            \
      errno = save_errno;                           \
      fprintf(stderr, __VA_ARGS__);                 \
      fprintf(stderr, "\n");                        \
      fflush(stderr);                               \
      errno = save_errno;                           \
    } while (0)

#endif

#define CONFIG_FILE "/etc/logwatch.conf"
#define LOG_FOLDER "ingenic-log"
#define KERNEL_LOG_NAME "kmsg.txt"
#define LOGCAT_LOG_NAME "logcat.txt"

struct object
{
    const char *name;
    char *value;
};

struct logcat
{
    struct object is_enable;
    struct object fifo_size;
    struct object prior;
};

struct kmsg
{
    struct object is_enable;
    struct object fifo_size;
    struct object prior;
};

struct misc
{
    struct object enable;
    struct object delay;
    struct object log_path;
    struct object log_num;
};

struct config
{
    struct misc *misc;
    struct kmsg *kmsg;
    struct logcat *logcat;
};

struct logwatch_data
{
    int is_enable_logwatch;
#define LOGWATCH_ENABLE_DEF	1

    int boot_delay;
#define BOOT_DELAY_DEF	0

    char* log_path;

#ifdef SUPPORT_ANDROID
#define	LOG_PATH_DEF	"/data"
#else
#define LOG_PATH_DEF    "/var"
#endif

    int log_num;
#define LOG_NUM_DEF		2

    pthread_attr_t attr;
    char* cur_log_path;

    int kmsg_fd;
    pthread_t kmsg_pid;
    int is_enable_kmsg;
#define KMSG_ENABLE_DEF	1

    unsigned long kmsg_size;
#define KMSG_SIZE_DEF (1024)

    int kmsg_prior;
#define KERN_EMERG		0
#define KERN_ALERT		1
#define KERN_CRIT		2
#define KERN_ERR		3
#define KERN_WARNING	4
#define KERN_NOTICE		5
#define KERN_INFO		6
#define KERN_DEBUG		7
#define KMSG_PRIOR_DEF	KERN_DEBUG

    pthread_t logcat_pid;
    int logcat_fd;
    int is_enable_logcat;
#ifdef SUPPORT_ANDROID
#define LOGCAT_ENABLE_DEF	1
#else
#define LOGCAT_ENABLE_DEF   0
#endif
    unsigned long logcat_size;
#define LOGCAT_SIZE_DEF	(1024)

    int logcat_prior;
#define	LOGCAT_VERBOSE	6
#define LOGCAT_DEBUG	5
#define	LOGCAT_INFO		4
#define	LOGCAT_WARNING	3
#define	LOGCAT_ERROR	2
#define	LOGCAT_FATAL	1
#define	LOGCAT_SILENT	0
#define	LOGCAT_PRIOR_DEF	LOGCAT_ERROR
};

#endif /* CONFIG_H_ */
