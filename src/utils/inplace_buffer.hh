#ifndef INPLACE_BUFFER_HH
#define INPLACE_BUFFER_HH

#include "ranges.hh"
#include "stl.hh"

#include <algorithm>
#include <array>
#include <cassert>
#include <cstdint>
#include <iterator>
#include <limits>
#include <span>
#include <type_traits>

// A run-time-sized buffer of (possibly) uninitialized data with a known
// upper-bound for the size.

// Typically objects of this type are used as local variables. Not as member
// variables of a larger class.

// This class is similar to 'static_vector<T, N>' (new name will be
// 'inplace_vector'). Except that this one leaves the initial content
// uninitialized.

// If you're unsure about the upper-bound for the size, or if that upper-bound
// is only rarely needed, then consider using 'small_uninitialized_buffer'.

template<typename T, size_t BUF_SIZE>
class inplace_buffer
{
	static_assert(std::is_trivially_constructible_v<T>);

	using SizeType =
	        std::conditional_t<BUF_SIZE <= std::numeric_limits<uint8_t >::max(), uint8_t,
	        std::conditional_t<BUF_SIZE <= std::numeric_limits<uint16_t>::max(), uint16_t,
	        std::conditional_t<BUF_SIZE <= std::numeric_limits<uint32_t>::max(), uint32_t,
	                                                                             uint64_t>>>;

public:
	explicit inplace_buffer(uninitialized_tag, size_t size) : sz(SizeType(size))
	{
		assert(size <= BUF_SIZE);
	}

	explicit inplace_buffer(size_t size, const T& t)
		: inplace_buffer(uninitialized_tag{}, size)
	{
		ranges::fill(std::span{buffer.data(), sz}, t);
	}

	template<typename Range>
	explicit inplace_buffer(const Range& range)
		: inplace_buffer(uninitialized_tag{}, std::distance(std::begin(range), std::end(range)))
	{
		std::copy(std::begin(range), std::end(range), begin());
	}

	[[nodiscard]] explicit(false) operator std::span<T>() noexcept {
		return {buffer.data(), sz};
	}

	[[nodiscard]] size_t size() const noexcept { return sz; }
	[[nodiscard]] bool empty() const noexcept { return sz == 0; }
	[[nodiscard]] T* data() noexcept { return buffer.data(); }
	[[nodiscard]] auto begin() noexcept { return buffer.begin(); }
	[[nodiscard]] auto end()   noexcept { return buffer.begin() + sz; }

	[[nodiscard]] T& operator[](size_t index) {
		assert(index < sz);
		return buffer[index];
	}
	[[nodiscard]] T& front() {
		assert(!empty());
		return buffer.front();
	}
	[[nodiscard]] T& back() {
		assert(!empty());
		return buffer[sz - 1];
	}

private:
	std::array<T, BUF_SIZE> buffer;
	SizeType sz;
};

#endif
