#ifndef RDESTL_TERNARY_SEARCH_TREE_H
#define RDESTL_TERNARY_SEARCH_TREE_H

#include "rdestl/allocator.h"
#include "rdestl/pair.h"
#include "rdestl/vector.h"

namespace rde
{

template<typename T, class TAllocator = rde::allocator>
class ternary_search_tree
{
public:
	typedef int				size_type;
	typedef T				value_type;
	typedef rde::pair<const char*, T>	pair_type;
	typedef TAllocator		allocator_type;

	struct node
	{
		node():	low_child(0), high_child(0), eq_child(0) {}
		explicit node(const T& value)
		:	split_char('\0'), low_child(0), high_child(0), eq_child(0), value(value) {}

		char	split_char;
		node*	low_child;
		node*	high_child;
		node*	eq_child;
		T		value;
	};
	typedef node*	iterator;
	static const size_type		kNodeSize = sizeof(node);
	static const int			kMaxPoolElements = 1000;

	explicit ternary_search_tree(const allocator_type& allocator = allocator_type())
	:	m_allocator(allocator),
		m_count(0),
		m_root(0),
		m_numNodes(0),
		m_nodePools(allocator),
		m_numFreePoolElements(-1)
	{
		/**/
	}
	~ternary_search_tree()
	{
	}

	// Only for compatibility reasons.
	iterator begin()	{ return m_root; }
	iterator end()		{ return 0; }

	void insert(const pair_type& v)
	{
		insert_p(v.first, v.second);
	}
	void insert_p(const char* key, const T& value)
	{
		node** pp = &m_root;
		node* iter;
		while ((iter = *pp) != 0)
		{
			if (*key == iter->split_char)
			{
				if (*key == '\0')	// already exists
					return;
				key++;
				pp = &(iter->eq_child);
			}
			else if (*key < iter->split_char)
				pp = &(iter->low_child);
			else
				pp = &(iter->high_child);
		}
		while (true)
		{
			if (*key == '\0')
			{
				node *n(construct_node(value));
				++m_count;
				*pp = n;
				return;
			}
			*pp = construct_node();
			node* n = *pp;
			n->split_char = *key++;
			pp = &(n->eq_child);
		}
	}

	iterator find(const char* key)
	{
		node* iter(m_root);
		while (iter != 0)
		{
			if (*key < iter->split_char)
				iter = iter->low_child;
			else if (*key == iter->split_char)
			{
				if (*key++ == '\0')
					return iter;
				iter = iter->eq_child;
			}
			else
				iter = iter->high_child;
		}
		return 0;
	}

	node* construct_node()
	{
		void* mem = alloc_node_mem();
		return new (mem) node();
	}
	node* construct_node(const value_type& v)
	{
		void* mem = alloc_node_mem();
		return new (mem) node(v);
	}
	void destruct_node(node* n)
	{
		(void)n;
		n->~node();
	}
	size_t used_memory() const	{ return m_numNodes * kNodeSize; }

	bool empty() const		{ return m_count == 0; }
	size_type size() const	{ return m_count; }

private:
	node* alloc_node_mem()
	{
		if (m_numFreePoolElements < 0)
			alloc_new_pool(kMaxPoolElements);
		++m_numNodes;
		return m_currentPool + m_numFreePoolElements--;
	}
	void alloc_new_pool(int pool_elements)
	{
		node* newPool = 
			(node*)m_allocator.allocate(pool_elements * sizeof(node));
		m_nodePools.push_back(newPool);
		m_currentPool = newPool;
		m_numFreePoolElements = kMaxPoolElements - 1;
	}

	allocator_type		m_allocator;
	size_type			m_count;
	node*				m_root;
	size_type			m_numNodes;
	rde::vector<node*>	m_nodePools;
	node*				m_currentPool;
	int					m_numFreePoolElements;
};

}

#endif
