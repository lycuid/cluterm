#ifndef __RENDERER__GLYPH__CACHE_H__
#define __RENDERER__GLYPH__CACHE_H__

#include <cluterm/gfx/glyph_table.h>
#include <cluterm/terminal/buffer.h>
#include <stdbool.h>

#define MAP_MAX_SIZE (1 << 4)

typedef Cell Key;
typedef Glyph Value;

typedef struct Node Node;
typedef bool (*KeyEq)(Key, Key);
typedef void (*ValueDealloc)(Value *);
typedef struct Bucket Bucket;
typedef Bucket *HashTable[MAP_MAX_SIZE];

typedef struct GlyphCache {
    size_t capacity;
    Node *head, *stale;
    HashTable table;
    KeyEq key_eq;
    ValueDealloc value_dealloc;
} GlyphCache;

void gcache_put(GlyphCache *, Key, Value *);
Value *gcache_get(GlyphCache *, Key);
void gcache_clear(GlyphCache *);

#endif
