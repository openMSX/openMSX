#ifndef CIRCULARBUFFER_HH
#define CIRCULARBUFFER_HH

#include <cassert>

namespace openmsx {

template<class T, size_t MAXSIZE>
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
		auto tmp = first;
		first = next(first);
		return buffer[tmp];
	}
	T& removeBack() {
		assert(!isEmpty());
		last = prev(last);
		return buffer[last];
	}
	T& operator[](size_t pos) {
		assert(pos < MAXSIZE);
		auto tmp = first + pos;
		if (tmp > MAXSIZE) {
			tmp -= (MAXSIZE + 1);
		}
		return buffer[tmp];
	}
	const T& operator[](size_t pos) const {
		return const_cast<CircularBuffer&>(*this)[pos];
	}
	bool isEmpty() const {
		return (first == last);
	}
	bool isFull() const {
		return (first == next(last));
	}
	size_t size() const {
		if (first > last) {
			return MAXSIZE + 1 - first + last;
		} else {
			return last - first;
		}
	}

private:
	inline size_t next(size_t a) const {
		return (a != MAXSIZE) ? a + 1 : 0;
	}
	inline size_t prev(size_t a) const {
		return (a != 0) ? a - 1 : MAXSIZE;
	}

	size_t first, last;
	// one extra to be able to distinguish full and empty
	T buffer[MAXSIZE + 1];
};

} // namespace openmsx

#endif
