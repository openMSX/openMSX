#ifndef RANGES_HH
#define RANGES_HH

#include "stl.hh"
#include <algorithm>
#include <functional>
#include <iterator> // for std::begin(), std::end()
#include <numeric>

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

template<typename ForwardRange, typename Compare = std::less<>, typename Proj = identity>
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
void sort(RAIter first, RAIter last, Compare comp, Proj proj)
{
	std::sort(first, last,
		[&](const auto& x, const auto& y) {
			return comp(std::invoke(proj, x), std::invoke(proj, y));
		});
}

template<typename RandomAccessRange, typename Compare = std::less<>, typename Proj>
void sort(RandomAccessRange&& range, Compare comp, Proj proj)
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

template<typename ForwardRange, typename T, typename Compare = std::less<>, typename Proj = identity>
[[nodiscard]] auto lower_bound(ForwardRange&& range, const T& value, Compare comp = {}, Proj proj = {})
{
	auto comp2 = [&](const auto& x, const auto& y) {
		return comp(std::invoke(proj, x), y);
	};
	return std::lower_bound(std::begin(range), std::end(range), value, comp2);
}

template<typename ForwardRange, typename T, typename Compare = std::less<>, typename Proj = identity>
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
template<typename ForwardRange, typename T, typename Compare = std::less<>, typename Proj = identity>
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
	return std::find_if(std::begin(range), std::end(range), pred);
}

template<typename InputRange, typename UnaryPredicate>
[[nodiscard]] bool all_of(InputRange&& range, UnaryPredicate pred)
{
	return std::all_of(std::begin(range), std::end(range), pred);
}

template<typename InputRange, typename UnaryPredicate>
[[nodiscard]] bool any_of(InputRange&& range, UnaryPredicate pred)
{
	return std::any_of(std::begin(range), std::end(range), pred);
}

template<typename InputRange, typename UnaryPredicate>
[[nodiscard]] bool none_of(InputRange&& range, UnaryPredicate pred)
{
	return std::none_of(std::begin(range), std::end(range), pred);
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
auto copy(InputRange&& range, OutputIter out)
{
	return std::copy(std::begin(range), std::end(range), out);
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
void fill(ForwardRange&& range, const T& value)
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
	iota(std::begin(range), std::end(range), std::forward<T>(value));
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

} // namespace ranges

#endif
