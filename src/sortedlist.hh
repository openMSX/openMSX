
#ifndef __SORTED_LIST__
#define __SORTED_LIST__

#include <stl.h>

template <class T, class Compare = less<T> >
class SortedList : protected list<T>
{
	public:
	bool empty () const;
	const T &getFirst () const;
	void removeFirst ();
	void insert (const T element);
};

#endif //__SORTED_LIST__
