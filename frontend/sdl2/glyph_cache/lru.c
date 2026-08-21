#include "lru.h"
#include <cluterm/debug.h>

struct Node {
    Key key;
    Value value;
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

// @NOTE: Assuming key doesn't exist (for updating simple update the node *).
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

static inline void node_attach(LRU *lru, Node *node)
{
    if (node) {
        if ((node->next = lru->head))
            node->next->prev = node;
        else
            lru->stale = node;
        lru->head = node, lru->capacity--;
    }
}

static inline Node *node_detach(LRU *lru, Node *node)
{
    if (node) {
        if (node->next)
            node->next->prev = node->prev;
        else
            lru->stale = node->prev;
        if (node->prev)
            node->prev->next = node->next;
        else
            lru->head = node->next;
        node->next = node->prev = NULL, lru->capacity++;
    }
    return node;
}

Value lru_get(LRU *lru, Key key)
{
    Node *node = ht_get(lru->table, key, lru->key_eq);
    node_attach(lru, node_detach(lru, node));
    return node ? node->value : NULL;
}

static inline Node *evict(LRU *lru)
{
    Node *node = lru->stale;
    if ((node = node_detach(lru, node)))
        ht_remove(lru->table, node->key, lru->key_eq);
    return node;
}

Value lru_put(LRU *lru, Key key, Value value)
{
    Node *node      = node_detach(lru, ht_get(lru->table, key, lru->key_eq));
    Value old_value = NULL;
    if (!node) {
        node      = lru->capacity ? calloc(1, sizeof(Node)) : evict(lru);
        old_value = node->value;
        node->key = key;
        ht_set(lru->table, key, node);
    }
    node->value = value;
    node_attach(lru, node);
    return old_value;
}

Value lru_evict(LRU *lru)
{
    Value value = 0;
    Node *node;
    if ((node = evict(lru))) {
        value = node->value;
        free(node);
    }
    return value;
}
