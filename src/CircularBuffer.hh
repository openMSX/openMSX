// $Id$

#ifndef __CIRCULARBUFFER_HH__
#define __CIRCULARBUFFER_HH__

#include <cassert>

namespace openmsx {

template<class T, unsigned MAXSIZE>
class CircularBuffer
{
public:
	CircularBuffer()
		: first(0), last(0)
	{
	}
	void addFront(const T& element) {
		assert(!isFull());
		first = prev(first);
		buffer[first] = element;
	}
	void addBack(const T& element) {
		assert(!isFull());
		buffer[last] = element;
		last = next(last);
	}
	T& removeFront() {
		assert(!isEmpty());
		unsigned tmp = first;
		first = next(first);
		return buffer[tmp];
	}
	T& removeBack() {
		assert(!isEmpty());
		last = prev(last);
		return buffer[last];
	}
	T& operator[](unsigned pos) {
		assert(pos < MAXSIZE);
		unsigned tmp = first + pos;
		if (tmp > MAXSIZE) {
			tmp -= (MAXSIZE + 1);
		}
		return buffer[tmp];
	}
	const T& operator[](unsigned pos) const {
		return const_cast<CircularBuffer&>(*this)[pos];
	}
	bool isEmpty() const {
		return (first == last);
	}
	bool isFull() const {
		return (first == next(last));
	}
	unsigned size() const {
		if (first > last) {
			return MAXSIZE + 1 - first + last;
		} else {
			return last - first;
		}
	}

private:
	inline unsigned next(unsigned a) const {
		return (a != MAXSIZE) ? a + 1 : 0;
	}
	inline unsigned prev(unsigned a) const {
		return (a != 0) ? a - 1 : MAXSIZE;
	}
	
	unsigned first, last;
	// one extra to be able to distinguish full and empty
	T buffer[MAXSIZE + 1];
};

} // namespace openmsx

#endif
