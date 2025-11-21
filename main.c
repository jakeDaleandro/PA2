#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <unistd.h>

#include "worker.h"
#include "hash.h"
#include "log.h"
#include "locks.h"
#include "ops.h"

hashTable table;

int main()
{
    /* init hash table */
    initTable(&table);

    /* open input commands */
    FILE *fp = fopen("commands.txt", "r");
    if (!fp) {
        printf("Cannot open commands.txt\n");
        return 1;
    }

    /* reset output.txt */
    FILE *out_init = fopen("output.txt", "w");
    if (out_init) fclose(out_init);

    /* init log */
    log_init("hash.log");

    /* parse "threads,N,start" */
    char line[256];
    fgets(line, sizeof(line), fp);
    line[strcspn(line, "\n")] = 0;

    int num_threads = 0;
    int start_index = 0;
    sscanf(line, "threads,%d,%d", &num_threads, &start_index);

    /* allocate jobs */
    Job *jobs = calloc(num_threads, sizeof(Job));
    setup_jobs(jobs, num_threads, fp);

    /* spawn threads */
    pthread_t *tids = malloc(sizeof(pthread_t) * num_threads);
    for (int i = 0; i < num_threads; i++) {
        pthread_create(&tids[i], NULL, worker_fn, &jobs[i]);
        usleep(10);
    }

    /* start first thread */
    sem_post(&jobs[start_index].start_sem);

    /* join threads in sequence */
    for (int i = 0; i < num_threads; i++) {
        int idx = (start_index + i) % num_threads;
        pthread_join(tids[idx], NULL);

        if (i < num_threads - 1) {
            int next = (idx + 1) % num_threads;
            sem_post(&jobs[next].start_sem);
        }
    }

    /* final logging */
    pthread_mutex_lock(&log_mutex);
    fprintf(log_file, "Number of lock acquisitions: %d\n", lock_acquisitions);
    fprintf(log_file, "Number of lock releases: %d\n", lock_releases);
    fprintf(log_file, "Final Table:\n");
    pthread_mutex_unlock(&log_mutex);

    pthread_rwlock_rdlock(&table.lock);
    printAll(&table, log_file);
    pthread_rwlock_unlock(&table.lock);

    /* cleanup */
    for (int i = 0; i < num_threads; i++)
        sem_destroy(&jobs[i].start_sem);

    free(jobs);
    free(tids);

    log_close();
    destroyTable(&table);
    fclose(fp);

    return 0;
}
