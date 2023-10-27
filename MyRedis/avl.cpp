#include "avl.h"

static void avl_init(AVLNode *node) {
    node->depth = 1;
    node->cnt = 1;
    node->left = node->right = node->parent = NULL;
}

static uint32_t avl_depth(AVLNode *node) {
    return node ? node->depth : 0;
}

static uint32_t avl_cnt(AVLNode *node) {
    return node ? node->cnt : 0;
}

static uint32_t max(uint32_t lhs, uint32_t rhs) {
    return lhs < rhs ? rhs : lhs;
}

// maintaining the depth and cnt field
static void avl_update(AVLNode *node) {
    node->depth = 1 + max(avl_depth(node->left), avl_depth(node->right));
    node->cnt = 1 + avl_cnt(node->left) + avl_cnt(node->right);
}

/**
 * @return the pointer to the new root node
*/
static AVLNode *rot_left(AVLNode *node) {
    AVLNode* new_root = node->right;
    if (new_root->left != NULL) {
        new_root->left->parent = node;
    }
    node->right = new_root->left;
    new_root->left = node;
    new_root->parent = node->parent;
    node->parent = new_root;

    avl_update(node);
    avl_update(new_root);
    return new_root;
}

static AVLNode *rot_right(AVLNode *node) {
    AVLNode *new_root = node->left;
    if (new_root->right != NULL) {
        new_root->right->parent = node;
    }
    node->left = new_root->right;
    new_root->right = node;
    new_root->parent = node->parent;
    node->parent = new_root;

    avl_update(node);
    avl_update(new_root);
    return new_root;
}

// the left subtree is too deep
static AVLNode *avl_fix_left(AVLNode *root) {
    if (avl_depth(root->left->left) < avl_depth(root->left->right)) {
        root->left = rot_left(root->left);
    }
    return rot_right(root);
}

// the right subtree is too deep
static AVLNode *avl_fix_right(AVLNode *root) {
    if (avl_depth(root->right->right) < avl_depth(root->right->left)) {
        root->right = rot_right(root->right);
    }
    return rot_left(root);
}

// fix imbalanced nodes and maintain invariants until the root is reached
static AVLNode *avl_fix(AVLNode *node) {
    while (true) {
        avl_update(node);
        uint32_t lHight = avl_depth(node->left);
        uint32_t rHight = avl_depth(node->right);

        AVLNode **from = NULL;
        if (node->parent != NULL) {
            if (node->parent->left == node) {
                from = &node->parent->left;
            } else {
                from = &node->parent->right;
            }
        }
        if (lHight == rHight + 2) {
            node = avl_fix_left(node);
        } else if (rHight == lHight + 2) {
            node = avl_fix_right(node);
        }

        if (!from) {
            return node;
        }
        *from = node;
        node = node->parent;
    }
}

AVLNode *avl_del(AVLNode *node) {
    if (node->right == NULL) {
        AVLNode *parent = node->parent;
        if (node->left != NULL) {
            node->left->parent = parent;
        }

        if (parent != NULL) {
            (parent->left == node ? parent->left : parent->right) = node->left;
            return avl_fix(parent);
        } else {
            return node->left;
        }
    } else {
        AVLNode *victim = node->right;
        while (victim->left) {
            victim = victim->left;
        }
        AVLNode *root = avl_del(victim);

        *victim = *node;
        if (victim->left != NULL) {
            victim->left->parent = victim; // victim is a address
        }
        if (victim->right != NULL) {
            victim->right->parent = victim;
        }
        AVLNode *parent = node->parent;
        if (parent != NULL) {
            (parent->left == node ? parent->left : parent->right) = victim;
            return root;
        } else {
            return victim;
        }
    }
}