#include "worker.h"
#include "ops.h"
#include "log.h"
#include <string.h>
#include <stdlib.h>
#include <stdio.h>

void setup_jobs(Job *jobs, int num_threads, FILE *fp)
{
    char line[256];

    for (int i = 0; i < num_threads; i++)
    {
        if (!fgets(line, sizeof(line), fp))
            line[0] = '\0';

        line[strcspn(line, "\n")] = 0;

        jobs[i].id = i;
        strncpy(jobs[i].line, line, sizeof(jobs[i].line));
        sem_init(&jobs[i].start_sem, 0, 0);
    }
}

void *worker_fn(void *arg)
{
    Job *job = arg;
    int id = job->id;

    log_msg(id, "WAITING FOR MY TURN");
    sem_wait(&job->start_sem);
    log_msg(id, "AWAKENED FOR WORK");

    char op[32];
    char cmdcpy[256];
    strncpy(cmdcpy, job->line, sizeof(cmdcpy));

    char *p = strtok(cmdcpy, ",");
    if (!p) pthread_exit(NULL);

    strncpy(op, p, sizeof(op)-1);

    if      (strcmp(op, "insert") == 0) handle_insert(job, id);
    else if (strcmp(op, "delete") == 0) handle_delete(job, id);
    else if (strcmp(op, "update") == 0) handle_update(job, id);
    else if (strcmp(op, "search") == 0) handle_search(job, id);
    else if (strcmp(op, "print")  == 0) handle_print(job, id);
    else
    {
        char buf[128];
        snprintf(buf, sizeof(buf), "UNKNOWN COMMAND: %s", job->line);
        log_msg(id, buf);
    }

    pthread_exit(NULL);
}
