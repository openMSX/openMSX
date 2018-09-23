#ifndef KEYRANGE_HH
#define KEYRANGE_HH

#include <iterator>
#include <tuple>

namespace detail {

template<typename MAP, size_t N> class KeyIterator
{
	using map_iter  = typename MAP::const_iterator;
	using pair_type = typename std::iterator_traits<map_iter>::value_type;
public:

	using value_type        = const std::tuple_element_t<N, pair_type>;
	using pointer           = value_type*;
	using reference         = value_type&;
	using difference_type   = typename std::iterator_traits<map_iter>::difference_type;
	using iterator_category = std::forward_iterator_tag;

	/*implicit*/ KeyIterator(map_iter it_) : it(it_) {}
	reference operator*() const { return std::get<N>(*it); }
	KeyIterator& operator++() { ++it; return *this; }
	bool operator==(const KeyIterator& other) const { return it == other.it; }
	bool operator!=(const KeyIterator& other) const { return it != other.it; }
private:
	map_iter it;
};

template<typename MAP, size_t N> class KeyRange
{
public:
	explicit KeyRange(const MAP& map_)
		: map(map_) {}

	KeyIterator<MAP, N> begin() const { return map.begin(); }
	KeyIterator<MAP, N> end()   const { return map.end();   }
private:
	const MAP& map;
};

} // namespace detail


// Input: a collection that contains key-value pairs or tuples.
// Output: a range that represents (only) the keys, the values or the N-th
//         elements of the items in the input.

template<typename MAP> detail::KeyRange<MAP, 0> keys(const MAP& map)
{
	return detail::KeyRange<MAP, 0>(map);
}

template<typename MAP> detail::KeyRange<MAP, 1> values(const MAP& map)
{
	return detail::KeyRange<MAP, 1>(map);
}

template<size_t N, typename MAP> detail::KeyRange<MAP, N> elements(const MAP& map)
{
	return detail::KeyRange<MAP, N>(map);
}

#endif
