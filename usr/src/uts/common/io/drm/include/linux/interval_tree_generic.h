/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * illumos: interval_tree_generic.h stub.
 *
 * Provides INTERVAL_TREE_DEFINE, which the Linux kernel implements via
 * augmented red-black trees (lib/interval_tree_generic.h).  On illumos we
 * use the BSD rb-tree from rbtree.h (sys/tree.h underneath) with a simple
 * sorted-by-start-address BST.  iter_first / iter_next are O(n) linear
 * scans; insert / remove are O(log n).  This is correct for Phase 1.
 */
#ifndef _LINUX_INTERVAL_TREE_GENERIC_H
#define _LINUX_INTERVAL_TREE_GENERIC_H

#include <linux/rbtree.h>
#include <linux/container_of.h>

/*
 * INTERVAL_TREE_DEFINE(ITSTRUCT, ITRB, ITTYPE, ITSUBTREE,
 *                      ITSTART, ITLAST, ITSTATIC, ITPREFIX)
 *
 * Generates four functions:
 *   ITPREFIX_insert(ITSTRUCT *node, struct rb_root_cached *root)
 *   ITPREFIX_remove(ITSTRUCT *node, struct rb_root_cached *root)
 *   ITSTRUCT *ITPREFIX_iter_first(struct rb_root_cached *root,
 *                                 ITTYPE start, ITTYPE last)
 *   ITSTRUCT *ITPREFIX_iter_next(ITSTRUCT *node,
 *                                ITTYPE start, ITTYPE last)
 */
#define INTERVAL_TREE_DEFINE(ITSTRUCT, ITRB, ITTYPE, ITSUBTREE,	\
			     ITSTART, ITLAST, ITSTATIC, ITPREFIX)	\
									\
static bool ITPREFIX##_it_less(struct rb_node *_a,			\
			       const struct rb_node *_b)		\
{									\
	ITSTRUCT *ia = container_of(_a, ITSTRUCT, ITRB);		\
	const ITSTRUCT *ib = container_of(_b, const ITSTRUCT, ITRB);	\
	return ITSTART(ia) < ITSTART(ib);				\
}									\
									\
ITSTATIC void								\
ITPREFIX##_insert(ITSTRUCT *node, struct rb_root_cached *root)		\
{									\
	node->ITSUBTREE = ITLAST(node);					\
	rb_add_cached(&node->ITRB, root, ITPREFIX##_it_less);		\
}									\
									\
ITSTATIC void								\
ITPREFIX##_remove(ITSTRUCT *node, struct rb_root_cached *root)		\
{									\
	rb_erase_cached(&node->ITRB, root);				\
	RB_CLEAR_NODE(&node->ITRB);					\
}									\
									\
ITSTATIC ITSTRUCT *							\
ITPREFIX##_iter_first(struct rb_root_cached *root,			\
		      ITTYPE start, ITTYPE last)			\
{									\
	struct rb_node *_n;						\
	for (_n = rb_first_cached(root); _n != NULL;			\
	     _n = rb_next(_n)) {					\
		ITSTRUCT *_node = container_of(_n, ITSTRUCT, ITRB);	\
		if (ITSTART(_node) > last)				\
			break;						\
		if (ITLAST(_node) >= start)				\
			return _node;					\
	}								\
	return NULL;							\
}									\
									\
ITSTATIC ITSTRUCT *							\
ITPREFIX##_iter_next(ITSTRUCT *node, ITTYPE start, ITTYPE last)		\
{									\
	struct rb_node *_n = rb_next(&node->ITRB);			\
	for (; _n != NULL; _n = rb_next(_n)) {				\
		ITSTRUCT *_next = container_of(_n, ITSTRUCT, ITRB);	\
		if (ITSTART(_next) > last)				\
			break;						\
		if (ITLAST(_next) >= start)				\
			return _next;					\
	}								\
	return NULL;							\
}

#endif /* _LINUX_INTERVAL_TREE_GENERIC_H */
