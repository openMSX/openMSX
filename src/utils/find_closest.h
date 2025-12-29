#ifndef FIND_CLOSEST_H
#define FIND_CLOSEST_H

#include <algorithm>
#include <functional>
#include <iterator>
#include <ranges>
#include <type_traits>
#include <utility>

// Find the element in a sorted range that is closest to the requested value.
// @pre: is_sorted(range, comp, proj)
//
// The return value contains:
// * An iterator pointing at the closest element (or end() when the range is
//   empty). When two neighboring elements are equally close, the earlier
//   element is returned.
// * The absolute distance between that element and the requested value. When
//   the range is empty the distance is unspecified.
template<std::ranges::bidirectional_range Range, typename T,
         typename Compare = std::ranges::less, typename Proj = std::identity>
[[nodiscard]] constexpr auto find_closest(
	Range&& range, const T& value, Compare comp = {}, Proj proj = {})
{
	using Ref = std::ranges::range_reference_t<Range>;
	using Key = std::remove_cvref_t<std::invoke_result_t<Proj&, Ref>>;
	using LeftDiff = std::remove_cvref_t<decltype(std::declval<Key>() - std::declval<T>())>;
	using RightDiff = std::remove_cvref_t<decltype(std::declval<T>() - std::declval<Key>())>;
	static_assert(std::is_same_v<LeftDiff, RightDiff>,
	              "find_closest() requires symmetric subtraction results.");
	using Diff = LeftDiff;
	using Iterator = std::ranges::iterator_t<Range>;
	using Result = std::pair<Iterator, Diff>;

	auto first = std::ranges::begin(range);
	auto last = std::ranges::end(range);
	auto it = std::ranges::lower_bound(range, value, comp, proj);

	if (it == first) {
		if (it == last) return Result{last, Diff{}};
		return Result{it, std::invoke(proj, *it) - value};
	}
	if (it == last) {
		auto prev = std::prev(last);
		return Result{prev, value - std::invoke(proj, *prev)};
	}

	auto prev = std::prev(it);
	auto distPrev = value - std::invoke(proj, *prev);
	auto distCurr = std::invoke(proj, *it) - value;
	return (distPrev <= distCurr) ? Result{prev, distPrev}
	                              : Result{it, distCurr};
}

#endif
