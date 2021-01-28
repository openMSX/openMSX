#ifndef CIRCULARBUFFER_HH
#define CIRCULARBUFFER_HH

#include <cassert>
#include <cstddef>
#include <utility>

namespace openmsx {

template<typename T, size_t MAXSIZE>
class CircularBuffer
{
public:
	constexpr CircularBuffer() = default;

	constexpr void addFront(const T& element) {
		assert(!isFull());
		first = prev(first);
		buffer[first] = element;
	}
	constexpr void addFront(T&& element) {
		assert(!isFull());
		first = prev(first);
		buffer[first] = std::move(element);
	}
	constexpr void addBack(const T& element) {
		assert(!isFull());
		buffer[last] = element;
		last = next(last);
	}
	constexpr void addBack(T&& element) {
		assert(!isFull());
		buffer[last] = std::move(element);
		last = next(last);
	}
	constexpr T& removeFront() {
		assert(!isEmpty());
		auto tmp = first;
		first = next(first);
		return buffer[tmp];
	}
	constexpr T& removeBack() {
		assert(!isEmpty());
		last = prev(last);
		return buffer[last];
	}
	[[nodiscard]] constexpr T& operator[](size_t pos) {
		assert(pos < MAXSIZE);
		auto tmp = first + pos;
		if (tmp > MAXSIZE) {
			tmp -= (MAXSIZE + 1);
		}
		return buffer[tmp];
	}
	[[nodiscard]] constexpr const T& operator[](size_t pos) const {
		return const_cast<CircularBuffer&>(*this)[pos];
	}
	[[nodiscard]] constexpr bool isEmpty() const {
		return (first == last);
	}
	[[nodiscard]] constexpr bool isFull() const {
		return (first == next(last));
	}
	[[nodiscard]] constexpr size_t size() const {
		if (first > last) {
			return MAXSIZE + 1 - first + last;
		} else {
			return last - first;
		}
	}

private:
	[[nodiscard]] constexpr size_t next(size_t a) const {
		return (a != MAXSIZE) ? a + 1 : 0;
	}
	[[nodiscard]] constexpr size_t prev(size_t a) const {
		return (a != 0) ? a - 1 : MAXSIZE;
	}

	size_t first = 0;
	size_t last = 0;
	// one extra to be able to distinguish full and empty
	T buffer[MAXSIZE + 1];
};

} // namespace openmsx

#endif
