#include "sortedlist.hh"

template <class T, class Compare>
bool SortedList<T, Compare>::empty () const {
	return list<T>::empty();
};

template <class T, class Compare>
const T &SortedList<T, Compare>::getFirst () const {
	return front();
};

template <class T, class Compare>
void SortedList<T, Compare>::removeFirst () {
	return pop_front();
};

template <class T, class Compare>
void SortedList<T, Compare>::insert (const T element) {
	Compare comp;
	list<T>::iterator i = begin();
	while (true) {
		if (i == end()) {
			push_back (element);
			return;
		}
		if (! comp (*i, element)) {
			list<T>::insert (i, element);
			return;
		}
		i++;
	}
};


#if 0
class C {
	public:
	int i;
	C (int a) : i(a) {}
	bool operator< (const C &c) const { return i < c.i; }
};
			     
void main(int argc, char** argv)
{
	SortedList<C> sl1;
	SortedList<int> sl2;

	sl1.insert(C(5));	sl2.insert(5);
	sl1.insert(C(7));	sl2.insert(7);
	sl1.insert(C(6));	sl2.insert(6);
	sl1.insert(C(4));	sl2.insert(4);
	
	while (! sl1.empty()) {
		cout << sl1.getFirst().i << " ";
		sl1.removeFirst();
	} cout << endl;
	while (! sl2.empty()) {
		cout << sl2.getFirst() << " ";
		sl2.removeFirst();
	} cout << endl;
}
#endif
	
