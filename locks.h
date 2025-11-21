#ifndef LOCKS_H
#define LOCKS_H

#include <pthread.h>
#include "hash.h"

extern int lock_acquisitions;
extern int lock_releases;

void acquire_read_lock(pthread_rwlock_t *lock, int thread_id);
void release_read_lock(pthread_rwlock_t *lock, int thread_id);

void acquire_write_lock(pthread_rwlock_t *lock, int thread_id);
void release_write_lock(pthread_rwlock_t *lock, int thread_id);

#endif
