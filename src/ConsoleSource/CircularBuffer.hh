// $Id$

#ifndef __CIRCULARBUFFER_HH__
#define __CIRCULARBUFFER_HH__

#include <cassert>


template<class T, int maxSize>
class CircularBuffer {
	public:
		CircularBuffer() {
			first = last = 0;
		}
		void addFront(const T &element) {
			assert(!isFull());
			first = prev(first);
			buffer[first] = element;
		}
		void addBack (const T &element) {
			assert(!isFull());
			int tmp = last;
			last = next(last);
			buffer[tmp] = element;
		}
		T &removeFront() {
			assert(!isEmpty());
			int tmp = first;
			first = next(first);
			return buffer[tmp];
		}
		T &removeBack () {
			assert(!isEmpty());
			last = prev(last);
			return buffer[last];
		}
		T &operator[](int pos) {
			assert(pos < maxSize);
			int tmp = first+pos;
			if (tmp>maxSize) tmp -= (maxSize+1);
			return buffer[tmp];
		}
		bool isEmpty() const {
			return (first == last);
		}
		bool isFull() const {
			return (first == next(last));
		}
		int size() const {
			int tmp = last-first;
			if (tmp<0) tmp += (maxSize+1);
			return tmp;
		}

	private:
		inline int next(int a) const {
			return (a==maxSize) ? 0 : a+1;
		}
		inline int prev(int a) const {
			return (a==0) ? maxSize : a-1;
		}
		
		T buffer[maxSize+1];
		int first, last;
};

#endif
