/*
 * Copyright (c) 2026 ykat UG (haftungsbeschraenkt) - All Rights Reserved
 *
 * Permission for non-commercial use is hereby granted,
 * free of charge, without warranty of any kind.
 */

#include <linux/errno.h>
#include <linux/export.h>
#include <linux/init.h>
#include <linux/jiffies.h>
#include <linux/list.h>
#include <linux/module.h>
#include <linux/mutex.h>
#include <linux/workqueue.h>
/*
#include "htable.h"

HLIST_HEAD(__htable_index);
static DEFINE_MUTEX(htable_index_lock);
static struct delayed_work htable_cleanup_work;
static bool htable_stopping;
EXPORT_SYMBOL_GPL(__htable_index);

void htable_index_add(struct htable *table)
{
	mutex_lock(&htable_index_lock);
	if (!table->registered) {
		WRITE_ONCE(table->registered, true);
		hlist_add_head(&table->index_node, &__htable_index);
	}
	mutex_unlock(&htable_index_lock);
}
EXPORT_SYMBOL_GPL(htable_index_add);

void htable_index_del(struct htable *table)
{
	mutex_lock(&htable_index_lock);
	if (table->registered) {
		WRITE_ONCE(table->registered, false);
		hlist_del_init(&table->index_node);
	}
	mutex_unlock(&htable_index_lock);
}
EXPORT_SYMBOL_GPL(htable_index_del);

void htable_index_each(void (*callback)(struct htable *, void *), void *arg)
{
	struct htable *table;

	if (!callback)
		return;

	mutex_lock(&htable_index_lock);
	hlist_for_each_entry(table, &__htable_index, index_node)
		callback(table, arg);
	mutex_unlock(&htable_index_lock);
}
EXPORT_SYMBOL_GPL(htable_index_each);

static void htable_cleanup_worker(struct work_struct *work)
{
	struct htable *table;

	(void)work;
	mutex_lock(&htable_index_lock);
	hlist_for_each_entry(table, &__htable_index, index_node) {
		int ret = htable_resize(table);

		if (ret < 0)
			pr_warn_ratelimited("htable: periodic resize failed: %d\n",
					    ret);
	}
	mutex_unlock(&htable_index_lock);

	if (!READ_ONCE(htable_stopping))
		schedule_delayed_work(&htable_cleanup_work,
				      msecs_to_jiffies(
					      HTABLE_CLEANUP_INTERVAL_SECONDS *
					      1000));
}
*/
static int __init htable_module_init(void)
{
/*
	INIT_DELAYED_WORK(&htable_cleanup_work, htable_cleanup_worker);
	WRITE_ONCE(htable_stopping, false);
	schedule_delayed_work(&htable_cleanup_work,
			      msecs_to_jiffies(
				      HTABLE_CLEANUP_INTERVAL_SECONDS * 1000));
	return 0;
	*/
}

static void __exit htable_module_exit(void)
{
	/*
	WRITE_ONCE(htable_stopping, true);
	cancel_delayed_work_sync(&htable_cleanup_work);
	*/
}

module_init(htable_module_init);
module_exit(htable_module_exit);

MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("Caller-synchronized htable support");
