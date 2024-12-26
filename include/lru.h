#ifndef LRU_H
#define LRU_H

#include "utils.h"

file_cache* open_file(const char *path, const size_t block_size, const size_t max_blocks);
int close_file(file_cache *cache);
int fsync_file(file_cache *cache);
ssize_t write_file(file_cache *cache, const void *buf, ssize_t count, off_t offset);
ssize_t read_file(file_cache *cache, void *buf, ssize_t count, off_t offset);
off_t lseek_file(file_cache *cache, const off_t offset, const int whence);

#endif //LRU_H
