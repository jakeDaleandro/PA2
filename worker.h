#ifndef WORKER_H
#define WORKER_H

#include <pthread.h>
#include <semaphore.h>
#include "hash.h"

/* One "job" per thread */
typedef struct {
    int id;
    char line[256];
    sem_t start_sem;
} Job;



extern int lock_acquisitions;
extern int lock_releases;

void setup_jobs(Job *jobs, int num_threads, FILE *fp);
void *worker_fn(void *arg);

#endif
