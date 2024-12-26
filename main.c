#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#include "utils.h"
#include "lru.h"

#define BLOCK_SIZE 4096
#define MAX_BLOCKS 4

int bin_search(const char *filename, int target) {
    FILE *file = fopen(filename, "rb");
    if (!file) {
        perror("[Ошибка] Не удалось открыть двоичный файл для чтения");
        return -1;
    }

    fseek(file, 0, SEEK_END);
    long file_size = ftell(file);
    if (file_size < 0) {
        perror("[Ошибка] Не удалось определить размер файла");
        fclose(file);
        return -1;
    }

    int total_ints = file_size / sizeof(int);

    int left = 0;
    int right = total_ints - 1;

    int found = 0;
    while (left <= right) {
        int mid = left + (right - left) / 2;
        fseek(file, mid * sizeof(int), SEEK_SET);
        int value;
        if (fread(&value, sizeof(int), 1, file) != 1) {
            perror("[Ошибка] Не удалось прочитать значение");
            fclose(file);
            return -1;
        }

        if (value == target) {
            printf("Число %d найдено на позиции %d (без кэша)\n", target, mid);
            found = 1;
            break;
        } else if (value < target) {
            left = mid + 1;
        } else {
            right = mid - 1;
        }
    }

    if (!found) {
        printf("Число %d не найдено (без кэша)\n", target);
    }

    fclose(file);
    return 0;
}

int bin_search_lru_cache_impl(const char *filename, int target) {
    file_cache *cache = open_file(filename, BLOCK_SIZE, MAX_BLOCKS);
    if (!cache) {
        perror("[Ошибка] Не удалось открыть двоичный файл через LRU-кэш");
        return -1;
    }

    off_t file_size = lseek(cache->fd, 0, SEEK_END);
    if (file_size < 0) {
        perror("[Ошибка] Не удалось определить размер файла");
        close_file(cache);
        return -1;
    }

    int total_ints = file_size / sizeof(int);

    int left = 0;
    int right = total_ints - 1;

    int found = 0;
    while (left <= right) {
        int mid = left + (right - left) / 2;
        int value;
        ssize_t bytes_read = read_file(cache, &value, sizeof(int), mid * sizeof(int));
        if (bytes_read < (ssize_t)sizeof(int)) {
            perror("[Ошибка] Не удалось прочитать значение из кэша");
            break;
        }

        if (value == target) {
            printf("Число %d найдено на позиции %d (с LRU-кэшем)\n", target, mid);
            found = 1;
            break;
        } else if (value < target) {
            left = mid + 1;
        } else {
            right = mid - 1;
        }
    }

    if (!found) {
        printf("Число %d не найдено (с LRU-кэшем)\n", target);
    }

    close_file(cache);
    return 0;
}

static void remove_newline(char *str) {
    size_t len = strlen(str);
    if (len > 0 && (str[len - 1] == '\n' || str[len - 1] == '\r')) {
        str[len - 1] = '\0';
    }
}

static int is_line_unique(char **lines, size_t count, const char *line) {
    for (size_t i = 0; i < count; i++) {
        if (strcmp(lines[i], line) == 0) {
            return 0;
        }
    }
    return 1;
}

static void add_line(char ***lines, size_t *count, size_t *capacity, const char *new_line) {
    if (*count >= *capacity) {
        size_t new_capacity = (*capacity == 0) ? 16 : (*capacity * 2);
        char **tmp = realloc(*lines, new_capacity * sizeof(char *));
        if (!tmp) {
            perror("[Ошибка] Не удалось увеличить массив строк");
            return;
        }
        *lines = tmp;
        *capacity = new_capacity;
    }

    (*lines)[*count] = strdup(new_line);
    if (!(*lines)[*count]) {
        perror("[Ошибка] Не удалось выделить память под строку");
        return;
    }
    (*count)++;
}

int deduplicate_no_cache(const char *in_filename, const char *out_filename) {
    FILE *in_file = fopen(in_filename, "r");
    if (!in_file) {
        perror("[Ошибка] Не удалось открыть входной TXT для чтения");
        return -1;
    }

    FILE *out_file = fopen(out_filename, "w");
    if (!out_file) {
        perror("[Ошибка] Не удалось открыть выходной TXT для записи");
        fclose(in_file);
        return -1;
    }

    char **unique_lines = NULL;
    size_t lines_count = 0;
    size_t lines_capacity = 0;

    char buffer[1024];
    while (fgets(buffer, sizeof(buffer), in_file)) {
        remove_newline(buffer);
        if (is_line_unique(unique_lines, lines_count, buffer)) {
            add_line(&unique_lines, &lines_count, &lines_capacity, buffer);
            fprintf(out_file, "%s\n", buffer);
        }
    }

    for (size_t i = 0; i < lines_count; i++) {
        free(unique_lines[i]);
    }
    free(unique_lines);

    fclose(in_file);
    fclose(out_file);

    return 0;
}

static ssize_t read_line_from_cache(file_cache *cache, char *line, size_t size, off_t *offset) {
    if (size == 0) {
        return -1;
    }

    size_t i = 0;
    while (i < size - 1) {
        char c;
        ssize_t bytes_read = read_file(cache, &c, 1, *offset);
        if (bytes_read < 0) {
            return -1;
        } else if (bytes_read == 0) {
            break;
        }
        (*offset)++;

        if (c == '\n') {
            break;
        }
        line[i++] = c;
    }
    line[i] = '\0';
    return i;
}

int deduplicate_lru_cache_impl(const char *in_filename, const char *out_filename) {
    file_cache *cache = open_file(in_filename, BLOCK_SIZE, MAX_BLOCKS);
    if (!cache) {
        perror("[Ошибка] Не удалось открыть TXT через LRU-кэш");
        return -1;
    }

    FILE *out_file = fopen(out_filename, "w");
    if (!out_file) {
        perror("[Ошибка] Не удалось открыть выходной TXT для записи");
        close_file(cache);
        return -1;
    }

    char **unique_lines = NULL;
    size_t lines_count = 0;
    size_t lines_capacity = 0;

    char buffer[1024];
    off_t offset = 0;

    while (1) {
        ssize_t len = read_line_from_cache(cache, buffer, sizeof(buffer), &offset);
        if (len < 0) {
            perror("[Ошибка] Не удалось корректно прочитать строку из кэша");
            break;
        } else if (len == 0) {
            break;
        }
        remove_newline(buffer);

        if (is_line_unique(unique_lines, lines_count, buffer)) {
            add_line(&unique_lines, &lines_count, &lines_capacity, buffer);
            fprintf(out_file, "%s\n", buffer);
        }
    }

    for (size_t i = 0; i < lines_count; i++) {
        free(unique_lines[i]);
    }
    free(unique_lines);

    fclose(out_file);
    close_file(cache);

    return 0;
}

int main() {
    clock_t start_time, end_time;

    start_time = clock();
    bin_search("../test.bin", 100);
    end_time = clock();
    printf("Бинарный поиск БЕЗ кеша: %f секунд\n", (double)(end_time - start_time) / CLOCKS_PER_SEC);

    start_time = clock();
    bin_search_lru_cache_impl("../test.bin", 100);
    end_time = clock();
    printf("Бинарный поиск С использованием LRU-кеша: %f секунд\n", (double)(end_time - start_time) / CLOCKS_PER_SEC);

    start_time = clock();
    deduplicate_no_cache("../test.txt", "../deduplicated_nocache.txt");
    end_time = clock();
    printf("Дедупликация БЕЗ кеша: %f секунд\n", (double)(end_time - start_time) / CLOCKS_PER_SEC);

    start_time = clock();
    deduplicate_lru_cache_impl("../test.txt", "../deduplicated_cache.txt");
    end_time = clock();
    printf("Дедупликация С использованием LRU-кеша: %f секунд\n", (double)(end_time - start_time) / CLOCKS_PER_SEC);

    return 0;
}