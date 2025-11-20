#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "hash.h"

// -------------------------------------------------
// Initialize hash table
// -------------------------------------------------
void initTable(hashTable *t) {
    t->head = NULL;
    pthread_rwlock_init(&t->lock, NULL);
}

// -------------------------------------------------
// Destroy hash table
// -------------------------------------------------
void destroyTable(hashTable *t) {
    pthread_rwlock_wrlock(&t->lock);

    hashRecord *curr = t->head;
    while (curr) {
        hashRecord *tmp = curr->next;
        free(curr);
        curr = tmp;
    }

    pthread_rwlock_unlock(&t->lock);
    pthread_rwlock_destroy(&t->lock);
}

// -------------------------------------------------
// Jenkins one-at-a-time hash
// -------------------------------------------------
uint32_t jenkins_hash(const char *key) {
    uint32_t hash = 0;
    while (*key) {
        hash += *key++;
        hash += (hash << 10);
        hash ^= (hash >> 6);
    }
    hash += (hash << 3);
    hash ^= (hash >> 11);
    hash += (hash << 15);
    return hash;
}

// -------------------------------------------------
// Insert (sorted by key / hash)
// -------------------------------------------------
int insertRecord(hashTable *t, const char *name, uint32_t salary) {
    uint32_t h = jenkins_hash(name);

    // LOG: Write Lock Acquired
    pthread_rwlock_wrlock(&t->lock);

    // Check for duplicate
    hashRecord *curr = t->head;
    while (curr) {
        if (curr->hash == h) {
            pthread_rwlock_unlock(&t->lock);
            return -1; // duplicate
        }
        curr = curr->next;
    }

    // Create new node
    hashRecord *node = malloc(sizeof(hashRecord));
    node->hash = h;
    strcpy(node->name, name);
    node->salary = salary;
    node->next = NULL;

    // CASE 1: empty list OR insert at head (h < head->hash)
    if (t->head == NULL || h < t->head->hash) {
        node->next = t->head;
        t->head = node;

        pthread_rwlock_unlock(&t->lock);
        return 0;
    }

    // CASE 2: insert somewhere after head
    hashRecord *prev = t->head;
    curr = t->head->next;

    while (curr && curr->hash < h) {
        prev = curr;
        curr = curr->next;
    }

    // Insert between prev and curr
    prev->next = node;
    node->next = curr;

    // LOG: Write Lock Released
    pthread_rwlock_unlock(&t->lock);
    return 0;
}

// -------------------------------------------------
// Delete
// -------------------------------------------------
int deleteRecord(hashTable *t, const char *name) {
    uint32_t h = jenkins_hash(name);

    // LOG: Write Lock Acquired
    pthread_rwlock_wrlock(&t->lock);

    hashRecord *curr = t->head;
    hashRecord *prev = NULL;

    while (curr) {
        if (curr->hash == h) {
            if (prev) prev->next = curr->next;
            else t->head = curr->next;

            free(curr);
            pthread_rwlock_unlock(&t->lock);
            return 0;
        }
        prev = curr;
        curr = curr->next;
    }

    pthread_rwlock_unlock(&t->lock);
    return -1; // Not found
}

// -------------------------------------------------
// Update salary
// -------------------------------------------------
int updateSalary(hashTable *t, const char *name, uint32_t newSalary) {
    uint32_t h = jenkins_hash(name);

    // LOG: Write Lock Acquired
    pthread_rwlock_wrlock(&t->lock);

    hashRecord *curr = t->head;
    while (curr) {
        if (curr->hash == h) {
            curr->salary = newSalary;
            pthread_rwlock_unlock(&t->lock);
            return 0;
        }
        curr = curr->next;
    }

    pthread_rwlock_unlock(&t->lock);
    return -1; // Not found
}

// -------------------------------------------------
// Search
// -------------------------------------------------
hashRecord* searchRecord(hashTable *t, const char *name) {
    uint32_t h = jenkins_hash(name);

    // LOG: Read Lock Acquired
    pthread_rwlock_rdlock(&t->lock);

    hashRecord *curr = t->head;
    while (curr) {
        if (curr->hash == h) {
            pthread_rwlock_unlock(&t->lock);
            return curr;
        }
        curr = curr->next;
    }

    pthread_rwlock_unlock(&t->lock);
    return NULL;
}

// -------------------------------------------------
// Print entire database
// -------------------------------------------------
void printAll(hashTable *t, FILE *out) {
    pthread_rwlock_rdlock(&t->lock);

    fprintf(out, "Current Database:\n");

    hashRecord *curr = t->head;
    while (curr) {
        fprintf(out, "%u,%s,%u\n", curr->hash, curr->name, curr->salary);
        curr = curr->next;
    }

    pthread_rwlock_unlock(&t->lock);
}
