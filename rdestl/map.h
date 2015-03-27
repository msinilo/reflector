#ifndef RDESTL_MAP_H
#define RDESTL_MAP_H

#include "rdestl/rb_tree.h"

namespace rde
{
template<typename Tk, typename Tv, 
	class TAllocator = rde::allocator>
class map
{
	template<typename TNodePtr, typename TPtr, typename TRef>
	class node_iterator
	{
	public:
		typedef forward_iterator_tag	iterator_category;

		explicit node_iterator(TNodePtr node, map* map_)
		:	m_node(node),
			m_map(map_)
		{/**/}
		template<typename UNodePtr, typename UPtr, typename URef>
		node_iterator(const node_iterator<UNodePtr, UPtr, URef>& rhs)
		:	m_node(rhs.node()),
			m_map(rhs.get_map())
		{
			/**/
		}

		TRef operator*() const
		{
			RDE_ASSERT(m_node != 0);
			return m_node->key;
		}
		TPtr operator->() const
		{
			return &m_node->key;
		}
		TNodePtr node() const
		{
			return m_node;
		}

		node_iterator& operator++() 
		{
			RDE_ASSERT(m_node != 0);
			TNodePtr next = m_node->next;
			if (next == 0)
				next = find_next_node(m_node);
			m_node = next;
			return *this;
		} 
		node_iterator operator++(int)
		{
			node_iterator copy(*this);
			++(*this);
			return copy;
		}

		bool operator==(const node_iterator& rhs) const
		{
			return rhs.m_node == m_node && m_map == rhs.m_map;
		}
		bool operator!=(const node_iterator& rhs) const
		{
			return !(rhs == *this);
		}

		map* get_map() const { return m_map; }
	private:
		TNodePtr find_next_node(TNodePtr node) const
		{
			return 0;
		}

		TNodePtr	m_node;
		map*		m_map;
	};

	template<typename Tk, typename Tv>
	struct map_pair : public rde::pair<Tk, Tv>
	{
		map_pair() {}
		map_pair(const Tk& k, const Tv& v): pair(k, v) {}
		bool operator<(const map_pair& rhs) const
		{
			return first < rhs.first;
		}
	};
public:
	typedef Tk															key_type;
	typedef map_pair<Tk, Tv>											value_type;
	typedef rb_tree<value_type>											tree_type;
	typedef node_iterator<typename tree_type::node*, value_type*, value_type&>	iterator;
	typedef node_iterator<typename tree_type::node*, const value_type*, const value_type&>	const_iterator;
	typedef TAllocator													allocator_type;

	explicit map(const allocator_type& allocator = allocator_type())
	:	m_tree(allocator) {}

	iterator end()
	{
		return iterator(0, this);
	}

	void insert(const value_type& v)
	{
		m_tree.insert(v);
	}
	iterator find(const key_type& key)
	{
		return iterator(m_tree.find_node(value_type(key, Tv())), this);
	}

private:
	rb_tree<value_type>	m_tree;
};

}

#endif // RDESTL_MAP_H

