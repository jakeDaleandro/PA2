#include "worker.h"
#include "log.h"
#include "timestamp.h"
#include <string.h>
#include <stdlib.h>
#include <unistd.h>

#define MAX_CMD_LEN 256

/* Shared hash table from main */
extern hashTable table;

/* counters */
int lock_acquisitions = 0;
int lock_releases = 0;

/* Build job list from commands.txt */
void setup_jobs(Job *jobs, int num_threads, FILE *fp)
{
    char line[256];

    for (int i = 0; i < num_threads; i++)
    {
        if (!fgets(line, sizeof(line), fp))
            line[0] = '\0';

        line[strcspn(line, "\n")] = 0;

        jobs[i].id = i;
        strncpy(jobs[i].line, line, sizeof(jobs[i].line) - 1);
        jobs[i].line[sizeof(jobs[i].line) - 1] = '\0';
        sem_init(&jobs[i].start_sem, 0, 0);
    }
}

/* Worker thread */
void *worker_fn(void *arg)
{
    Job *job = (Job *)arg;
    int id = job->id;

    log_msg(id, "WAITING FOR MY TURN");
    sem_wait(&job->start_sem);
    log_msg(id, "AWAKENED FOR WORK");

    char cmdcpy[256];
    strncpy(cmdcpy, job->line, sizeof(cmdcpy));
    cmdcpy[sizeof(cmdcpy) - 1] = '\0';
    char op[16];
    char *p = strtok(cmdcpy, ",");
    if (!p) pthread_exit(NULL);

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
}
