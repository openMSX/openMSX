#ifndef SMALL_BUFFER_HH
#define SMALL_BUFFER_HH

#include "ranges.hh"
#include "stl.hh"

#include <algorithm>
#include <array>
#include <cassert>
#include <iterator>
#include <memory>
#include <span>
#include <type_traits>

// A run-time-sized buffer of (possibly) uninitialized data.
// Sizes upto 'BUF_SIZE' are stored internally in this object. Larger sizes use
// heap-allocation. In other words: this implements the small-buffer-optimization.

// Typically objects of this type are used as local variables. Not as member
// variables of a larger class.

// If you're sure the run-time will _always_ be <= BUF_SIZE, then it may be
// better to use 'inplace_uninitialized_buffer'.

template<typename T, size_t BUF_SIZE>
class small_buffer
{
	static_assert(std::is_trivially_constructible_v<T>);

public:
	explicit small_buffer(uninitialized_tag, size_t size)
	{
		if (size <= BUF_SIZE) {
			sp = std::span<T>(inplaceBuf.data(), size);
		} else {
			extBuf = std::make_unique_for_overwrite<T[]>(size);
			sp = std::span<T>(extBuf.get(), size);
		}
	}

	explicit small_buffer(size_t size, const T& t)
		: small_buffer(uninitialized_tag{}, size)
	{
		ranges::fill(sp, t);
	}

	template<typename Range>
	explicit small_buffer(const Range& range)
		: small_buffer(uninitialized_tag{}, std::distance(range.begin(), range.end()))
	{
		std::copy(range.begin(), range.end(), begin());
	}

	[[nodiscard]] operator std::span<T>() noexcept { return sp; }

	[[nodiscard]] auto size()  const noexcept { return sp.size(); }
	[[nodiscard]] auto empty() const noexcept { return sp.empty(); }
	[[nodiscard]] auto data()  noexcept { return sp.data(); }
	[[nodiscard]] auto begin() noexcept { return sp.begin(); }
	[[nodiscard]] auto end()   noexcept { return sp.end(); }

	[[nodiscard]] T& operator[](size_t index) {
		assert(index < size());
		return sp[index];
	}
	[[nodiscard]] T& front() {
		assert(!empty());
		return sp.front();
	}
	[[nodiscard]] T& back() {
		assert(!empty());
		return sp.back();
	}

private:
	std::span<T> sp; // span representing the active buffer
	std::array<T, BUF_SIZE> inplaceBuf;
	std::unique_ptr<T[]> extBuf;
};

#endif
