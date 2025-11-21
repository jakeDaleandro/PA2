#include "locks.h"
#include "log.h"

int lock_acquisitions = 0;
int lock_releases     = 0;

void acquire_read_lock(pthread_rwlock_t *lock, int thread_id)
{
    log_msg(thread_id, "READ LOCK ACQUIRED");
    pthread_rwlock_rdlock(lock);

    pthread_mutex_lock(&log_mutex);
    lock_acquisitions++;
    pthread_mutex_unlock(&log_mutex);
}

void release_read_lock(pthread_rwlock_t *lock, int thread_id)
{
    pthread_rwlock_unlock(lock);

    pthread_mutex_lock(&log_mutex);
    lock_releases++;
    pthread_mutex_unlock(&log_mutex);

    log_msg(thread_id, "READ LOCK RELEASED");
}

void acquire_write_lock(pthread_rwlock_t *lock, int thread_id)
{
    log_msg(thread_id, "WRITE LOCK ACQUIRED");
    pthread_rwlock_wrlock(lock);

    pthread_mutex_lock(&log_mutex);
    lock_acquisitions++;
    pthread_mutex_unlock(&log_mutex);
}

void release_write_lock(pthread_rwlock_t *lock, int thread_id)
{
    pthread_rwlock_unlock(lock);

    pthread_mutex_lock(&log_mutex);
    lock_releases++;
    pthread_mutex_unlock(&log_mutex);

    log_msg(thread_id, "WRITE LOCK RELEASED");
}
