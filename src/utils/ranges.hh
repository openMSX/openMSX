#ifndef RANGES_HH
#define RANGES_HH

#include <algorithm>
#include <bit>
#include <cassert>
#include <cstdint>
#include <functional>
#include <iterator>
#include <ranges>
#include <span>
#include <version> // for _LIBCPP_VERSION

// Range based versions of the standard algorithms, these will likely become
// part of c++20. For example see this post:
//   http://ericniebler.com/2018/12/05/standard-ranges/
// In the future we can remove our implementation and instead use the standard
// version (e.g. by a namespace alias like 'namespace ranges = std::ranges').
//
// All of the range algorithms below do nothing more than delegate to the
// corresponding iterator-pair version of the algorithm.
//
// This list of algorithms is not complete. But it's easy to extend if/when we
// need more.

namespace ranges {

// part of c++23, not yet available in clang-16
template<std::forward_iterator ForwardIt, typename T>
constexpr void iota(ForwardIt first, ForwardIt last, T value)
{
	while (first != last) {
		*first++ = value;
		++value;
	}
}

template<std::ranges::forward_range Range, typename T>
constexpr void iota(Range&& range, T&& value)
{
	::ranges::iota(std::ranges::begin(range), std::ranges::end(range), std::forward<T>(value));
}

// Test whether all elements in the given range are equal to each other (after
// applying a projection).
template<std::ranges::input_range Range, typename Proj = std::identity>
bool all_equal(Range&& range, Proj proj = {})
{
	auto it = std::ranges::begin(range);
	auto et = std::ranges::end(range);
	if (it == et) return true;

	auto val = std::invoke(proj, *it);
	for (++it; it != et; ++it) {
		if (std::invoke(proj, *it) != val) {
			return false;
		}
	}
	return true;
}

} // namespace ranges

// Copy from a (sized-)range to another (non-overlapping) (sized-)range.
// This differs from std::ranges::copy() in how the destination is passed:
// * This version requires a (sized-)range.
// * std::ranges::copy() requires an output-iterator.
// The latter is more general, but the former can do an extra safety check:
// * The destination size must be as least as large as the source size.
template<std::ranges::sized_range Input, std::ranges::sized_range Output>
constexpr auto copy_to_range(Input&& in, Output&& out)
{
	assert(std::ranges::size(in) <= std::ranges::size(out));
	// Workaround: this doesn't work when copying a std::initializer_list into a std::array with libstdc++ debug-STL ???
	//     return std::copy(std::begin(in), std::end(in), std::begin(out));
	auto f = std::ranges::begin(in);
	auto l = std::ranges::end(in);
	auto o = std::ranges::begin(out);
	while (f != l) {
		*o++ = *f++;
	}
	return o;
}

// Perform a binary-search in a sorted range.
// The given 'range' must be sorted according to 'comp'.
// We search for 'value' after applying 'proj' to the elements in the range.
// Returns a pointer to the element in 'range', or nullptr if not found.
//
// This helper function is typically used to simplify code like:
//     auto it = std::ranges::lower_bound(myMap, name, {}, &Element::name);
//     if ((it != myMap.end()) && (it->name == name)) {
//         ... use *it
//     }
// Note that this code needs to check both for the end-iterator and that the
// element was actually found. By using binary_find() this complexity is hidden:
//     if (auto m = binary_find(myMap, name, {}, &Element::name)) {
//         ... use *m
//     }
template<std::ranges::forward_range Range, typename T, typename Compare = std::less<>, typename Proj = std::identity>
[[nodiscard]] auto* binary_find(Range&& range, const T& value, Compare comp = {}, Proj proj = {})
{
	auto it = std::ranges::lower_bound(range, value, comp, proj);
	return ((it != std::ranges::end(range)) && (!std::invoke(comp, value, std::invoke(proj, *it))))
	       ? &*it
	       : nullptr;
}

// Convenience function to convert part of an array (or vector, ...) into a
// span. These are not part of the c++ ranges namespace, but often these results
// are then further used in range algorithms, that's why I placed them in this
// header.

template<std::ranges::range Range>
constexpr auto make_span(Range&& range)
{
#ifndef _LIBCPP_VERSION
	// C++20 version, works with gcc/visual studio
	// Simple enough that we don't actually need this helper function.
	return std::span(range);
#else
	// Unfortunately we do need a workaround for clang/libc++, version 14 and 15
	// return std::span(std::begin(range), std::end(range));

	// Further workaround for Xcode-14, clang-13
	// Don't always use this workaround, because '&*begin()' is actually
	// undefined behavior when the range is empty. It works fine in
	// practice, except when using a DEBUG-STL version.
	return std::span(&*std::begin(range), std::size(range));
#endif
}

template<std::ranges::range Range>
[[nodiscard]] constexpr auto subspan(Range&& range, size_t offset, size_t count = std::dynamic_extent)
{
	return make_span(std::forward<Range>(range)).subspan(offset, count);
}

template<size_t Count, std::ranges::range Range>
[[nodiscard]] constexpr auto subspan(Range&& range, size_t offset = 0)
{
	return make_span(std::forward<Range>(range)).subspan(offset).template first<Count>();
}

template<typename T, size_t Size>
[[nodiscard]] inline auto as_byte_span(std::span<T, Size> s)
{
	return std::span{std::bit_cast<const uint8_t*>(s.data()), s.size_bytes()};
}

#endif
