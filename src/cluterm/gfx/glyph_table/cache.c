#include "cache.h"

struct Node {
    Key key;
    Value *value;
    struct Node *next, *prev;
};

struct Bucket {
    Key key;
    Node *node;
    struct Bucket *next;
};

static inline Node *ht_get(GlyphCache *gcache, Key key)
{
    Bucket *bucket = gcache->table[key.value % MAP_MAX_SIZE];
    for (; bucket; bucket = bucket->next)
        if (gcache->key_eq(bucket->key, key))
            break;
    return bucket ? bucket->node : NULL;
}

// @NOTE: Assuming key doesn't exist (check with 'ht_get' before calling this).
static inline void ht_set(GlyphCache *gcache, Key key, Node *node)
{
    Bucket **head  = &gcache->table[key.value % MAP_MAX_SIZE],
           *bucket = malloc(sizeof(Bucket));
    bucket->key = key, bucket->node = node, bucket->next = *head,
    *head = bucket;
}

static inline void ht_remove(GlyphCache *gcache, Key key)
{
    Bucket *current = gcache->table[key.value % MAP_MAX_SIZE], *previous = NULL;
    for (; current; previous = current, current = current->next) {
        if (gcache->key_eq(current->key, key)) {
            if (previous)
                previous->next = current->next;
            else
                gcache->table[key.value % MAP_MAX_SIZE] = current->next;
            free(current);
            return;
        }
    }
}

static inline void node_attach(GlyphCache *gcache, Node *node)
{
    if (node) {
        if ((node->next = gcache->head))
            node->next->prev = node;
        else
            gcache->stale = node;
        gcache->head = node, gcache->capacity--;
    }
}

static inline Node *node_detach(GlyphCache *gcache, Node *node)
{
    if (node) {
        if (node->next)
            node->next->prev = node->prev;
        else
            gcache->stale = node->prev;
        if (node->prev)
            node->prev->next = node->next;
        else
            gcache->head = node->next;
        node->next = NULL, node->prev = NULL, gcache->capacity++;
    }
    return node;
}

static inline void node_free(GlyphCache *gcache, Node *node)
{
    if (node) {
        ht_remove(gcache, node->key);
        node_detach(gcache, node);
        gcache->value_dealloc(node->value);
        free(node);
    }
}

Value *gcache_get(GlyphCache *gcache, Key key)
{
    Node *node = ht_get(gcache, key);
    node_attach(gcache, node_detach(gcache, node));
    return node ? node->value : NULL;
}

void gcache_put(GlyphCache *gcache, Key key, Value *value)
{
    Node *node = ht_get(gcache, key);
    if (!node) {
        if (!gcache->capacity)
            node_free(gcache, gcache->stale);
        node      = malloc(sizeof(Node));
        node->key = key, node->next = node->prev = NULL;
        ht_set(gcache, key, node);
        node_attach(gcache, node);
    }
    node->value = value;
}

void gcache_clear(GlyphCache *gcache)
{
    for (Node *node = gcache->head; node;) {
        Node *stale = node;
        node        = node->next;
        node_free(gcache, stale);
    }
    /* @DEBUG */ {
        int buckets = 0;
        for (int i = 0; i < MAP_MAX_SIZE; ++i)
            buckets += (gcache->table[i] != NULL);
        printf("possible memory leak in hash-table: %d.\n", buckets);
    }
}
