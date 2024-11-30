#ifndef STATIC_VECTOR_HH
#define STATIC_VECTOR_HH

#include "ranges.hh"

#include <algorithm>
#include <array>
#include <cassert>
#include <cstddef>
#include <cstdint>
#include <initializer_list>
#include <limits>
#include <span>
#include <type_traits>
#include <utility>

// This is a _very_ minimal implementation of the following (we can easily
// extend this when the need arises).
//
//    static_vector
//    A dynamically-resizable vector with fixed capacity and embedded storage
//    http://www.open-std.org/jtc1/sc22/wg21/docs/papers/2018/p0843r2.html
//
// For us the main purpose of this class is to use it during constexpr
// (compile-time) calculations. Because for now, it's no yet possible to use
// heap memory in constexpr functions (there are proposals to loosen this
// restriction in future C++ standards).

struct from_range_t {};
inline constexpr from_range_t from_range;

template<typename T, size_t N>
class static_vector
{
	using SizeType =
	        std::conditional_t<N <= std::numeric_limits<uint8_t >::max(), uint8_t,
	        std::conditional_t<N <= std::numeric_limits<uint16_t>::max(), uint16_t,
	        std::conditional_t<N <= std::numeric_limits<uint32_t>::max(), uint32_t,
	                                                                      uint64_t>>>;

public:
	using value_type = T;
	using reference = value_type&;
	using const_reference = const value_type&;
	using iterator = value_type*;
	using const_iterator = const value_type*;
	using pointer = value_type*;
	using const_pointer = const value_type*;
	using size_type = size_t;
	using difference_type = ptrdiff_t;
	using reverse_iterator = std::reverse_iterator<iterator>;
	using const_reverse_iterator = std::reverse_iterator<const_iterator>;

	constexpr static_vector() = default;

	constexpr static_vector(std::initializer_list<T> list) {
		assert(list.size() <= N);
		ranges::copy(list, array);
		sz = SizeType(list.size());
	}

	template<typename Range>
	constexpr static_vector(from_range_t, Range&& range) {
		for (auto&& elem : range) {
			push_back(std::forward<decltype(elem)>(elem));
		}
	}

	[[nodiscard]] constexpr auto begin()        noexcept { return array.begin(); }
	[[nodiscard]] constexpr auto begin()  const noexcept { return array.begin(); }
	[[nodiscard]] constexpr auto end()          noexcept { return array.begin() + sz; }
	[[nodiscard]] constexpr auto end()    const noexcept { return array.begin() + sz; }
	[[nodiscard]] constexpr auto rbegin()       noexcept { return       reverse_iterator(end()); }
	[[nodiscard]] constexpr auto rbegin() const noexcept { return const_reverse_iterator(end()); }
	[[nodiscard]] constexpr auto rend()         noexcept { return       reverse_iterator(begin()); }
	[[nodiscard]] constexpr auto rend()   const noexcept { return const_reverse_iterator(begin()); }

	[[nodiscard]] constexpr size_t size()     const noexcept { return sz; }
	[[nodiscard]] constexpr size_t max_size() const noexcept { return array.max_size(); }
	[[nodiscard]] constexpr bool   empty()    const noexcept { return sz == 0; }

	[[nodiscard]] constexpr       T& operator[](size_t index)       noexcept { return array[index]; }
	[[nodiscard]] constexpr const T& operator[](size_t index) const noexcept { return array[index]; }
	[[nodiscard]] constexpr T&       front()       noexcept { assert(sz > 0); return array[0]; }
	[[nodiscard]] constexpr const T& front() const noexcept { assert(sz > 0); return array[0]; }
	[[nodiscard]] constexpr T&       back()        noexcept { assert(sz > 0); return array[sz - 1]; }
	[[nodiscard]] constexpr const T& back()  const noexcept { assert(sz > 0); return array[sz - 1]; }
	[[nodiscard]] constexpr       T* data()        noexcept { return array.data(); }
	[[nodiscard]] constexpr const T* data()  const noexcept { return array.data(); }

	constexpr void push_back(const T& a) { assert(sz < N); array[sz++] = a; }
	constexpr void push_back(T&& a)      { assert(sz < N); array[sz++] = std::move(a); }
	template<typename... Args> constexpr T& emplace_back(Args&&... args) {
		assert(sz < N);
		array[sz++] = T(std::forward<Args>(args)...);
		return array[sz - 1];
	}
	constexpr void pop_back()   noexcept { assert(sz > 0); sz--; }
	constexpr void clear()      noexcept { sz = 0; }

	operator std::span<      T>()       noexcept { return {array.data(), sz}; }
	operator std::span<const T>() const noexcept { return {array.data(), sz}; }

private:
	std::array<T, N> array = {};
	SizeType sz = 0;
};

#endif
