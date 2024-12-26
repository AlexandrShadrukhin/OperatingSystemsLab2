#ifndef API_H
#define API_H

#include "utils.h"

file_cache* lab2_open(const char *path, const size_t block_size, const size_t max_blocks);
int lab2_close(file_cache *cache);
int lab2_fsync(file_cache *cache);
ssize_t lab2_write(file_cache *cache, const void *buf, ssize_t count, off_t offset);
ssize_t lab2_read(file_cache *cache, void *buf, ssize_t count, off_t offset);
off_t lab2_lseek(file_cache *cache, const off_t offset, const int whence);

#endif //API_H
