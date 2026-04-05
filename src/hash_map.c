/*
 * Open addressing, linear probing, power-of-two bucket count.
 * FNV-1a 64-bit hash; grow when load factor would exceed LOAD_NUM/LOAD_DEN.
 */

#include "hash_map.h"

#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#define LOAD_NUM 2u
#define LOAD_DEN 3u /* grow before (count + 1) * LOAD_DEN > n_buckets * LOAD_NUM */

typedef struct {
    char *key;
    int   value;
} hl_entry_t;

struct hl_hash_map {
    hl_entry_t *entries;
    size_t      n_buckets;
    size_t      count;
};

static char *hl_strdup(const char *s) {
    size_t n = strlen(s) + 1;
    char  *p = (char *)malloc(n);
    if (p) memcpy(p, s, n);
    return p;
}

static uint64_t hash_string(const char *s) {
    uint64_t h = 14695981039346656037ULL;
    while (*s) {
        h ^= (unsigned char)(*s++);
        h *= 1099511628211ULL;
    }
    return h;
}

static size_t next_pow2_size(size_t x) {
    if (x <= 1) return 1;
    size_t p = 1;
    while (p < x) {
        if (p > SIZE_MAX / 2) return SIZE_MAX;
        p <<= 1;
    }
    return p;
}

static hl_entry_t *alloc_buckets(size_t n) {
    return (hl_entry_t *)calloc(n, sizeof(hl_entry_t));
}

static void free_all_keys(hl_hash_map_t *m) {
    if (!m->entries) return;
    for (size_t i = 0; i < m->n_buckets; i++) free(m->entries[i].key);
}

static int rehash(hl_hash_map_t *m, size_t new_min) {
    size_t new_n = next_pow2_size(new_min);
    if (new_n < 8) new_n = 8;

    hl_entry_t *new_e = alloc_buckets(new_n);
    if (!new_e) return -1;

    hl_entry_t *old_e       = m->entries;
    size_t      old_buckets = m->n_buckets;

    m->entries   = new_e;
    m->n_buckets = new_n;
    m->count     = 0;

    if (old_e) {
        for (size_t i = 0; i < old_buckets; i++) {
            if (!old_e[i].key) continue;
            size_t h   = (size_t)hash_string(old_e[i].key);
            size_t idx = h & (new_n - 1);
            while (m->entries[idx].key) idx = (idx + 1) & (new_n - 1);
            m->entries[idx].key   = old_e[i].key;
            m->entries[idx].value = old_e[i].value;
            m->count++;
        }
        free(old_e);
    }
    return 0;
}

hl_hash_map_t *hl_hash_map_create(size_t max_entries) {
    if (max_entries == 0) return NULL;

    hl_hash_map_t *m = (hl_hash_map_t *)calloc(1, sizeof(hl_hash_map_t));
    if (!m) return NULL;

    /* Sparse table: ~2/3 max load before grow; at least 8 buckets */
    size_t buckets = next_pow2_size(max_entries * LOAD_DEN / LOAD_NUM + 8);
    if (buckets < 8) buckets = 8;

    m->entries = alloc_buckets(buckets);
    if (!m->entries) {
        free(m);
        return NULL;
    }
    m->n_buckets = buckets;
    return m;
}

void hl_hash_map_destroy(hl_hash_map_t *m) {
    if (!m) return;
    free_all_keys(m);
    free(m->entries);
    free(m);
}

static size_t find_slot(const hl_hash_map_t *m, const char *key, int *found) {
    size_t h   = (size_t)hash_string(key);
    size_t idx = h & (m->n_buckets - 1);
    for (;;) {
        if (!m->entries[idx].key) {
            *found = 0;
            return idx;
        }
        if (strcmp(m->entries[idx].key, key) == 0) {
            *found = 1;
            return idx;
        }
        idx = (idx + 1) & (m->n_buckets - 1);
    }
}

void hl_hash_map_put(hl_hash_map_t *m, const char *key, int value) {
    if (!m || !key) return;

    if (m->count + 1 > (m->n_buckets * LOAD_NUM) / LOAD_DEN) {
        if (rehash(m, m->n_buckets * 2) != 0) return;
    }

    int    found;
    size_t idx = find_slot(m, key, &found);
    if (found) {
        m->entries[idx].value = value;
        return;
    }

    char *copy = hl_strdup(key);
    if (!copy) return;

    m->entries[idx].key   = copy;
    m->entries[idx].value = value;
    m->count++;
}

int hl_hash_map_get(const hl_hash_map_t *m, const char *key) {
    if (!m || !key) return -1;
    int    found;
    size_t idx = find_slot(m, key, &found);
    if (!found) return -1;
    return m->entries[idx].value;
}
