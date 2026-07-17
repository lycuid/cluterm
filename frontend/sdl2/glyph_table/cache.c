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

static inline Node *ht_get(HashTable table, Key key, KeyEq key_eq)
{
    Bucket *bucket = table[key.value % MAP_MAX_SIZE];
    for (; bucket; bucket = bucket->next)
        if (key_eq(bucket->key, key))
            break;
    return bucket ? bucket->node : NULL;
}

// @NOTE: Assuming key doesn't exist (check with 'ht_get' before calling this).
static inline void ht_set(HashTable table, Key key, Node *node)
{
    Bucket **head  = &table[key.value % MAP_MAX_SIZE],
           *bucket = malloc(sizeof(Bucket));
    bucket->key = key, bucket->node = node, bucket->next = *head,
    *head = bucket;
}

static inline void ht_remove(HashTable table, Key key, KeyEq key_eq)
{
    Bucket *current = table[key.value % MAP_MAX_SIZE], *previous = NULL;
    for (; current; previous = current, current = current->next) {
        if (key_eq(current->key, key)) {
            if (previous)
                previous->next = current->next;
            else
                table[key.value % MAP_MAX_SIZE] = current->next;
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
        node->next = node->prev = NULL, gcache->capacity++;
    }
    return node;
}

static inline void node_free(GlyphCache *gcache, Node *node)
{
    if ((node = node_detach(gcache, node))) {
        ht_remove(gcache->table, node->key, gcache->key_eq);
        gcache->value_dealloc(node->value);
        free(node);
    }
}

Value *gcache_get(GlyphCache *gcache, Key key)
{
    Node *node = ht_get(gcache->table, key, gcache->key_eq);
    node_attach(gcache, node_detach(gcache, node));
    return node ? node->value : NULL;
}

void gcache_put(GlyphCache *gcache, Key key, Value *value)
{
    Node *node =
        node_detach(gcache, ht_get(gcache->table, key, gcache->key_eq));
    if (!node) {
        if (!gcache->capacity)
            node_free(gcache, gcache->stale);
        node      = malloc(sizeof(Node));
        node->key = key, node->next = node->prev = NULL;
        ht_set(gcache->table, key, node);
    }
    node->value = value;
    node_attach(gcache, node);
}

void gcache_clear(GlyphCache *gcache)
{
    while (gcache->head)
        node_free(gcache, gcache->head);
    /* @DEBUG */ {
        int buckets = 0;
        for (int i = 0; i < MAP_MAX_SIZE; ++i)
            buckets += (gcache->table[i] != NULL);
        printf("possible memory leak in hash-table: %d.\n", buckets);
    }
}
