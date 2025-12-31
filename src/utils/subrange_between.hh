#ifndef SUBRANGE_BETWEEN_HH
#define SUBRANGE_BETWEEN_HH

#include <algorithm>
#include <functional>
#include <ranges>

// subrange_between
//
// Find the subrange of all elements whose projected value lies in [l, h).
//
// This is equivalent to:
//   first = lower_bound(r, l, comp, proj);
//   last  = lower_bound(r, h, comp, proj);
//
// We implement this using a single equal_range() by mapping the range
// into three monotonic regions (-1, 0, +1) via a projection, and then
// searching for the contiguous block equal to 0.
//
// Parameters:
//   r    - a sorted forward range
//   l, h - lower and upper bound (half-open interval [l, h))
//   comp - optional comparator (default: std::ranges::less)
//   proj - optional projection (default: std::identity)
template<std::ranges::forward_range R,
         typename T,
         typename Comp = std::ranges::less,
         typename Proj = std::identity>
auto subrange_between(R&& r, const T& l, const T& h, Comp comp = {}, Proj proj = {})
{
	// Projection mapping: -1 if x<l, 0 if l<=x<h, +1 if h<=x
	auto map_range = [l,h,&comp,&proj](const auto& x) -> int {
		auto val = std::invoke(proj, x);
		if (std::invoke(comp, val, l)) return -1;  // val < l
		if (!std::invoke(comp, val, h)) return 1;  // h <= val
		return 0;                                  // l <= val < h
	};

	auto [first, last] = std::ranges::equal_range(r, 0, std::ranges::less{}, map_range);
	return std::ranges::subrange(first, last);
}

#endif
