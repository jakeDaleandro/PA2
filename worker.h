#ifndef WORKER_H
#define WORKER_H

#include <semaphore.h>
#include <stdint.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct {
    int id;
    char line[256];
    sem_t start_sem;
} Job;

void setup_jobs(Job *jobs, int num_threads, FILE *fp);
void *worker_fn(void *arg);

#endif
