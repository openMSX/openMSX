#ifndef RANGES_HH
#define RANGES_HH

#include <algorithm>
#include <cassert>
#include <functional>
#include <iterator> // for std::begin(), std::end()
#include <numeric>
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

// (Simplified) implementation of c++20 "range" and "sized_range" concepts.
// * A "range" is something that has a begin() and end().
// * A "sized_range" in addition has a size().
template<typename T>
concept range = requires(T& t) {
	std::begin(t);
	std::end  (t);
};

template<typename T>
concept sized_range = range<T> && requires(T& t) { std::size(t); };


template<typename ForwardRange, typename Compare = std::less<>, typename Proj = std::identity>
[[nodiscard]] bool is_sorted(ForwardRange&& range, Compare comp = {}, Proj proj = {})
{
	return std::is_sorted(std::begin(range), std::end(range),
		[&](const auto& x, const auto& y) {
			return comp(std::invoke(proj, x), std::invoke(proj, y));
		});
}

template<typename RandomAccessRange>
constexpr void sort(RandomAccessRange&& range)
{
	std::sort(std::begin(range), std::end(range));
}

template<typename RandomAccessRange, typename Compare>
constexpr void sort(RandomAccessRange&& range, Compare comp)
{
	std::sort(std::begin(range), std::end(range), comp);
}

template<typename RAIter, typename Compare = std::less<>, typename Proj>
constexpr void sort(RAIter first, RAIter last, Compare comp, Proj proj)
{
	std::sort(first, last,
		[&](const auto& x, const auto& y) {
			return comp(std::invoke(proj, x), std::invoke(proj, y));
		});
}

template<typename RandomAccessRange, typename Compare = std::less<>, typename Proj>
constexpr void sort(RandomAccessRange&& range, Compare comp, Proj proj)
{
	sort(std::begin(range), std::end(range), comp, proj);
}

template<typename RandomAccessRange>
void stable_sort(RandomAccessRange&& range)
{
	std::stable_sort(std::begin(range), std::end(range));
}

template<typename RandomAccessRange, typename Compare>
void stable_sort(RandomAccessRange&& range, Compare comp)
{
	std::stable_sort(std::begin(range), std::end(range), comp);
}

template<typename RAIter, typename Compare = std::less<>, typename Proj>
void stable_sort(RAIter first, RAIter last, Compare comp, Proj proj)
{
	std::stable_sort(first, last,
		[&](const auto& x, const auto& y) {
			return comp(std::invoke(proj, x), std::invoke(proj, y));
		});
}

template<typename RandomAccessRange, typename Compare = std::less<>, typename Proj>
void stable_sort(RandomAccessRange&& range, Compare comp, Proj proj)
{
	stable_sort(std::begin(range), std::end(range), comp, proj);
}

template<typename ForwardRange, typename T>
[[nodiscard]] bool binary_search(ForwardRange&& range, const T& value)
{
	return std::binary_search(std::begin(range), std::end(range), value);
}

template<typename ForwardRange, typename T, typename Compare>
[[nodiscard]] bool binary_search(ForwardRange&& range, const T& value, Compare comp)
{
	return std::binary_search(std::begin(range), std::end(range), value, comp);
}

template<typename ForwardRange, typename T, typename Compare = std::less<>, typename Proj = std::identity>
[[nodiscard]] auto lower_bound(ForwardRange&& range, const T& value, Compare comp = {}, Proj proj = {})
{
	auto comp2 = [&](const auto& x, const auto& y) {
		return comp(std::invoke(proj, x), y);
	};
	return std::lower_bound(std::begin(range), std::end(range), value, comp2);
}

template<typename ForwardRange, typename T, typename Compare = std::less<>, typename Proj = std::identity>
[[nodiscard]] auto upper_bound(ForwardRange&& range, const T& value, Compare comp = {}, Proj proj = {})
{
	auto comp2 = [&](const auto& x, const auto& y) {
		return comp(x, std::invoke(proj, y));
	};
	return std::upper_bound(std::begin(range), std::end(range), value, comp2);
}

template<typename ForwardRange, typename T, typename Compare = std::less<>>
[[nodiscard]] auto equal_range(ForwardRange&& range, const T& value, Compare comp = {})
{
	return std::equal_range(std::begin(range), std::end(range), value, comp);
}
template<typename ForwardRange, typename T, typename Compare = std::less<>, typename Proj = std::identity>
[[nodiscard]] auto equal_range(ForwardRange&& range, const T& value, Compare comp, Proj proj)
{
	using Iter = decltype(std::begin(range));
	using R = typename std::iterator_traits<Iter>::value_type;
	struct Comp2 {
		Compare comp;
		Proj proj;

		bool operator()(const R& x, const R& y) const {
			return comp(std::invoke(proj, x), std::invoke(proj, y));
		}
		bool operator()(const R& x, const T& y) const {
			return comp(std::invoke(proj, x), y);
		}
		bool operator()(const T& x, const R& y) const {
			return comp(x, std::invoke(proj, y));
		}
	};
	return std::equal_range(std::begin(range), std::end(range), value, Comp2{comp, proj});
}

template<typename InputRange, typename T>
[[nodiscard]] auto find(InputRange&& range, const T& value)
{
	return std::find(std::begin(range), std::end(range), value);
}

template<typename InputRange, typename T, typename Proj>
[[nodiscard]] auto find(InputRange&& range, const T& value, Proj proj)
{
	return find_if(std::forward<InputRange>(range),
	               [&](const auto& e) { return std::invoke(proj, e) == value; });
}

template<typename InputRange, typename UnaryPredicate>
[[nodiscard]] auto find_if(InputRange&& range, UnaryPredicate pred)
{
	auto it = std::begin(range);
	auto et = std::end(range);
	for (/**/; it != et; ++it) {
		if (std::invoke(pred, *it)) {
			return it;
		}
	}
	return it;
}

template<typename InputRange, typename UnaryPredicate>
[[nodiscard]] bool all_of(InputRange&& range, UnaryPredicate pred)
{
	//return std::all_of(std::begin(range), std::end(range), pred);
	auto it = std::begin(range);
	auto et = std::end(range); // allow 'it' and 'et' to have different types
	for (/**/; it != et; ++it) {
		if (!std::invoke(pred, *it)) return false;
	}
	return true;
}

template<typename InputRange, typename UnaryPredicate>
[[nodiscard]] bool any_of(InputRange&& range, UnaryPredicate pred)
{
	//return std::any_of(std::begin(range), std::end(range), pred);
	auto it = std::begin(range);
	auto et = std::end(range); // allow 'it' and 'et' to have different types
	for (/**/; it != et; ++it) {
		if (std::invoke(pred, *it)) return true;
	}
	return false;
}

template<typename InputRange, typename UnaryPredicate>
[[nodiscard]] bool none_of(InputRange&& range, UnaryPredicate pred)
{
	//return std::none_of(std::begin(range), std::end(range), pred);
	auto it = std::begin(range);
	auto et = std::end(range); // allow 'it' and 'et' to have different types
	for (/**/; it != et; ++it) {
		if (std::invoke(pred, *it)) return false;
	}
	return true;
}

template<typename ForwardRange>
[[nodiscard]] auto unique(ForwardRange&& range)
{
	return std::unique(std::begin(range), std::end(range));
}

template<typename ForwardRange, typename BinaryPredicate>
[[nodiscard]] auto unique(ForwardRange&& range, BinaryPredicate pred)
{
	return std::unique(std::begin(range), std::end(range), pred);
}

template<typename RAIter, typename Compare = std::equal_to<>, typename Proj>
[[nodiscard]] auto unique(RAIter first, RAIter last, Compare comp, Proj proj)
{
	return std::unique(first, last,
		[&](const auto& x, const auto& y) {
			return comp(std::invoke(proj, x), std::invoke(proj, y));
		});
}

template<typename RandomAccessRange, typename Compare = std::equal_to<>, typename Proj>
[[nodiscard]] auto unique(RandomAccessRange&& range, Compare comp, Proj proj)
{
	return unique(std::begin(range), std::end(range), comp, proj);
}

template<typename InputRange, typename OutputIter>
	requires(!range<OutputIter>)
auto copy(InputRange&& range, OutputIter out)
{
	return std::copy(std::begin(range), std::end(range), out);
}

template<sized_range Input, sized_range Output>
auto copy(Input&& in, Output&& out)
{
	assert(std::size(in) <= std::size(out));
	return std::copy(std::begin(in), std::end(in), std::begin(out));
}

template<typename InputRange, typename OutputIter, typename UnaryPredicate>
auto copy_if(InputRange&& range, OutputIter out, UnaryPredicate pred)
{
	return std::copy_if(std::begin(range), std::end(range), out, pred);
}

template<typename InputRange, typename OutputIter, typename UnaryOperation>
auto transform(InputRange&& range, OutputIter out, UnaryOperation op)
{
	return std::transform(std::begin(range), std::end(range), out, op);
}

template<typename ForwardRange, typename Generator>
void generate(ForwardRange&& range, Generator&& g)
{
	std::generate(std::begin(range), std::end(range), std::forward<Generator>(g));
}

template<typename ForwardRange, typename T>
[[nodiscard]] auto remove(ForwardRange&& range, const T& value)
{
	return std::remove(std::begin(range), std::end(range), value);
}

template<typename ForwardRange, typename UnaryPredicate>
[[nodiscard]] auto remove_if(ForwardRange&& range, UnaryPredicate pred)
{
	return std::remove_if(std::begin(range), std::end(range), pred);
}

template<typename ForwardRange, typename T>
constexpr void replace(ForwardRange&& range, const T& old_value, const T& new_value)
{
	std::replace(std::begin(range), std::end(range), old_value, new_value);
}

template<typename ForwardRange, typename UnaryPredicate, typename T>
void replace_if(ForwardRange&& range, UnaryPredicate pred, const T& new_value)
{
	std::replace_if(std::begin(range), std::end(range), pred, new_value);
}

template<typename ForwardRange, typename T>
constexpr void fill(ForwardRange&& range, const T& value)
{
	std::fill(std::begin(range), std::end(range), value);
}

// part of c++20
template<typename ForwardIt, typename T>
constexpr void iota(ForwardIt first, ForwardIt last, T value)
{
    while (first != last) {
        *first++ = value;
        ++value;
    }
}

template<typename ForwardRange, typename T>
constexpr void iota(ForwardRange&& range, T&& value)
{
	::ranges::iota(std::begin(range), std::end(range), std::forward<T>(value));
}

template<typename InputRange, typename T>
[[nodiscard]] T accumulate(InputRange&& range, T init)
{
	return std::accumulate(std::begin(range), std::end(range), init);
}

template<typename InputRange, typename T, typename BinaryOperation>
[[nodiscard]] T accumulate(InputRange&& range, T init, BinaryOperation op)
{
	return std::accumulate(std::begin(range), std::end(range), init, op);
}

template<typename InputRange, typename T>
[[nodiscard]] auto count(InputRange&& range, const T& value)
{
	return std::count(std::begin(range), std::end(range), value);
}

template<typename InputRange, typename UnaryPredicate>
[[nodiscard]] auto count_if(InputRange&& range, UnaryPredicate pred)
{
	return std::count_if(std::begin(range), std::end(range), pred);
}

template<typename InputRange1, typename InputRange2, typename OutputIter>
auto set_difference(InputRange1&& range1, InputRange2&& range2, OutputIter out)
{
	return std::set_difference(std::begin(range1), std::end(range1),
	                           std::begin(range2), std::end(range2),
	                           out);
}

template<range InputRange1, range InputRange2,
         typename Pred = std::equal_to<void>,
         typename Proj1 = std::identity, typename Proj2 = std::identity>
bool equal(InputRange1&& range1, InputRange2&& range2, Pred pred = {},
           Proj1 proj1 = {}, Proj2 proj2 = {})
{
	auto it1 = std::begin(range1);
	auto it2 = std::begin(range2);
	auto et1 = std::end(range1);
	auto et2 = std::end(range2);
	for (/**/; (it1 != et1) && (it2 != et2); ++it1, ++it2) {
		if (!std::invoke(pred, std::invoke(proj1, *it1), std::invoke(proj2, *it2))) {
			return false;
		}
	}
	return (it1 == et1) && (it2 == et2);
}

template<sized_range SizedRange1, sized_range SizedRange2,
         typename Pred = std::equal_to<void>,
         typename Proj1 = std::identity, typename Proj2 = std::identity>
bool equal(SizedRange1&& range1, SizedRange2&& range2, Pred pred = {},
           Proj1 proj1 = {}, Proj2 proj2 = {})
{
	if (std::size(range1) != std::size(range2)) return false;

	auto it1 = std::begin(range1);
	auto it2 = std::begin(range2);
	auto et1 = std::end(range1);
	for (/**/; it1 != et1; ++it1, ++it2) {
		if (!std::invoke(pred, std::invoke(proj1, *it1), std::invoke(proj2, *it2))) {
			return false;
		}
	}
	return true;
}

// Test whether all elements in the given range are equal to each other (after
// applying a projection).
template<typename InputRange, typename Proj = std::identity>
bool all_equal(InputRange&& range, Proj proj = {})
{
	auto it = std::begin(range);
	auto et = std::end(range);
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

// Perform a binary-search in a sorted range.
// The given 'range' must be sorted according to 'comp'.
// We search for 'value' after applying 'proj' to the elements in the range.
// Returns a pointer to the element in 'range', or nullptr if not found.
//
// This helper function is typically used to simplify code like:
//     auto it = ranges::lower_bound(myMap, name, {}, &Element::name);
//     if ((it != myMap.end()) && (it->name == name)) {
//         ... use *it
//     }
// Note that this code needs to check both for the end-iterator and that the
// element was actually found. By using binary_find() this complexity is hidden:
//     if (auto m = binary_find(myMap, name, {}, &Element::name)) {
//         ... use *m
//     }
template<typename ForwardRange, typename T, typename Compare = std::less<>, typename Proj = std::identity>
[[nodiscard]] auto* binary_find(ForwardRange&& range, const T& value, Compare comp = {}, Proj proj = {})
{
	auto it = ranges::lower_bound(range, value, comp, proj);
	return ((it != std::end(range)) && (!std::invoke(comp, value, std::invoke(proj, *it))))
	       ? &*it
	       : nullptr;
}

// Convenience function to convert part of an array (or vector, ...) into a
// span. These are not part of the c++ ranges namespace, but often these results
// are then further used in range algorithms, that's why I placed them in this
// header.

template<typename Range>
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

template<typename Range>
constexpr auto subspan(Range&& range, size_t offset, size_t count = std::dynamic_extent)
{
	return make_span(std::forward<Range>(range)).subspan(offset, count);
}

template<size_t Count, typename Range>
constexpr auto subspan(Range&& range, size_t offset = 0)
{
	return make_span(std::forward<Range>(range)).subspan(offset).template first<Count>();
}

#endif
