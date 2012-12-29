#ifndef KEYRANGE_HH
#define KEYRANGE_HH

#include "StringMap.hh"

template<typename MAP> class KeyIterator
{
public:
	KeyIterator(typename MAP::const_iterator it_)
		: it(it_) {}
	const typename MAP::key_type& operator*() const { return it->first; }
	KeyIterator& operator++() { ++it; return *this; }
	bool operator==(KeyIterator& other) const { return it == other.it; }
	bool operator!=(KeyIterator& other) const { return it != other.it; }
private:
	typename MAP::const_iterator it;
};

template<typename T> class KeyIterator2
{
public:
	KeyIterator2(typename StringMap<T>::const_iterator it_)
		: it(it_) {}
	string_ref operator*() const { return it->first(); }
	KeyIterator2& operator++() { ++it; return *this; }
	bool operator==(KeyIterator2& other) const { return it == other.it; }
	bool operator!=(KeyIterator2& other) const { return it != other.it; }
private:
	typename StringMap<T>::const_iterator it;
};

template<typename MAP> class KeyRange
{
public:
	KeyRange(const MAP& map_)
		: map(map_) {}

	KeyIterator<MAP> begin() const { return map.begin(); }
	KeyIterator<MAP> end()   const { return map.end();   }
private:
	const MAP& map;
};

template<typename T> class KeyRange<StringMap<T>>
{
public:
	KeyRange(const StringMap<T>& map_)
		: map(map_) {}

	KeyIterator2<T> begin() const { return map.begin(); }
	KeyIterator2<T> end()   const { return map.end();   }
private:
	const StringMap<T>& map;
};

template<typename MAP> KeyRange<MAP> keys(const MAP& map)
{
	return KeyRange<MAP>(map);
}

#endif
