// hash.c
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "hash.h"

/* Initialize hash table */
void initTable(hashTable *t) {
    t->head = NULL;
    pthread_rwlock_init(&t->lock, NULL);
}

/* Destroy hash table (caller should not hold lock) */
void destroyTable(hashTable *t) {
    hashRecord *curr = t->head;
    while (curr) {
        hashRecord *tmp = curr->next;
        free(curr);
        curr = tmp;
    }
    t->head = NULL;
    pthread_rwlock_destroy(&t->lock);
}

/* Jenkins one-at-a-time hash */
uint32_t jenkins_hash(const char *key) {
    uint32_t hash = 0;
    while (*key) {
        hash += (unsigned char)*key++;
        hash += (hash << 10);
        hash ^= (hash >> 6);
    }
    hash += (hash << 3);
    hash ^= (hash >> 11);
    hash += (hash << 15);
    return hash;
}

/* Insert record (NO locking inside; caller must hold WRITE lock).
   Returns 0 on success, -1 if duplicate (by hash & name). */
int insertRecord(hashTable *t, const char *name, uint32_t salary) {
    uint32_t h = jenkins_hash(name);

    /* check duplicate by hash and name */
    hashRecord *curr = t->head;
    while (curr) {
        if (curr->hash == h && strcmp(curr->name, name) == 0) {
            return -1; // duplicate
        }
        curr = curr->next;
    }

    /* allocate and insert sorted by hash ascending */
    hashRecord *node = malloc(sizeof(hashRecord));
    if (!node) return -1;
    node->hash = h;
    strncpy(node->name, name, sizeof(node->name)-1);
    node->name[sizeof(node->name)-1] = '\0';
    node->salary = salary;
    node->next = NULL;

    if (t->head == NULL || h < t->head->hash) {
        node->next = t->head;
        t->head = node;
        return 0;
    }

    hashRecord *prev = t->head;
    curr = t->head->next;
    while (curr && curr->hash < h) {
        prev = curr;
        curr = curr->next;
    }
    prev->next = node;
    node->next = curr;
    return 0;
}

/* Delete record (NO locking inside; caller must hold WRITE lock).
   Returns 0 on success, -1 if not found. */
int deleteRecord(hashTable *t, const char *name) {
    uint32_t h = jenkins_hash(name);

    hashRecord *curr = t->head;
    hashRecord *prev = NULL;
    while (curr) {
        if (curr->hash == h && strcmp(curr->name, name) == 0) {
            if (prev) prev->next = curr->next;
            else t->head = curr->next;
            free(curr);
            return 0;
        }
        prev = curr;
        curr = curr->next;
    }
    return -1;
}

/* Update salary (NO locking inside; caller must hold WRITE lock).
   Returns 0 on success, -1 if not found. */
int updateSalary(hashTable *t, const char *name, uint32_t newSalary) {
    uint32_t h = jenkins_hash(name);

    hashRecord *curr = t->head;
    while (curr) {
        if (curr->hash == h && strcmp(curr->name, name) == 0) {
            curr->salary = newSalary;
            return 0;
        }
        curr = curr->next;
    }
    return -1;
}

/* Search record (NO locking inside; caller must hold READ lock).
   Returns pointer to record if found, else NULL. */
hashRecord* searchRecord(hashTable *t, const char *name) {
    uint32_t h = jenkins_hash(name);

    hashRecord *curr = t->head;
    while (curr) {
        if (curr->hash == h && strcmp(curr->name, name) == 0) {
            return curr;
        }
        curr = curr->next;
    }
    return NULL;
}

/* Print (NO locking inside; caller must hold READ lock). */
void printAll(hashTable *t, FILE *out) {
    fprintf(out, "Current Database:\n");
    hashRecord *curr = t->head;
    while (curr) {
        fprintf(out, "%u,%s,%u\n", curr->hash, curr->name, curr->salary);
        curr = curr->next;
    }
}
