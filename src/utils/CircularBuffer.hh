#ifndef CIRCULARBUFFER_HH
#define CIRCULARBUFFER_HH

#include <array>
#include <cassert>
#include <cstddef>
#include <utility>

namespace openmsx {

template<typename T, size_t MAXSIZE>
class CircularBuffer
{
public:
	constexpr CircularBuffer() = default;

	constexpr void push_front(const T& element) {
		assert(!full());
		first = prev(first);
		buffer[first] = element;
	}
	constexpr void push_front(T&& element) {
		assert(!full());
		first = prev(first);
		buffer[first] = std::move(element);
	}
	constexpr void push_back(const T& element) {
		assert(!full());
		buffer[last] = element;
		last = next(last);
	}
	constexpr void push_back(T&& element) {
		assert(!full());
		buffer[last] = std::move(element);
		last = next(last);
	}
	constexpr T& pop_front() {
		assert(!empty());
		auto tmp = first;
		first = next(first);
		return buffer[tmp];
	}
	constexpr T& pop_back() {
		assert(!empty());
		last = prev(last);
		return buffer[last];
	}

	[[nodiscard]] constexpr T& operator[](size_t pos) {
		assert(pos < size());
		auto tmp = first + pos;
		if (tmp > MAXSIZE) {
			tmp -= (MAXSIZE + 1);
		}
		return buffer[tmp];
	}
	[[nodiscard]] constexpr const T& operator[](size_t pos) const {
		return const_cast<CircularBuffer&>(*this)[pos];
	}

	[[nodiscard]] constexpr T& front() {
		assert(!empty());
		return buffer[first];
	}
	[[nodiscard]] constexpr const T& front() const {
		return const_cast<CircularBuffer&>(*this)->front();
	}

	[[nodiscard]] constexpr T& back() {
		assert(!empty());
		return buffer[prev(last)];
	}
	[[nodiscard]] constexpr const T& back() const {
		return const_cast<CircularBuffer&>(*this)->back();
	}

	[[nodiscard]] constexpr bool empty() const {
		return (first == last);
	}
	[[nodiscard]] constexpr bool full() const {
		return (first == next(last));
	}
	[[nodiscard]] constexpr size_t size() const {
		if (first > last) {
			return MAXSIZE + 1 - first + last;
		} else {
			return last - first;
		}
	}

	void clear() {
		first = last = 0;
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
	std::array<T, MAXSIZE + 1> buffer;
};

} // namespace openmsx

#endif
