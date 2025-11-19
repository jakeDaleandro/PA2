#ifndef HASH_H
#define HASH_H

#include <stdint.h>
#include <pthread.h>

typedef struct hash_struct {
    uint32_t hash;
    char name[50];
    uint32_t salary;
    struct hash_struct *next;
} hashRecord;

typedef struct {
    hashRecord *head;
    pthread_rwlock_t lock;
} hashTable;

void initTable(hashTable *t);
void destroyTable(hashTable *t);

// Hash function
uint32_t jenkins_hash(const char *key);

// Operations
int insertRecord(hashTable *t, const char *name, uint32_t salary);
int deleteRecord(hashTable *t, const char *name);
int updateSalary(hashTable *t, const char *name, uint32_t newSalary);
hashRecord* searchRecord(hashTable *t, const char *name);
void printAll(hashTable *t);

#endif
