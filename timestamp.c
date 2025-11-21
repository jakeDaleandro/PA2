#include "timestamp.h"

/* timestamp (microseconds since epoch) */
uint64_t timestamp_now()
{
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return (uint64_t)tv.tv_sec * 1000000ULL + tv.tv_usec;
}