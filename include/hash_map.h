#ifndef HL_HASH_MAP_H
#define HL_HASH_MAP_H

#include <stddef.h>

/*
 * Open-addressing hash map: string key → int.
 *
 * hl_hash_map_create(max_entries): initial bucket count is derived from
 * max_entries so the table starts sparse; the table grows automatically
 * (load factor capped at ~2/3). Average-case time is O(1) per put/get;
 * worst-case O(n) is possible if many keys collide (rare with FNV-1a).
 *
 * Keys are copied on insert; the map owns them until hl_hash_map_destroy.
 */

typedef struct hl_hash_map hl_hash_map_t;

hl_hash_map_t *hl_hash_map_create(size_t max_entries);
void hl_hash_map_destroy(hl_hash_map_t *m);

void hl_hash_map_put(hl_hash_map_t *m, const char *key, int value);
int hl_hash_map_get(const hl_hash_map_t *m, const char *key);

#endif /* HL_HASH_MAP_H */
