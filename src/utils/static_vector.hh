#ifndef STATIC_VECTOR_HH
#define STATIC_VECTOR_HH

#include <algorithm>
#include <cassert>
#include <cstddef>
#include <cstdint>
#include <initializer_list>
#include <limits>
#include <span>
#include <type_traits>

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

template<typename T, size_t N>
class static_vector
{
	using SizeType =
		std::conditional_t<N <= std::numeric_limits<uint8_t >::max(), uint8_t,
	        std::conditional_t<N <= std::numeric_limits<uint16_t>::max(), uint16_t,
	        std::conditional_t<N <= std::numeric_limits<uint32_t>::max(), uint32_t,
	                                                                      uint64_t>>>;

public:
	constexpr static_vector() = default;

	constexpr static_vector(std::initializer_list<T> list) {
		assert(list.size() <= N);
		std::copy(list.begin(), list.end(), data);
		sz = SizeType(list.size());
	}

	[[nodiscard]] constexpr const T* begin() const noexcept { return data; }
	[[nodiscard]] constexpr const T* end()   const noexcept { return data + sz; }

	[[nodiscard]] constexpr size_t size() const { return sz; }
	[[nodiscard]] constexpr bool empty() const { return sz == 0; }

	[[nodiscard]] constexpr       T& operator[](size_t index)       { return data[index]; }
	[[nodiscard]] constexpr const T& operator[](size_t index) const { return data[index]; }

	constexpr void push_back(const T& a) { assert(sz < N); data[sz++] = a; }

	constexpr void clear() { sz = 0; }

	operator std::span<      T>()       { return {data, sz}; }
	operator std::span<const T>() const { return {data, sz}; }

private:
	T data[N] = {};
	SizeType sz = 0;
};

#endif
