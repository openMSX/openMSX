#include <stl.h>

template <class T, class Compare = less<T> >
class SortedList : protected list<T>
{
	public:
	bool empty () const;
	T getFirst () const;
	void removeFirst ();
	void insert (const T element);
};
	
