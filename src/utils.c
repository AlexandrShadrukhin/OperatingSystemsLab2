#include "utils.h"

int lru_cache_evict(file_cache *cache) {
    if (!cache) {
        perror("[!] Get cache");
        return -1;
    }

    cache_block *prev = NULL;
    cache_block *least_used = cache->head;
    cache_block *current = cache->head;

    while (current->next_block) {
        if (current->next_block->usage.prev_time < least_used->usage.prev_time) {
            prev = current;
            least_used = current->next_block;
        }

        current = current->next_block;
    }

    if (least_used->is_dirty) {
        const ssize_t bytes_written = pwrite(cache->fd, least_used->data, least_used->block_size, least_used->offset);
        if (bytes_written < 0) {
            perror("[!] Write block to disk while eviction");
        }
    }

    if (prev) {
        prev->next_block = least_used->next_block;
    } else {
        cache->head = least_used->next_block;
    }

    free(least_used->data);
    free(least_used);

    cache->current_blocks--;

    return 0;
}

int create_lru_cache_block(file_cache *cache, const void *buf, ssize_t count, off_t offset) {
    size_t actual_count = (size_t) count > cache->block_size ? cache->block_size : (size_t) count;

    void *block_data = malloc(cache->block_size);
    if (!block_data) {
        perror("[!] Allocate memory for cache block");
        return -1;
    }

    memcpy(block_data, buf, actual_count);

    cache_block *new_block = malloc(sizeof(cache_block));
    if (!new_block) {
        perror("[!] Allocate memory for cache block");
        free(block_data);
        return -1;
    }

    new_block->offset = offset;
    new_block->data = block_data;
    new_block->block_size = actual_count;
    new_block->usage.prev_time = time(0);
    new_block->is_dirty = false;
    new_block->next_block = cache->head;

    cache->head = new_block;
    cache->current_blocks++;
    return 0;
}

int compare_times(time_t time1, time_t time2) {
    double difference = difftime(time1, time2);

    if (difference > 0) {
        return 1;
    } else if (difference < 0) {
        return -1;
    }

    return 0;
}