#ifndef LOG_H
#define LOG_H

#include <stdio.h>
#include <stdint.h>
#include <pthread.h>

extern FILE *log_file;
extern pthread_mutex_t log_mutex;

void log_init(const char *filename);
void log_close(void);
void log_msg(int thread_id, const char *msg);

#endif
