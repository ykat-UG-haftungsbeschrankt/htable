#include <linux/build_bug.h>
#include <linux/list.h>
#include <linux/stddef.h>
#include <linux/container_of.h>
#include <linux/stringify.h>

#define HTABLE_RESIZE_THRESHOLD 1000

typedef u32 htable_hash_t;

enum htable_flags {
	HTABLE_FLAGS_INIT  = 0,
	HTABLE_FLAGS_IS_TABLE = 1 << 0,
	HTABLE_FLAGS_IS_ADDED_TO_INDEX = 1 << 1,
};

struct htable {
	struct htable_index      index;
	struct htable_statistics stats;
	struct htable_list       list;
	struct htable_table      table;
	enum   htable_flags      flags;
};

struct htable_node{
	union{
		struct hlist_list_node list;
		struct hlist_table_node table;
	}node;
};




static inline struct htable_node *htable_node_from_hlist_node(struct hlist_node *hnode)
{
	return (struct htable_node *)hnode;
}

static inline struct htable_table_node *htable_node_to_htable_table_node(struct htable_node *hnode)
{
	return (struct htable_table_node *)hnode;
}

static inline struct htable_list_node *htable_node_to_htable_list_node(struct htable_node *hnode)
{
	return (struct htable_list_node *)hnode;
}

static inline struct hlist_node *__htable_node_is_type_table(struct htable_node *hnode)
{
	return (struct hlist_node *)hnode;
}

BUILD_BUG_ON(sizeof(struct hlist_head) < sizeof(void*));
BUILD_BUG_ON(sizeof(struct hlist_node) != sizeof(struct htable_node));
BUILD_BUG_ON(offsetof(struct hlist_list_node, hash) != offsetof(struct hlist_table_node, hash));


bool __htable_is_type_list(struct htable *table){
	return !(table->flags & HTABLE_FLAGS_IS_TABLE);
}

bool __htable_is_type_table(struct htable *table){
	return table->flags & HTABLE_FLAGS_IS_TABLE;
}

void __htable_set_type_list(struct htable *table){
	table->flags &= ~HTABLE_FLAGS_IS_TABLE;
}

void __htable_set_type_table(struct htable *table){
	table->flags |= HTABLE_FLAGS_IS_TABLE;
}

void htable_init(struct *htable){}
void htable_deinit(struct *htable){}


//////////////////////////////////////////////////////////////////////////
// Htable index management functions
//////////////////////////////////////////////////////////////////////////

void htable_resize_list2table(struct htable *table){
	struct htable_node *node = NULL;
	if(__htable_is_type_list(table)){
		__htable_set_type_table(table);
		while((node = htable_list_remove_node(table))){
			if(!htable_table_dynamic_add_node(table,node)){
				htable_list_add_node(table,node);
				while((node = htable_table_dynamic_remove_node(table))){
					htable_list_add_node(table,node);
				}
				__htable_set_type_list(table);
				break;
			}
		}
	}	
}

void htable_resize_table2list(struct htable *table){
	struct htable_node *node = NULL;
	if(__htable_is_type_table(table)){
		__htable_set_type_list(table);
		while((node = htable_table_dynamic_remove_node(table))){
			htable_list_add_node(table,node);
		}
	}
}

void htable_resize(struct htable *table){
	htable_resize_table2list(table);
	if(htable_table_resize(table)){
		htable_resize_list2table(table);
	}
}

void htable_add_node(struct htable *table, struct htable_node *node, uintptr_t key){
	if(__htable_is_type_table(table)){
		node->key = key;
		if(!htable_table_dynamic_add_node(table,node)){
			htable_resize_table2list(table);
			htable_list_add_node(table,node);
		}
	}else{
		htable_list_add_node(table,node);
	}
}

void htable_remove_node(struct htable_node *node){
	if(htable_node_is_type_table(node)){
		struct htable *table = container_of(
			 htable_table_get_by_node(node)
			,struct htable
			,table
		);
		if(!htable_table_dynamic_add_node(table,node)){
			htable_resize_table2list(table);
			htable_list_add_node(table,node);
		}
	}else{
		struct htable *table = container_of(
			 htable_table_list_get_by_node(node)
			,struct htable
			,list
		);
		htable_list_add_node(&table->list,node);
	}
}




//////////////////////////////////////////////////////////////////////////
// Htable hashtable.h wrapper
//////////////////////////////////////////////////////////////////////////

#define HTABLE_INIT(name) {						\
	.index = HTABLE_INDEX_INIT,						\
	.stats = HTABLE_STATS_INIT(name),		               			\
	.list = HTABLE_LIST_INIT,							\
	.table = HTABLE_TABLE_INIT,						\
	.flags = HTABLE_FLAGS_INIT,						\
}

#define DEFINE_HASHTABLE(name, bits)						    \
	struct htable name = HTABLE_INIT(name)

#define DEFINE_READ_MOSTLY_HASHTABLE(name, bits)				\
	struct htable __read_mostly name = HTABLE_INIT(name)

#define DECLARE_HASHTABLE(name, bits)                         	\
	struct htable name

/**
 * hash_init - initialize a hash table
 * @hashtable: hashtable to be initialized
 *
 * Calculates the size of the hashtable from the given parameter, otherwise
 * same as hash_init_size.
 *
 * This has to be a macro since HASH_BITS() will not work on pointers since
 * it calculates the size during preprocessing.
 */
#define hash_init(hashtable) __hash_init(hashtable)
static inline void __hash_init(struct htable *htable)
{
	htable_init(htable);
}

#define hash_deinit(hashtable) __hash_deinit(hashtable)
static inline void __hash_deinit(struct htable *htable)
{
	htable_deinit(htable);
}

#define htable_hash(val, bits) hash_long(val, bits)

/**
 * hash_add - add an object to a hashtable
 * @hashtable: hashtable to add to
 * @node: the &struct hlist_node of the object to be added
 * @key: the key of the object to be added
 */
#define hash_add(hashtable, node, key)						\
	__hash_add(hashtable##_htable,node, key)
static inline void __hash_add(struct htable *htable, struct hlist_node *hnode, htable_hash_t key)
{
	struct htable_node *node = (struct htable_node *)hnode;
	htable_index_add(htable);
	htable_stats_on_add(&htable->stats);
	if(__htable_is_type_table(htable)){
		if(!htable_table_hash_add(htable,node,key)){
			htable_resize_table2list(htable);
			htable_list_hash_add(htable,node,key);
		}
	}else{
		htable_list_hash_add(htable,node,key);
	}
}



/**
 * hash_add_rcu - add an object to a rcu enabled hashtable
 * @hashtable: hashtable to add to
 * @node: the &struct hlist_node of the object to be added
 * @key: the key of the object to be added
 */
#define hash_add_rcu(hashtable, node, key)					\
	__hash_add_rcu(hashtable##_htable, node, key)
static inline void __hash_add_rcu(struct htable *htable, struct hlist_node *node, htable_hash_t key)
{
	htable_index_add(htable);
	hlist_add_head_rcu(node,&htable->table_static[htable_hash(key,htable->hash_bits)]);
}

/**
 * hash_hashed - check whether an object is in any hashtable
 * @node: the &struct hlist_node of the object to be checked
 */
static inline bool hash_hashed(struct hlist_node *hnode)
{
	struct htable_node *node = (struct htable_node *)hnode;
	return htable_node_is_hashed(node);
	//return !hlist_unhashed(&node->hnode);
}



/**
 * hash_empty - check whether a hashtable is empty
 * @hashtable: hashtable to check
 *
 * This has to be a macro since HASH_BITS() will not work on pointers since
 * it calculates the size during preprocessing.
 */
#define hash_empty(hashtable) __hash_empty(hashtable##_htable)
static inline bool __hash_empty(struct htable *htable)
{
	return htable->stats.entries == 0;
}

/**
 * hash_del - remove an object from a hashtable
 * @node: &struct hlist_node of the object to remove
 */
static inline void hash_del(struct hlist_node *hnode)
{
	struct htable_node *node = htable_node_from_hlist_node(hnode);
	struct htable *htable;

	if(__htable_node_is_type_table(node)){
		struct htable_table_node *node = htable_node_to_htable_table_node(hnode);
		htable = container_of(
			 htable_table_get_by_node(node)
			,struct htable
			,table
		);
		htable_table_hash_del(&htable->table,node);
	}else{
		struct htable_list_node *node = htable_node_to_htable_list_node(hnode);
		htable = container_of(
			 htable_list_get_by_node(node)
			,struct htable
			,list
		);
		htable_list_hash_del(&htable->list,node);
	}
	htable_stats_on_del(&htable->stats);
}

/**
 * hash_del_rcu - remove an object from a rcu enabled hashtable
 * @node: &struct hlist_node of the object to remove
 */
static inline void hash_del_rcu(struct hlist_node *node)
{
	struct htable *htable = container_of(node->pprev, struct htable, table_static);
	if(htable->table_static == NULL)
		return;
	hlist_del_init_rcu(node);
}
