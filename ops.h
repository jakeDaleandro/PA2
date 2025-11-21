#ifndef OPS_H
#define OPS_H

#include "worker.h"
#include "hash.h"

void handle_insert(Job *job, int thread_id);
void handle_delete(Job *job, int thread_id);
void handle_update(Job *job, int thread_id);
void handle_search(Job *job, int thread_id);
void handle_print(Job *job, int thread_id);

#endif
