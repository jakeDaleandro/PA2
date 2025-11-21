// main.c
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <semaphore.h>
#include <stdint.h>
#include <unistd.h>
#include <sys/time.h>
#include "hash.h"

#define MAX_LINE 256
#define MAX_CMD_LEN 256

/* Logging globals */
FILE *log_file;
pthread_mutex_t log_mutex = PTHREAD_MUTEX_INITIALIZER;

/* Counters for lock events */
static int lock_acquisitions = 0;
static int lock_releases = 0;

/* timestamp (microseconds since epoch) */
static inline uint64_t timestamp_now()
{
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return (uint64_t)tv.tv_sec * 1000000ULL + tv.tv_usec;
}

/* thread-safe log writer matching professor format */
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

/* job struct: each thread gets one job line */
typedef struct
{
    int id;                 // thread id (0..N-1)
    char line[MAX_CMD_LEN]; // the command line assigned to this thread
    sem_t start_sem;        // semaphore the thread waits on to be awakened
} Job;

static Job *jobs = NULL;
static int num_threads = 0;
static int start_thread_index = 0;

/* global hash table shared by threads */
static hashTable table;

/* Thread function: wait for its turn, log states, perform op */
void *worker_fn(void *arg)
{
    Job *job = (Job *)arg;
    int id = job->id;

    /* At startup, each thread immediately logs that it's waiting */
    log_msg(id, "WAITING FOR MY TURN");

    /* Wait until main posts this thread's semaphore to start */
    sem_wait(&job->start_sem);

    /* Awakened */
    log_msg(id, "AWAKENED FOR WORK");

    /* parse the assigned command (already trimmed) */
    char cmdcpy[MAX_CMD_LEN];
    strncpy(cmdcpy, job->line, MAX_CMD_LEN - 1);
    cmdcpy[MAX_CMD_LEN - 1] = '\0';

    char op[16];
    // split by comma to get op
    char *p = strtok(cmdcpy, ",");
    if (!p)
    {
        // nothing
        pthread_exit(NULL);
    }
    strncpy(op, p, sizeof(op) - 1);
    op[sizeof(op) - 1] = '\0';

    // handle operations
    if (strcmp(op, "insert") == 0)
    {
        char name[50];
        char salaryStr[32];
        char priorityStr[32];
        // original line format: insert,name,salary,priority
        // we need the original line to compute hash and print exactly
        char full[MAX_CMD_LEN];
        strncpy(full, job->line, sizeof(full) - 1);
        full[sizeof(full) - 1] = '\0';

        // parse fields
        // we start from second token
        p = strtok(NULL, ",");
        if (p)
            strncpy(name, p, sizeof(name) - 1);
        else
            name[0] = '\0';
        p = strtok(NULL, ",");
        if (p)
            strncpy(salaryStr, p, sizeof(salaryStr) - 1);
        else
            salaryStr[0] = '\0';
        p = strtok(NULL, ",");
        if (p)
            strncpy(priorityStr, p, sizeof(priorityStr) - 1);
        else
            priorityStr[0] = '\0';

        uint32_t salary = (uint32_t)atoi(salaryStr);
        uint32_t h = jenkins_hash(name);

        // log the INSERT line with computed hash (matches professor format)
        char buf[256];
        snprintf(buf, sizeof(buf), "INSERT,%u,%s,%u", h, name, salary);
        log_msg(id, buf);

        // Acquire write lock (main-level lock) and log
        log_msg(id, "WRITE LOCK ACQUIRED");
        pthread_rwlock_wrlock(&table.lock);
        // increment acquisition counter (protected by log mutex)
        pthread_mutex_lock(&log_mutex);
        lock_acquisitions++;
        pthread_mutex_unlock(&log_mutex);

        /* perform insertion (hash.c functions assume caller holds lock) */
        int res = insertRecord(&table, name, salary); // now non-locking implementation

        // Release write lock and log
        pthread_rwlock_unlock(&table.lock);
        pthread_mutex_lock(&log_mutex);
        lock_releases++;
        pthread_mutex_unlock(&log_mutex);
        log_msg(id, "WRITE LOCK RELEASED");

        /* Write final output to output.txt (append requires main-level file) */
        // We will let main gather final output; but to maintain old behavior, write to output.txt now:
        FILE *out = fopen("output.txt", "a");
        if (out)
        {
            if (res == 0)
            {
                fprintf(out, "Inserted %u,%s,%u\n", h, name, salary);
            }
            else
            {
                fprintf(out, "Insert failed. Entry %u is a duplicate.\n", h);
            }
            fclose(out);
        }
    }
    else if (strcmp(op, "delete") == 0)
    {
        char name[50], dummy[32];

        // delete,name,priority
        char full[MAX_CMD_LEN];
        strncpy(full, job->line, sizeof(full) - 1);
        full[sizeof(full) - 1] = '\0';

        p = strtok(NULL, ",");
        if (p)
            strncpy(name, p, sizeof(name) - 1);
        else
            name[0] = '\0';
        p = strtok(NULL, ",");
        if (p)
            strncpy(dummy, p, sizeof(dummy) - 1);
        else
            dummy[0] = '\0';

        uint32_t h = jenkins_hash(name);

        char buf[256];
        snprintf(buf, sizeof(buf), "DELETE,%u,%s", h, name);
        log_msg(id, buf);

        // ---- BEFORE DELETE: capture old record so we can print salary ----
        pthread_rwlock_rdlock(&table.lock);
        hashRecord *old = searchRecord(&table, name);
        pthread_rwlock_unlock(&table.lock);

        uint32_t oldSalary = (old ? old->salary : 0);

        // ---- WRITE LOCK + DELETE ----
        log_msg(id, "WRITE LOCK ACQUIRED");
        pthread_rwlock_wrlock(&table.lock);
        pthread_mutex_lock(&log_mutex);
        lock_acquisitions++;
        pthread_mutex_unlock(&log_mutex);

        int res = deleteRecord(&table, name);

        pthread_rwlock_unlock(&table.lock);
        pthread_mutex_lock(&log_mutex);
        lock_releases++;
        pthread_mutex_unlock(&log_mutex);
        log_msg(id, "WRITE LOCK RELEASED");

        // ---- OUTPUT ----
        FILE *out = fopen("output.txt", "a");
        if (out)
        {
            if (res == 0)
            {
                if (old)
                    fprintf(out, "Deleted record for %u,%s,%u\n",
                            h, name, oldSalary);
                else
                    fprintf(out, "Deleted record for %u,%s,UNKNOWN\n",
                            h, name);
            }
            else
            {
                fprintf(out, "Entry %u not deleted. Not in database.\n", h);
            }
            fclose(out);
        }
    }
    else if (strcmp(op, "update") == 0)
    {
        char name[50], salaryStr[32];

        // update,name,newSalary
        p = strtok(NULL, ",");
        if (p)
            strncpy(name, p, sizeof(name) - 1);
        else
            name[0] = '\0';

        p = strtok(NULL, ",");
        if (p)
            strncpy(salaryStr, p, sizeof(salaryStr) - 1);
        else
            salaryStr[0] = '\0';

        uint32_t newSalary = (uint32_t)atoi(salaryStr);
        uint32_t h = jenkins_hash(name);

        char buf[256];
        snprintf(buf, sizeof(buf), "UPDATE,%u,%s,%u", h, name, newSalary);
        log_msg(id, buf);

        // ---- BEFORE UPDATE: capture old salary ----
        pthread_rwlock_rdlock(&table.lock);
        hashRecord *old = searchRecord(&table, name);
        pthread_rwlock_unlock(&table.lock);

        uint32_t oldSalary = (old ? old->salary : 0);

        // ---- WRITE LOCK + UPDATE ----
        log_msg(id, "WRITE LOCK ACQUIRED");
        pthread_rwlock_wrlock(&table.lock);
        pthread_mutex_lock(&log_mutex);
        lock_acquisitions++;
        pthread_mutex_unlock(&log_mutex);

        int res = updateSalary(&table, name, newSalary);

        pthread_rwlock_unlock(&table.lock);
        pthread_mutex_lock(&log_mutex);
        lock_releases++;
        pthread_mutex_unlock(&log_mutex);
        log_msg(id, "WRITE LOCK RELEASED");

        // ---- OUTPUT ----
        FILE *out = fopen("output.txt", "a");
        if (out)
        {
            if (res == 0)
            {
                if (old)
                {
                    fprintf(out,
                            "Updated record %u from %u,%s,%u to %u,%s,%u\n",
                            h,                  // record number
                            h, name, oldSalary, // BEFORE
                            h, name, newSalary  // AFTER
                    );
                }
                else
                {
                    // Should not happen normally, but safe fallback
                    fprintf(out,
                            "Updated record %u from UNKNOWN to %u,%s,%u\n",
                            h, h, name, newSalary);
                }
            }
            else
            {
                fprintf(out, "Update failed. Entry %u not found.\n", h);
            }
            fclose(out);
        }
    }
    else if (strcmp(op, "search") == 0)
    {
        char name[50], dummy[32];
        p = strtok(NULL, ",");
        if (p)
            strncpy(name, p, sizeof(name) - 1);
        else
            name[0] = '\0';
        p = strtok(NULL, ",");
        if (p)
            strncpy(dummy, p, sizeof(dummy) - 1);
        else
            dummy[0] = '\0';
        uint32_t h = jenkins_hash(name);
        char buf[256];
        snprintf(buf, sizeof(buf), "SEARCH,%u,%s", h, name);
        log_msg(id, buf);

        log_msg(id, "READ LOCK ACQUIRED");
        pthread_rwlock_rdlock(&table.lock);
        pthread_mutex_lock(&log_mutex);
        lock_acquisitions++;
        pthread_mutex_unlock(&log_mutex);

        hashRecord *r = searchRecord(&table, name); // non-locking search

        pthread_rwlock_unlock(&table.lock);
        pthread_mutex_lock(&log_mutex);
        lock_releases++;
        pthread_mutex_unlock(&log_mutex);
        log_msg(id, "READ LOCK RELEASED");

        FILE *out = fopen("output.txt", "a");
        if (out)
        {
            if (r)
                fprintf(out, "Found: %u,%s,%u\n", r->hash, r->name, r->salary);
            else
                fprintf(out, "No Record Found: %s\n", name);
            fclose(out);
        }
    }
    else if (strcmp(op, "print") == 0)
    {
        log_msg(id, "PRINT");
        log_msg(id, "READ LOCK ACQUIRED");
        pthread_rwlock_rdlock(&table.lock);
        pthread_mutex_lock(&log_mutex);
        lock_acquisitions++;
        pthread_mutex_unlock(&log_mutex);

        /* Print to output.txt using printAll (non-locking) */
        FILE *out = fopen("output.txt", "a");
        if (out)
        {
            printAll(&table, out); // assumes caller holds lock
            fclose(out);
        }

        pthread_rwlock_unlock(&table.lock);
        pthread_mutex_lock(&log_mutex);
        lock_releases++;
        pthread_mutex_unlock(&log_mutex);
        log_msg(id, "READ LOCK RELEASED");
    }
    else
    {
        // unknown command
        char buf[128];
        snprintf(buf, sizeof(buf), "UNKNOWN COMMAND: %s", job->line);
        log_msg(id, buf);
    }

    /* finished job - exit thread */
    pthread_exit(NULL);
    return NULL;
}

int main()
{
    /* initialize table and files */
    initTable(&table);

    FILE *fp = fopen("commands.txt", "r");
    if (!fp)
    {
        printf("Could not open commands.txt\n");
        return 1;
    }

    /* create output.txt (overwrite) */
    FILE *out_init = fopen("output.txt", "w");
    if (out_init)
        fclose(out_init);

    /* open hash.log */
    log_file = fopen("hash.log", "w");
    if (!log_file)
    {
        perror("Could not open hash.log");
        fclose(fp);
        return 1;
    }

    /* Read first line for "threads,N,start" */
    char line[MAX_LINE];
    if (!fgets(line, sizeof(line), fp))
    {
        fprintf(stderr, "commands.txt empty\n");
        fclose(fp);
        fclose(log_file);
        return 1;
    }
    // trim newline
    line[strcspn(line, "\n")] = 0;

    int parsed = 0;
    if (strncmp(line, "threads,", 8) == 0)
    {
        int threads_val = 0;
        int start_val = 0;
        parsed = sscanf(line, "threads,%d,%d", &threads_val, &start_val);
        if (parsed >= 1)
        {
            num_threads = threads_val;
            start_thread_index = (parsed == 2 ? start_val : 0);
        }
    }
    if (num_threads <= 0)
    {
        fprintf(stderr, "Invalid threads specification\n");
        fclose(fp);
        fclose(log_file);
        return 1;
    }

    /* allocate job array */
    jobs = calloc((size_t)num_threads, sizeof(Job));
    if (!jobs)
    {
        perror("calloc");
        fclose(fp);
        fclose(log_file);
        return 1;
    }

    /* read next num_threads lines and assign to jobs 0..N-1 */
    for (int i = 0; i < num_threads; ++i)
    {
        if (!fgets(line, sizeof(line), fp))
        {
            // if fewer commands than threads, remaining jobs are blank
            line[0] = '\0';
        }
        line[strcspn(line, "\n")] = 0; // remove newline
        jobs[i].id = i;
        strncpy(jobs[i].line, line, sizeof(jobs[i].line) - 1);
        jobs[i].line[sizeof(jobs[i].line) - 1] = '\0';
        sem_init(&jobs[i].start_sem, 0, 0); // initialize to 0
    }

    /* create worker threads */
    pthread_t *tids = malloc(sizeof(pthread_t) * (size_t)num_threads);
    for (int i = 0; i < num_threads; ++i)
    {
        pthread_create(&tids[i], NULL, worker_fn, &jobs[i]);
        // give a tiny sleep so threads log WAITING at slightly different times
        usleep(10);
    }

    /* Immediately awaken the start thread (e.g., thread 0) */
    sem_post(&jobs[start_thread_index].start_sem);

    /* Wait for the awakened thread to finish, then awaken next, etc.
       We use pthread_join to wait for each thread in sequence so the log
       ordering matches the sequential wake/run pattern. */
    for (int i = 0; i < num_threads; ++i)
    {
        int idx = (start_thread_index + i) % num_threads;
        pthread_join(tids[idx], NULL);
        // after this thread finishes, awaken the next if it's not already started
        int next_idx = (idx + 1) % num_threads;
        if (i < num_threads - 1)
        {
            sem_post(&jobs[next_idx].start_sem);
        }
    }

    /* after all threads finished, print counts and final table to hash.log */
    pthread_mutex_lock(&log_mutex);
    fprintf(log_file, "Number of lock acquisitions: %d\n", lock_acquisitions);
    fprintf(log_file, "Number of lock releases: %d\n", lock_releases);
    fflush(log_file);
    pthread_mutex_unlock(&log_mutex);

    /* Print final table to log (we will take a read lock while printing) */
    pthread_rwlock_rdlock(&table.lock);
    fprintf(log_file, "Final Table:\n");
    // printAll assumes caller holds lock
    printAll(&table, log_file);
    pthread_rwlock_unlock(&table.lock);

    /* cleanup */
    for (int i = 0; i < num_threads; ++i)
    {
        sem_destroy(&jobs[i].start_sem);
    }
    free(jobs);
    free(tids);

    fclose(log_file);
    destroyTable(&table); // free table memory (no locking required now)

    return 0;
}
