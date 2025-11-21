#include "log.h"
#include "timestamp.h"
#include <pthread.h>

FILE *log_file = NULL;
pthread_mutex_t log_mutex = PTHREAD_MUTEX_INITIALIZER;

void log_init(const char *filename)
{
    log_file = fopen(filename, "w");
    if (!log_file) {
        perror("Couldn't open log file");
    }
}

void log_close(void)
{
    if (log_file) fclose(log_file);
}

void log_msg(int thread_id, const char *msg)
{
    pthread_mutex_lock(&log_mutex);
    uint64_t ts = timestamp_now();
    fprintf(log_file, "%llu: THREAD %d %s\n",
            (unsigned long long)ts,
            thread_id,
            msg);
    fflush(log_file);
    pthread_mutex_unlock(&log_mutex);
}
