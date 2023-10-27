#ifndef _AVL_H
#define _AVL_H

#include <stddef.h>
#include <stdint.h>

struct AVLNode {
    uint32_t depth = 0;  // the height of the tree
    uint32_t cnt = 0;  // the size of the tree
    AVLNode *left = NULL;
    AVLNode *right = NULL;
    AVLNode *parent = NULL;
};

#endif