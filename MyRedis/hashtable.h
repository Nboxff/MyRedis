#ifndef _HASHTABLE_H
#define _HASHTABLE_H

#include <stddef.h>
#include <stdint.h>

// hashtable node
struct HNode {
    HNode *next = NULL;
    uint64_t hcode = 0;
};

// a simple fixed-size hashtable
struct HTab {
    HNode **tab = NULL;
    size_t mask = 0;
    size_t size = 0;
};

/**
 * final hashtable interface
*/
struct HMap {
    HTab ht1;
    HTab ht2;
    size_t resizing_pos = 0;
};

HNode *hm_lookup(HMap *hmap, HNode *key, bool (*cmp)(HNode *, HNode *));

void hm_insert(HMap *hmap, HNode *node);

HNode *hm_pop(HMap *hmap, HNode *key, bool (*cmp)(HNode *, HNode *));

size_t hm_size(HMap *hmap);

void hm_destroy(HMap *hmap);

#endif