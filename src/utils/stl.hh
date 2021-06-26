#ifndef STL_HH
#define STL_HH

#include <algorithm>
#include <cassert>
#include <iterator>
#include <initializer_list>
#include <map>
#include <numeric>
#include <tuple>
#include <utility>
#include <vector>

// Reimplementation of c++20 std::identity.
struct identity {
	template<typename T>
	[[nodiscard]] constexpr T&& operator()(T&& t) const noexcept {
		return std::forward<T>(t);
	}
};


// Dereference the two given (pointer-like) parameters and then compare
// them with the less-than operator.
struct LessDeref
{
	template<typename PTR>
	[[nodiscard]] constexpr bool operator()(PTR p1, PTR p2) const { return *p1 < *p2; }
};


// Compare the N-th element of two tuples using a custom comparison functor.
// Also provides overloads to compare the N-the element of a tuple with a
// single value (of a compatible type).
// ATM the functor cannot take constructor arguments, possibly refactor this in
// the future.
template<int N, typename CMP> struct CmpTupleElement
{
	template<typename... Args>
	[[nodiscard]] constexpr bool operator()(const std::tuple<Args...>& x, const std::tuple<Args...>& y) const {
		return cmp(std::get<N>(x), std::get<N>(y));
	}

	template<typename T, typename... Args>
	[[nodiscard]] constexpr bool operator()(const T& x, const std::tuple<Args...>& y) const {
		return cmp(x, std::get<N>(y));
	}

	template<typename T, typename... Args>
	[[nodiscard]] constexpr bool operator()(const std::tuple<Args...>& x, const T& y) const {
		return cmp(std::get<N>(x), y);
	}

	template<typename T1, typename T2>
	[[nodiscard]] constexpr bool operator()(const std::pair<T1, T2>& x, const std::pair<T1, T2>& y) const {
		return cmp(std::get<N>(x), std::get<N>(y));
	}

	template<typename T, typename T1, typename T2>
	[[nodiscard]] constexpr bool operator()(const T& x, const std::pair<T1, T2>& y) const {
		return cmp(x, std::get<N>(y));
	}

	template<typename T, typename T1, typename T2>
	[[nodiscard]] constexpr bool operator()(const std::pair<T1, T2>& x, const T& y) const {
		return cmp(std::get<N>(x), y);
	}

private:
	CMP cmp;
};

// Similar to CmpTupleElement above, but uses the less-than operator.
template<int N> using LessTupleElement = CmpTupleElement<N, std::less<>>;


// Check whether the N-the element of a tuple is equal to the given value.
template<int N, typename T> struct EqualTupleValueImpl
{
	explicit EqualTupleValueImpl(const T& t_) : t(t_) {}
	template<typename TUPLE>
	[[nodiscard]] constexpr bool operator()(const TUPLE& tup) const {
		return std::get<N>(tup) == t;
	}
private:
	const T& t;
};
template<int N, typename T>
[[nodiscard]] constexpr auto EqualTupleValue(const T& t) {
	return EqualTupleValueImpl<N, T>(t);
}


/** Check if a range contains a given value, using linear search.
  * Equivalent to 'find(first, last, val) != last', though this algorithm
  * is more convenient to use.
  * Note: we don't need a variant that uses 'find_if' instead of 'find' because
  * STL already has the 'any_of' algorithm.
  */
template<typename ITER, typename VAL>
[[nodiscard]] constexpr bool contains(ITER first, ITER last, const VAL& val)
{
	// c++20: return std::find(first, last, val) != last;
	while (first != last) {
		if (*first == val) return true;
		++first;
	}
	return false;
}
template<typename RANGE, typename VAL>
[[nodiscard]] constexpr bool contains(const RANGE& range, const VAL& val)
{
	return contains(std::begin(range), std::end(range), val);
}


/** Faster alternative to 'find' when it's guaranteed that the value will be
  * found (if not the behavior is undefined).
  * When asserts are enabled we check whether we really don't move beyond the
  * end of the range. And this check is the only reason why you need to pass
  * the 'last' parameter. Sometimes you see 'find_unguarded' without a 'last'
  * parameter, we could consider providing such an overload as well.
  */
template<typename ITER, typename VAL, typename Proj = identity>
[[nodiscard]] /*constexpr*/ ITER find_unguarded(ITER first, ITER last, const VAL& val, Proj proj = {})
{
	return find_if_unguarded(first, last,
		[&](const auto& e) { return std::invoke(proj, e) == val; });
}
template<typename RANGE, typename VAL, typename Proj = identity>
[[nodiscard]] /*constexpr*/ auto find_unguarded(RANGE& range, const VAL& val, Proj proj = {})
{
	return find_unguarded(std::begin(range), std::end(range), val, proj);
}

/** Faster alternative to 'find_if' when it's guaranteed that the predicate
  * will be true for at least one element in the given range.
  * See also 'find_unguarded'.
  */
template<typename ITER, typename PRED>
[[nodiscard]] constexpr ITER find_if_unguarded(ITER first, ITER last, PRED pred)
{
	(void)last;
	while (true) {
		assert(first != last);
		if (pred(*first)) return first;
		++first;
	}
}
template<typename RANGE, typename PRED>
[[nodiscard]] constexpr auto find_if_unguarded(RANGE& range, PRED pred)
{
	return find_if_unguarded(std::begin(range), std::end(range), pred);
}

/** Similar to the find(_if)_unguarded functions above, but searches from the
  * back to front.
  * Note that we only need to provide range versions. Because for the iterator
  * versions it is already possible to pass reverse iterators.
  */
template<typename RANGE, typename VAL, typename Proj = identity>
[[nodiscard]] /*constexpr*/ auto rfind_unguarded(RANGE& range, const VAL& val, Proj proj = {})
{
	auto it = find_unguarded(std::rbegin(range), std::rend(range), val, proj);
	++it;
	return it.base();
}

template<typename RANGE, typename PRED>
[[nodiscard]] constexpr auto rfind_if_unguarded(RANGE& range, PRED pred)
{
	auto it = find_if_unguarded(std::rbegin(range), std::rend(range), pred);
	++it;
	return it.base();
}


/** Erase the pointed to element from the given vector.
  *
  * We first move the last element into the indicated position and then pop
  * the last element.
  *
  * If the order of the elements in the vector is allowed to change this may
  * be a faster alternative than calling 'v.erase(it)'.
  */
template<typename VECTOR>
void move_pop_back(VECTOR& v, typename VECTOR::iterator it)
{
	// Check for self-move-assignment.
	//
	// This check is only needed in libstdc++ when compiled with
	// -D_GLIBCXX_DEBUG. In non-debug mode this routine works perfectly
	// fine without the check.
	//
	// See here for a related discussion:
	//    http://stackoverflow.com/questions/13129031/on-implementing-stdswap-in-terms-of-move-assignment-and-move-constructor
	// It's not clear whether the assert in libstdc++ is conforming
	// behavior.
	if (&*it != &v.back()) {
		*it = std::move(v.back());
	}
	v.pop_back();
}


/** This is like a combination of partition_copy() and remove().
 *
  * Each element is tested against the predicate. If it matches, the element is
  * copied to the output and removed from the input. So the output contains all
  * elements for which the predicate gives true and the input is modified
  * in-place to contain the elements for which the predicate gives false. (Like
  * the remove() algorithm, it's still required to call erase() to also shrink
  * the input).
  *
  * This algorithm returns a pair of iterators.
  * -first: one past the matching elements (in the output range). Similar to
  *    the return value of the partition_copy() algorithm.
  * -second: one past the non-matching elements (in the input range). Similar
  *    to the return value of the remove() algorithm.
  */
template<typename ForwardIt, typename OutputIt, typename UnaryPredicate>
[[nodiscard]] std::pair<OutputIt, ForwardIt> partition_copy_remove(
	ForwardIt first, ForwardIt last, OutputIt out_true, UnaryPredicate p)
{
	first = std::find_if(first, last, p);
	auto out_false = first;
	if (first != last) {
		goto l_true;
		while (first != last) {
			if (p(*first)) {
l_true:				*out_true++  = std::move(*first++);
			} else {
				*out_false++ = std::move(*first++);
			}
		}
	}
	return std::pair(out_true, out_false);
}

template<typename ForwardRange, typename OutputIt, typename UnaryPredicate>
[[nodiscard]] auto partition_copy_remove(ForwardRange&& range, OutputIt out_true, UnaryPredicate p)
{
	return partition_copy_remove(std::begin(range), std::end(range), out_true, p);
}


// Like range::transform(), but with equal source and destination.
template<typename ForwardRange, typename UnaryOperation>
auto transform_in_place(ForwardRange&& range, UnaryOperation op)
{
	return std::transform(std::begin(range), std::end(range), std::begin(range), op);
}


// Returns (a copy of) the minimum value in [first, last).
// Requires: first != last.
template<typename InputIterator>
[[nodiscard]] constexpr auto min_value(InputIterator first, InputIterator last)
{
	assert(first != last);
	auto result = *first++;
	while (first != last) {
		result = std::min(result, *first++);
	}
	return result;
}

template<typename InputRange>
[[nodiscard]] constexpr auto min_value(InputRange&& range)
{
	return min_value(std::begin(range), std::end(range));
}

// Returns (a copy of) the maximum value in [first, last).
// Requires: first != last.
template<typename InputIterator>
[[nodiscard]] constexpr auto max_value(InputIterator first, InputIterator last)
{
	assert(first != last);
	auto result = *first++;
	while (first != last) {
		result = std::max(result, *first++);
	}
	return result;
}

template<typename InputRange>
[[nodiscard]] constexpr auto max_value(InputRange&& range)
{
	return max_value(std::begin(range), std::end(range));
}


// Returns the sum of the elements in the given range.
// Assumes: elements can be summed via operator+, with a default constructed
// value being the identity-element for this operator.
template<typename InputRange>
[[nodiscard]] auto sum(InputRange&& range)
{
	using Iter = decltype(std::begin(range));
	using T = typename std::iterator_traits<Iter>::value_type;
	return std::accumulate(std::begin(range), std::end(range), T());
}


// to_vector
namespace detail {
	template<typename T, typename Iterator>
	using ToVectorType = std::conditional_t<
		std::is_same_v<T, void>,
		typename std::iterator_traits<Iterator>::value_type,
		T>;
}

// Convert any range to a vector. Optionally specify the type of the elements
// in the result.
// Example:
//   auto v1 = to_vector(view::drop(my_list, 3));
//   auto v2 = to_vector<Base*>(getDerivedPtrs());
template<typename T = void, typename Range>
[[nodiscard]] auto to_vector(Range&& range)
	-> std::vector<detail::ToVectorType<T, decltype(std::begin(range))>>
{
	return {std::begin(range), std::end(range)};
}

// Optimized version for r-value input and no type conversion.
template<typename T>
[[nodiscard]] auto to_vector(std::vector<T>&& v)
{
	return std::move(v);
}


// append() / concat()
namespace detail {

template<typename... Ranges>
[[nodiscard]] constexpr size_t sum_of_sizes(const Ranges&... ranges)
{
    return (0 + ... + std::distance(std::begin(ranges), std::end(ranges)));
}

template<typename Result>
void append(Result&)
{
	// nothing
}

template<typename Result, typename Range, typename... Tail>
void append(Result& x, Range&& y, Tail&&... tail)
{
#ifdef _GLIBCXX_DEBUG
	// Range can be a view::transform
	// but vector::insert in libstdc++ debug mode will wrongly try
	// to take its address to check for self-insertion (see
	// gcc/include/c++/7.1.0/debug/functions.h
	// __foreign_iterator_aux functions). So avoid vector::insert
	for (auto&& e : y) {
		x.emplace_back(std::forward<decltype(e)>(e));
	}
#else
	x.insert(std::end(x), std::begin(y), std::end(y));
#endif
	detail::append(x, std::forward<Tail>(tail)...);
}

// Allow move from an rvalue-vector.
// But don't allow to move from any rvalue-range. It breaks stuff like
//   append(v, view::reverse(w));
template<typename Result, typename T2, typename... Tail>
void append(Result& x, std::vector<T2>&& y, Tail&&... tail)
{
	x.insert(std::end(x),
		 std::move_iterator(std::begin(y)),
		 std::move_iterator(std::end(y)));
	detail::append(x, std::forward<Tail>(tail)...);
}

} // namespace detail

// Append a range to a vector.
template<typename T, typename... Tail>
void append(std::vector<T>& v, Tail&&... tail)
{
	auto extra = detail::sum_of_sizes(std::forward<Tail>(tail)...);
	auto current = v.size();
	auto required = current + extra;
	if (v.capacity() < required) {
		v.reserve(current + std::max(current, extra));
	}
	detail::append(v, std::forward<Tail>(tail)...);
}

// If both source and destination are vectors of the same type and the
// destination is empty and the source is an rvalue, then move the whole vector
// at once instead of moving element by element.
template<typename T>
void append(std::vector<T>& v, std::vector<T>&& range)
{
	if (v.empty()) {
		v = std::move(range);
	} else {
		v.insert(std::end(v),
		         std::move_iterator(std::begin(range)),
		         std::move_iterator(std::end(range)));
	}
}

template<typename T>
void append(std::vector<T>& x, std::initializer_list<T> list)
{
	x.insert(x.end(), list);
}


template<typename T = void, typename Range, typename... Tail>
[[nodiscard]] auto concat(const Range& range, Tail&&... tail)
{
	using T2 = detail::ToVectorType<T, decltype(std::begin(range))>;
	std::vector<T2> result;
	append(result, range, std::forward<Tail>(tail)...);
	return result;
}

template<typename T, typename... Tail>
[[nodiscard]] std::vector<T> concat(std::vector<T>&& v, Tail&&... tail)
{
	append(v, std::forward<Tail>(tail)...);
	return std::move(v);
}


// lookup in std::map
template<typename Key, typename Value>
[[nodiscard]] const Value* lookup(const std::map<Key, Value>& m, const Key& k)
{
	auto it = m.find(k);
	return (it != m.end()) ? &it->second : nullptr;
}

template<typename Key, typename Value>
[[nodiscard]] Value* lookup(std::map<Key, Value>& m, const Key& k)
{
	auto it = m.find(k);
	return (it != m.end()) ? &it->second : nullptr;
}

// will likely become part of future c++ standard
template<class... Ts> struct overloaded : Ts... { using Ts::operator()...; };
// explicit deduction guide (not needed as of C++20)
template<class... Ts> overloaded(Ts...) -> overloaded<Ts...>;

#endif
