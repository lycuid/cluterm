#ifndef __SDL2__GLYPH_CACHE__LRU_H__
#define __SDL2__GLYPH_CACHE__LRU_H__

#include <cluterm/vt/buffer.h>
#include <stdbool.h>

#define MAP_MAX_SIZE (1 << 6)

typedef Cell Key;
typedef void *Value;

typedef struct Node Node;
typedef bool (*KeyEq)(Key, Key);
typedef struct Bucket Bucket;
typedef Bucket *HashTable[MAP_MAX_SIZE];

typedef struct LRU {
    size_t capacity;
    Node *head, *stale;
    HashTable table;
    KeyEq key_eq;
} LRU;

Value lru_get(LRU *, Key);
Value lru_put(LRU *, Key, Value);
Value lru_evict(LRU *);

#endif
