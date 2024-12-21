#ifndef UTILS_H
#define UTILS_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdbool.h>
#include <time.h>

typedef struct cache_block cache_block;
typedef struct file_cache file_cache;
typedef union cache_block_usage cache_block_usage;

union cache_block_usage {
    int frequency;
    time_t prev_time;
};

struct cache_block {
    off_t offset;
    void *data;
    size_t block_size;
    cache_block_usage usage;
    // int usage_frequency;
    bool is_dirty;
    cache_block *next_block;
};

struct file_cache {
    int fd;
    size_t block_size;
    size_t max_blocks;
    off_t current_offset;
    size_t current_blocks;
    cache_block *head;
};

int lru_cache_evict(file_cache *cache);
int create_lru_cache_block(file_cache *cache, const void *buf, ssize_t count, off_t offset);
int compare_times(time_t time1, time_t time2);

#endif //UTILS_H
