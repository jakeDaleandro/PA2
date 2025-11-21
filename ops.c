#include "ops.h"
#include "locks.h"
#include "log.h"
#include "timestamp.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

extern hashTable table;

#define MAX_CMD_LEN 256

/* ---------------- INSERT ---------------- */
void handle_insert(Job *job, int id)
{
    char name[50], salaryStr[32], priorityStr[32];
    char full[MAX_CMD_LEN];
    strncpy(full, job->line, sizeof(full)-1);

    char *p = strtok(full, ","); // "insert"
    p = strtok(NULL, ","); if (p) strncpy(name, p, sizeof(name)-1);
    p = strtok(NULL, ","); if (p) strncpy(salaryStr, p, sizeof(salaryStr)-1);
    p = strtok(NULL, ","); if (p) strncpy(priorityStr, p, sizeof(priorityStr)-1);

    uint32_t salary = atoi(salaryStr);
    uint32_t h      = jenkins_hash(name);

    char buf[256];
    snprintf(buf, sizeof(buf), "INSERT,%u,%s,%u", h, name, salary);
    log_msg(id, buf);

    acquire_write_lock(&table.lock, id);
    int res = insertRecord(&table, name, salary);
    release_write_lock(&table.lock, id);

    FILE *out = fopen("output.txt", "a");
    if (out)
    {
        if (res == 0)
            fprintf(out, "Inserted %u,%s,%u\n", h, name, salary);
        else
            fprintf(out, "Insert failed. Entry %u is a duplicate.\n", h);
        fclose(out);
    }
}

/* ---------------- DELETE ---------------- */
void handle_delete(Job *job, int id)
{
    char full[MAX_CMD_LEN], name[50], dummy[32];
    strncpy(full, job->line, sizeof(full)-1);

    char *p = strtok(full, ",");
    p = strtok(NULL, ","); if (p) strncpy(name, p, sizeof(name)-1);
    p = strtok(NULL, ","); if (p) strncpy(dummy, p, sizeof(dummy)-1);

    uint32_t h = jenkins_hash(name);

    char buf[256];
    snprintf(buf, sizeof(buf), "DELETE,%u,%s", h, name);
    log_msg(id, buf);

    acquire_read_lock(&table.lock, id);
    hashRecord *old = searchRecord(&table, name);
    release_read_lock(&table.lock, id);

    uint32_t oldSalary = old ? old->salary : 0;

    acquire_write_lock(&table.lock, id);
    int res = deleteRecord(&table, name);
    release_write_lock(&table.lock, id);

    FILE *out = fopen("output.txt", "a");
    if (out)
    {
        if (res == 0)
        {
            if (old)
                fprintf(out, "Deleted record for %u,%s,%u\n", h, name, oldSalary);
            else
                fprintf(out, "Deleted record for %u,%s,UNKNOWN\n", h, name);
        }
        else
            fprintf(out, "Entry %u not deleted. Not in database.\n", h);

        fclose(out);
    }
}

/* ---------------- UPDATE ---------------- */
void handle_update(Job *job, int id)
{
    char name[50], salaryStr[32];
    char full[MAX_CMD_LEN];
    strncpy(full, job->line, sizeof(full)-1);

    char *p = strtok(full, ",");
    p = strtok(NULL, ","); if (p) strncpy(name, p, sizeof(name)-1);
    p = strtok(NULL, ","); if (p) strncpy(salaryStr, p, sizeof(salaryStr)-1);

    uint32_t newSalary = atoi(salaryStr);
    uint32_t h         = jenkins_hash(name);

    char buf[256];
    snprintf(buf, sizeof(buf), "UPDATE,%u,%s,%u", h, name, newSalary);
    log_msg(id, buf);

    acquire_read_lock(&table.lock, id);
    hashRecord *old = searchRecord(&table, name);
    release_read_lock(&table.lock, id);

    uint32_t oldSalary = old ? old->salary : 0;

    acquire_write_lock(&table.lock, id);
    int res = updateSalary(&table, name, newSalary);
    release_write_lock(&table.lock, id);

    FILE *out = fopen("output.txt", "a");
    if (out)
    {
        if (res == 0)
        {
            fprintf(out,
                    "Updated record %u from %u,%s,%u to %u,%s,%u\n",
                    h, h, name, oldSalary, h, name, newSalary);
        }
        else
        {
            fprintf(out, "Update failed. Entry %u not found.\n", h);
        }
        fclose(out);
    }
}

/* ---------------- SEARCH ---------------- */
void handle_search(Job *job, int id)
{
    char name[50], dummy[32];
    char full[MAX_CMD_LEN];
    strncpy(full, job->line, sizeof(full)-1);

    char *p = strtok(full, ",");
    p = strtok(NULL, ","); if (p) strncpy(name, p, sizeof(name)-1);
    p = strtok(NULL, ","); if (p) strncpy(dummy, p, sizeof(dummy)-1);

    uint32_t h = jenkins_hash(name);

    char buf[256];
    snprintf(buf, sizeof(buf), "SEARCH,%u,%s", h, name);
    log_msg(id, buf);

    acquire_read_lock(&table.lock, id);
    hashRecord *r = searchRecord(&table, name);
    release_read_lock(&table.lock, id);

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

/* ---------------- PRINT ---------------- */
void handle_print(Job *job, int id)
{
    log_msg(id, "PRINT");

    acquire_read_lock(&table.lock, id);
    FILE *out = fopen("output.txt", "a");
    if (out)
    {
        printAll(&table, out);
        fclose(out);
    }
    release_read_lock(&table.lock, id);
}
