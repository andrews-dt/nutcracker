#ifndef _NC_RBTREE_
#define _NC_RBTREE_

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>
#include <inttypes.h>
#include <string.h>
#include <stdio.h>
#include <ctype.h>
#include <errno.h>
#include <limits.h>
#include <time.h>
#include <unistd.h>

#define rbtree_red(_node)           ((_node)->color = 1)
#define rbtree_black(_node)         ((_node)->color = 0)
#define rbtree_is_red(_node)        ((_node)->color)
#define rbtree_is_black(_node)      (!rbtree_is_red(_node))
#define rbtree_copy_color(_n1, _n2) ((_n1)->color = (_n2)->color)

class rbnode 
{
public:
    rbnode() : left(NULL), right(NULL), parent(NULL),
        key(0LL), data(NULL), color(0)
    { }

    void reset()
    {
        left = NULL;
        right = NULL;
        parent = NULL;
        key = 0LL;
        data = NULL;
        color = 0;
    }

    void setData(void *_data)
    {
        data = _data;
    }

public:
    rbnode      *left;     /* left link */
    rbnode      *right;    /* right link */
    rbnode      *parent;   /* parent link */
    int64_t     key;       /* key for ordering */
    void        *data;     /* opaque data */
    uint8_t     color;     /* red | black */
};

class NcRbTree
{
public:
    NcRbTree() : m_root_(NULL), m_sentinel_(NULL)
    { }

    NcRbTree(rbnode *node)
    {
        rbtree_black(node); // 根节点是黑色节点
        m_root_ = node;
        m_sentinel_ = node;
    }

    rbnode* min()
    {
        rbnode *node = m_root_;
        rbnode *sentinel = m_sentinel_;

        /* empty tree */
        if (node == sentinel) 
        {
            return NULL;
        }

        while (node->left != sentinel) 
        {
            node = node->left;
        }

        return node;
    }

    rbnode* max()
    {
        rbnode *node = m_root_;
        rbnode *sentinel = m_sentinel_;

        /* empty tree */
        if (node == sentinel) 
        {
            return NULL;
        }

        if (node->right != NULL)
        {
            while (node->right != sentinel) 
            {
                node = node->right;
            }
        }
        
        return node;
    }

    // 左旋转
    void leftRotate(rbnode *node)
    {
        if (node == NULL)
        {
            return ;
        }

        rbnode *temp;

        temp = node->right;
        node->right = temp->left;

        if (temp->left != m_sentinel_) 
        {
            temp->left->parent = node;
        }

        temp->parent = node->parent;

        if (node == m_root_) 
        {
            m_root_ = temp;
        } 
        else if (node == node->parent->left) 
        {
            node->parent->left = temp;
        } 
        else 
        {
            node->parent->right = temp;
        }

        temp->left = node;
        node->parent = temp;
    }

    // 右旋转
    void rightRotate(rbnode *node)
    {
        if (node == NULL)
        {
            return ;
        }

        rbnode *temp;

        temp = node->left;
        node->left = temp->right;

        if (temp->right != m_sentinel_) 
        {
            temp->right->parent = node;
        }

        temp->parent = node->parent;

        if (node == m_root_) 
        {
            m_root_ = temp;
        } 
        else if (node == node->parent->right) 
        {
            node->parent->right = temp;
        } 
        else 
        {
            node->parent->left = temp;
        }

        temp->right = node;
        node->parent = temp;
    }

    // 增加
    void insert(rbnode *node)
    {
        rbnode **root = &m_root_;
        rbnode *sentinel = m_sentinel_;
        rbnode *temp, **p;

        if (*root == sentinel) 
        {
            node->parent = NULL;
            node->left = sentinel;
            node->right = sentinel;
            rbtree_black(node);
            *root = node;

            return;
        }

        temp = *root;
        for (;;)
        {
            p = (node->key < temp->key) ? &temp->left : &temp->right;
            if (*p == sentinel) 
            {
                break;
            }

            temp = *p;
        }

        *p = node;
        node->parent = temp;
        node->left = sentinel;
        node->right = sentinel;
        rbtree_red(node);

        // 调整为平衡二叉树
        while (node != *root && rbtree_is_red(node->parent)) 
        {
            if (node->parent == node->parent->parent->left) 
            {
                temp = node->parent->parent->right;

                if (rbtree_is_red(temp)) 
                {
                    rbtree_black(node->parent);
                    rbtree_black(temp);
                    rbtree_red(node->parent->parent);
                    node = node->parent->parent;
                } 
                else 
                {
                    if (node == node->parent->right) 
                    {
                        node = node->parent;
                        leftRotate(node);
                    }

                    rbtree_black(node->parent);
                    rbtree_red(node->parent->parent);
                    rightRotate(node->parent->parent);
                }
            } 
            else 
            {
                temp = node->parent->parent->left;

                if (rbtree_is_red(temp)) 
                {
                    rbtree_black(node->parent);
                    rbtree_black(temp);
                    rbtree_red(node->parent->parent);
                    node = node->parent->parent;
                } 
                else 
                {
                    if (node == node->parent->left) 
                    {
                        node = node->parent;
                        rightRotate(node);
                    }

                    rbtree_black(node->parent);
                    rbtree_red(node->parent->parent);
                    leftRotate(node->parent->parent);
                }
            }
        }

        rbtree_black(*root);
    }

    // 删除
    void remove(rbnode *node)
    {
        if (node == NULL)
        {
            return ;
        }
        
        rbnode **root = &m_root_;
        rbnode *sentinel = m_sentinel_;
        rbnode *subst, *temp, *w;
        uint8_t red;

        if (node->left == sentinel) 
        {
            temp = node->right;
            subst = node;
        } 
        else if (node->right == sentinel) 
        {
            temp = node->left;
            subst = node;
        } 
        else 
        {
            subst = min();
            temp = subst->right;
        }

        if (subst == NULL)
        {
            return ;
        }

        if (subst == *root) 
        {
            *root = temp;
            rbtree_black(temp);
            node->reset();
            return ;
        }

        if (subst->parent == NULL)
        {
            return ;
        }

        red = rbtree_is_red(subst);
        if (subst == subst->parent->left) 
        {
            subst->parent->left = temp;
        } 
        else 
        {
            subst->parent->right = temp;
        }

        if (subst == node) 
        {
            temp->parent = subst->parent;
        } 
        else 
        {
            if (subst->parent == node) 
            {
                temp->parent = subst;
            } 
            else 
            {
                temp->parent = subst->parent;
            }

            subst->left = node->left;
            subst->right = node->right;
            subst->parent = node->parent;
            rbtree_copy_color(subst, node);

            if (node == *root) 
            {
                *root = subst;
            } 
            else 
            {
                if (node == node->parent->left) 
                {
                    node->parent->left = subst;
                } 
                else 
                {
                    node->parent->right = subst;
                }
            }

            if (subst->left != sentinel) 
            {
                subst->left->parent = subst;
            }

            if (subst->right != sentinel) 
            {
                subst->right->parent = subst;
            }
        }

        node->reset();

        if (red) 
        {
            return;
        }

        while (temp != *root && rbtree_is_black(temp)) 
        {
            if (temp == temp->parent->left) 
            {
                w = temp->parent->right;

                if (rbtree_is_red(w)) 
                {
                    rbtree_black(w);
                    rbtree_red(temp->parent);
                    leftRotate(temp->parent);
                    w = temp->parent->right;
                }

                if (rbtree_is_black(w->left) && rbtree_is_black(w->right)) 
                {
                    rbtree_red(w);
                    temp = temp->parent;
                } 
                else 
                {
                    if (rbtree_is_black(w->right)) 
                    {
                        rbtree_black(w->left);
                        rbtree_red(w);
                        rightRotate(w);
                        w = temp->parent->right;
                    }

                    rbtree_copy_color(w, temp->parent);
                    rbtree_black(temp->parent);
                    rbtree_black(w->right);
                    leftRotate(temp->parent);
                    temp = *root;
                }
            } 
            else 
            {
                w = temp->parent->left;

                if (rbtree_is_red(w)) 
                {
                    rbtree_black(w);
                    rbtree_red(temp->parent);
                    rightRotate(temp->parent);
                    w = temp->parent->left;
                }

                if (rbtree_is_black(w->left) && rbtree_is_black(w->right)) 
                {
                    rbtree_red(w);
                    temp = temp->parent;
                } 
                else 
                {
                    if (rbtree_is_black(w->left)) 
                    {
                        rbtree_black(w->right);
                        rbtree_red(w);
                        leftRotate(w);
                        w = temp->parent->left;
                    }

                    rbtree_copy_color(w, temp->parent);
                    rbtree_black(temp->parent);
                    rbtree_black(w->left);
                    rightRotate(temp->parent);
                    temp = *root;
                }
            }
        }

        rbtree_black(temp);
    }

private:
    rbnode *m_root_;
    rbnode *m_sentinel_; /* nil node */
};

#endif
