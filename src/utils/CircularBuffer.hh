#ifndef CIRCULARBUFFER_HH
#define CIRCULARBUFFER_HH

#include <cassert>

namespace openmsx {

template<class T, size_t MAXSIZE>
class CircularBuffer
{
public:
	CircularBuffer() = default;

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
	[[nodiscard]] T& operator[](size_t pos) {
		assert(pos < MAXSIZE);
		auto tmp = first + pos;
		if (tmp > MAXSIZE) {
			tmp -= (MAXSIZE + 1);
		}
		return buffer[tmp];
	}
	[[nodiscard]] const T& operator[](size_t pos) const {
		return const_cast<CircularBuffer&>(*this)[pos];
	}
	[[nodiscard]] bool isEmpty() const {
		return (first == last);
	}
	[[nodiscard]] bool isFull() const {
		return (first == next(last));
	}
	[[nodiscard]] size_t size() const {
		if (first > last) {
			return MAXSIZE + 1 - first + last;
		} else {
			return last - first;
		}
	}

private:
	[[nodiscard]] inline size_t next(size_t a) const {
		return (a != MAXSIZE) ? a + 1 : 0;
	}
	[[nodiscard]] inline size_t prev(size_t a) const {
		return (a != 0) ? a - 1 : MAXSIZE;
	}

	size_t first = 0;
	size_t last = 0;
	// one extra to be able to distinguish full and empty
	T buffer[MAXSIZE + 1];
};

} // namespace openmsx

#endif
