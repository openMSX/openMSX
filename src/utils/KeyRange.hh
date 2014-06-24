#ifndef KEYRANGE_HH
#define KEYRANGE_HH

#include "StringMap.hh"
#include <iterator>
#include <tuple>

namespace detail {

template<typename MAP, size_t N> class KeyIterator
{
	typedef typename MAP::const_iterator                        map_iter;
	typedef typename std::iterator_traits<map_iter>::value_type pair_type;
public:

	typedef const typename std::tuple_element<N, pair_type>::type     value_type;
	typedef value_type*                                               pointer;
	typedef value_type&                                               reference;
	typedef typename std::iterator_traits<map_iter>::difference_type  difference_type;
	typedef std::forward_iterator_tag                                 iterator_category;

	KeyIterator(map_iter it_) : it(it_) {}
	reference operator*() const { return std::get<N>(*it); }
	KeyIterator& operator++() { ++it; return *this; }
	bool operator==(KeyIterator& other) const { return it == other.it; }
	bool operator!=(KeyIterator& other) const { return it != other.it; }
private:
	map_iter it;
};

template<typename T, size_t N> struct StringMapGetter;
template<typename T> struct StringMapGetter<T, 0> {
	typedef string_ref  value_type;
	typedef string_ref* pointer;
	typedef string_ref  reference;
	reference operator()(const StringMapEntry<T>& entry) { return entry.first(); }
};
template<typename T> struct StringMapGetter<T, 1> {
	typedef const T  value_type;
	typedef const T* pointer;
	typedef const T& reference;
	reference operator()(const StringMapEntry<T>& entry) { return entry.second; }
};

template<typename T, size_t N> class KeyIterator2
{
	static_assert(N==0 || N==1, "StringMap only has key and value");
	typedef StringMapGetter<T, N> Getter;
public:
	typedef typename Getter::value_type value_type;
	typedef typename Getter::pointer    pointer;
	typedef typename Getter::reference  reference;
	typedef int                         difference_type;
	typedef std::forward_iterator_tag   iterator_category;

	KeyIterator2(typename StringMap<T>::const_iterator it_) : it(it_) {}
	reference operator*() const { Getter getter; return getter(*it); }
	KeyIterator2& operator++() { ++it; return *this; }
	bool operator==(KeyIterator2& other) const { return it == other.it; }
	bool operator!=(KeyIterator2& other) const { return it != other.it; }
private:
	typename StringMap<T>::const_iterator it;
};

template<typename MAP, size_t N> class KeyRange
{
public:
	KeyRange(const MAP& map_)
		: map(map_) {}

	KeyIterator<MAP, N> begin() const { return map.begin(); }
	KeyIterator<MAP, N> end()   const { return map.end();   }
private:
	const MAP& map;
};

template<typename T, size_t N> class KeyRange<StringMap<T>, N>
{
public:
	KeyRange(const StringMap<T>& map_)
		: map(map_) {}

	KeyIterator2<T, N> begin() const { return map.begin(); }
	KeyIterator2<T, N> end()   const { return map.end();   }
private:
	const StringMap<T>& map;
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
