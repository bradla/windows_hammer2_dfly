/*
 * hammer2_rbtree_windows.h - Linux <linux/rbtree.h>-compatible red-black tree
 * for the HAMMER2 Windows port.
 *
 * HAMMER2's generic tree macros (hammer2_rb.h) are written against the Linux
 * kernel rbtree API: struct rb_node / rb_root, rb_link_node(),
 * rb_insert_color(), rb_erase(), rb_first/last/next/prev(), rb_entry(),
 * RB_ROOT, and RB_EMPTY_ROOT().  This header provides a self-contained,
 * standard top-down/bottom-up red-black tree with exactly that surface so the
 * macros compile and behave identically on Windows.
 *
 * The node stores its parent pointer and color packed into a single word, as
 * the Linux implementation does, so element struct layouts are unchanged.
 */

#ifndef _HAMMER2_RBTREE_WINDOWS_H_
#define _HAMMER2_RBTREE_WINDOWS_H_

#ifdef _WIN32

#include "hammer2_windows_port.h"
#include <stdint.h>

struct rb_node {
	uintptr_t		__rb_parent_color;
	struct rb_node		*rb_right;
	struct rb_node		*rb_left;
};

struct rb_root {
	struct rb_node		*rb_node;
};

#define RB_RED		0
#define RB_BLACK	1

#define RB_ROOT			((struct rb_root){ NULL })
#define rb_entry(ptr, type, member)	container_of(ptr, type, member)
#define RB_EMPTY_ROOT(root)		((root)->rb_node == NULL)
#define RB_EMPTY_NODE(node)		\
	((node)->__rb_parent_color == (uintptr_t)(node))
#define RB_CLEAR_NODE(node)		\
	((node)->__rb_parent_color = (uintptr_t)(node))

#define __rb_parent(pc)		((struct rb_node *)((pc) & ~(uintptr_t)3))
#define rb_parent(n)		((struct rb_node *)((n)->__rb_parent_color & ~(uintptr_t)3))
#define __rb_color(pc)		((int)((pc) & 1))
#define rb_color(n)		__rb_color((n)->__rb_parent_color)
#define rb_is_red(n)		(!rb_color(n))
#define rb_is_black(n)		(rb_color(n))

static inline void
rb_set_parent(struct rb_node *n, struct rb_node *p)
{
	n->__rb_parent_color = (n->__rb_parent_color & 1) | (uintptr_t)p;
}

static inline void
rb_set_parent_color(struct rb_node *n, struct rb_node *p, int color)
{
	n->__rb_parent_color = (uintptr_t)p | (uintptr_t)color;
}

static inline void
rb_set_black(struct rb_node *n)
{
	n->__rb_parent_color |= RB_BLACK;
}

static inline void
rb_set_red(struct rb_node *n)
{
	n->__rb_parent_color &= ~(uintptr_t)1;
}

static inline void
rb_link_node(struct rb_node *node, struct rb_node *parent,
	     struct rb_node **rb_link)
{
	node->__rb_parent_color = (uintptr_t)parent;	/* color = RB_RED (0) */
	node->rb_left = node->rb_right = NULL;
	*rb_link = node;
}

static inline void
__rb_rotate_left(struct rb_node *n, struct rb_root *root)
{
	struct rb_node *r = n->rb_right;
	struct rb_node *p = rb_parent(n);

	n->rb_right = r->rb_left;
	if (r->rb_left)
		rb_set_parent(r->rb_left, n);
	r->rb_left = n;
	rb_set_parent(r, p);
	if (p) {
		if (n == p->rb_left)
			p->rb_left = r;
		else
			p->rb_right = r;
	} else {
		root->rb_node = r;
	}
	rb_set_parent(n, r);
}

static inline void
__rb_rotate_right(struct rb_node *n, struct rb_root *root)
{
	struct rb_node *l = n->rb_left;
	struct rb_node *p = rb_parent(n);

	n->rb_left = l->rb_right;
	if (l->rb_right)
		rb_set_parent(l->rb_right, n);
	l->rb_right = n;
	rb_set_parent(l, p);
	if (p) {
		if (n == p->rb_right)
			p->rb_right = l;
		else
			p->rb_left = l;
	} else {
		root->rb_node = l;
	}
	rb_set_parent(n, l);
}

static inline void
rb_insert_color(struct rb_node *n, struct rb_root *root)
{
	struct rb_node *parent, *gparent, *uncle;

	while ((parent = rb_parent(n)) && rb_is_red(parent)) {
		gparent = rb_parent(parent);
		if (parent == gparent->rb_left) {
			uncle = gparent->rb_right;
			if (uncle && rb_is_red(uncle)) {
				rb_set_black(uncle);
				rb_set_black(parent);
				rb_set_red(gparent);
				n = gparent;
				continue;
			}
			if (n == parent->rb_right) {
				__rb_rotate_left(parent, root);
				uncle = parent;	/* swap n/parent */
				parent = n;
				n = uncle;
			}
			rb_set_black(parent);
			rb_set_red(gparent);
			__rb_rotate_right(gparent, root);
		} else {
			uncle = gparent->rb_left;
			if (uncle && rb_is_red(uncle)) {
				rb_set_black(uncle);
				rb_set_black(parent);
				rb_set_red(gparent);
				n = gparent;
				continue;
			}
			if (n == parent->rb_left) {
				__rb_rotate_right(parent, root);
				uncle = parent;
				parent = n;
				n = uncle;
			}
			rb_set_black(parent);
			rb_set_red(gparent);
			__rb_rotate_left(gparent, root);
		}
	}
	rb_set_black(root->rb_node);
}

static inline void
__rb_erase_color(struct rb_node *n, struct rb_node *parent,
		 struct rb_root *root)
{
	struct rb_node *sibling;

	while ((n == NULL || rb_is_black(n)) && n != root->rb_node) {
		if (parent->rb_left == n) {
			sibling = parent->rb_right;
			if (sibling && rb_is_red(sibling)) {
				rb_set_black(sibling);
				rb_set_red(parent);
				__rb_rotate_left(parent, root);
				sibling = parent->rb_right;
			}
			if ((sibling->rb_left == NULL ||
			     rb_is_black(sibling->rb_left)) &&
			    (sibling->rb_right == NULL ||
			     rb_is_black(sibling->rb_right))) {
				rb_set_red(sibling);
				n = parent;
				parent = rb_parent(n);
			} else {
				if (sibling->rb_right == NULL ||
				    rb_is_black(sibling->rb_right)) {
					rb_set_black(sibling->rb_left);
					rb_set_red(sibling);
					__rb_rotate_right(sibling, root);
					sibling = parent->rb_right;
				}
				rb_set_parent_color(sibling, rb_parent(sibling),
						    rb_color(parent));
				rb_set_black(parent);
				if (sibling->rb_right)
					rb_set_black(sibling->rb_right);
				__rb_rotate_left(parent, root);
				n = root->rb_node;
				break;
			}
		} else {
			sibling = parent->rb_left;
			if (sibling && rb_is_red(sibling)) {
				rb_set_black(sibling);
				rb_set_red(parent);
				__rb_rotate_right(parent, root);
				sibling = parent->rb_left;
			}
			if ((sibling->rb_left == NULL ||
			     rb_is_black(sibling->rb_left)) &&
			    (sibling->rb_right == NULL ||
			     rb_is_black(sibling->rb_right))) {
				rb_set_red(sibling);
				n = parent;
				parent = rb_parent(n);
			} else {
				if (sibling->rb_left == NULL ||
				    rb_is_black(sibling->rb_left)) {
					rb_set_black(sibling->rb_right);
					rb_set_red(sibling);
					__rb_rotate_left(sibling, root);
					sibling = parent->rb_left;
				}
				rb_set_parent_color(sibling, rb_parent(sibling),
						    rb_color(parent));
				rb_set_black(parent);
				if (sibling->rb_left)
					rb_set_black(sibling->rb_left);
				__rb_rotate_right(parent, root);
				n = root->rb_node;
				break;
			}
		}
	}
	if (n)
		rb_set_black(n);
}

static inline void
rb_erase(struct rb_node *n, struct rb_root *root)
{
	struct rb_node *child, *parent;
	int color;

	if (n->rb_left == NULL) {
		child = n->rb_right;
	} else if (n->rb_right == NULL) {
		child = n->rb_left;
	} else {
		struct rb_node *old = n, *left;

		n = n->rb_right;
		while ((left = n->rb_left) != NULL)
			n = left;

		if (rb_parent(old)) {
			if (rb_parent(old)->rb_left == old)
				rb_parent(old)->rb_left = n;
			else
				rb_parent(old)->rb_right = n;
		} else {
			root->rb_node = n;
		}

		child = n->rb_right;
		parent = rb_parent(n);
		color = rb_color(n);

		if (parent == old) {
			parent = n;
		} else {
			if (child)
				rb_set_parent(child, parent);
			parent->rb_left = child;

			n->rb_right = old->rb_right;
			rb_set_parent(old->rb_right, n);
		}

		n->__rb_parent_color = old->__rb_parent_color;
		n->rb_left = old->rb_left;
		rb_set_parent(old->rb_left, n);

		if (color == RB_BLACK)
			__rb_erase_color(child, parent, root);
		return;
	}

	parent = rb_parent(n);
	color = rb_color(n);

	if (child)
		rb_set_parent(child, parent);
	if (parent) {
		if (parent->rb_left == n)
			parent->rb_left = child;
		else
			parent->rb_right = child;
	} else {
		root->rb_node = child;
	}

	if (color == RB_BLACK)
		__rb_erase_color(child, parent, root);
}

static inline struct rb_node *
rb_first(const struct rb_root *root)
{
	struct rb_node *n = root->rb_node;

	if (n == NULL)
		return NULL;
	while (n->rb_left)
		n = n->rb_left;
	return n;
}

static inline struct rb_node *
rb_last(const struct rb_root *root)
{
	struct rb_node *n = root->rb_node;

	if (n == NULL)
		return NULL;
	while (n->rb_right)
		n = n->rb_right;
	return n;
}

static inline struct rb_node *
rb_next(const struct rb_node *n)
{
	struct rb_node *parent;

	if (RB_EMPTY_NODE((struct rb_node *)n))
		return NULL;

	if (n->rb_right) {
		n = n->rb_right;
		while (n->rb_left)
			n = n->rb_left;
		return (struct rb_node *)n;
	}

	while ((parent = rb_parent((struct rb_node *)n)) && n == parent->rb_right)
		n = parent;
	return parent;
}

static inline struct rb_node *
rb_prev(const struct rb_node *n)
{
	struct rb_node *parent;

	if (RB_EMPTY_NODE((struct rb_node *)n))
		return NULL;

	if (n->rb_left) {
		n = n->rb_left;
		while (n->rb_right)
			n = n->rb_right;
		return (struct rb_node *)n;
	}

	while ((parent = rb_parent((struct rb_node *)n)) && n == parent->rb_left)
		n = parent;
	return parent;
}

#endif /* _WIN32 */

#endif /* _HAMMER2_RBTREE_WINDOWS_H_ */
