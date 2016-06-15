#ifndef STL_HH
#define STL_HH

#include <algorithm>
#include <tuple>
#include <utility>
#include <cassert>

// Dereference the two given (pointer-like) parameters and then compare
// them with the less-than operator.
struct LessDeref
{
	template<typename PTR>
	bool operator()(PTR p1, PTR p2) const { return *p1 < *p2; }
};

// C++14 has an std::equal_to functor that is very much like this version.
// TODO remove this version once we switch to C++14.
struct EqualTo
{
       template<typename T1, typename T2>
       bool operator()(const T1& t1, const T2& t2) const { return t1 == t2; }
};

// Heterogeneous version of std::less.
struct LessThan
{
	template<typename T1, typename T2>
	bool operator()(const T1& t1, const T2& t2) const { return t1 < t2; }
};


// Compare the N-th element of two tuples using a custom comparison functor.
// Also provides overloads to compare the N-the element of a tuple with a
// single value (of a compatible type).
// ATM the functor cannot take constructor arguments, possibly refactor this in
// the future.
template<int N, typename CMP> struct CmpTupleElement
{
	template<typename... Args>
	bool operator()(const std::tuple<Args...>& x, const std::tuple<Args...>& y) const {
		return cmp(std::get<N>(x), std::get<N>(y));
	}

	template<typename T, typename... Args>
	bool operator()(const T& x, const std::tuple<Args...>& y) const {
		return cmp(x, std::get<N>(y));
	}

	template<typename T, typename... Args>
	bool operator()(const std::tuple<Args...>& x, const T& y) const {
		return cmp(std::get<N>(x), y);
	}

	template<typename T1, typename T2>
	bool operator()(const std::pair<T1, T2>& x, const std::pair<T1, T2>& y) const {
		return cmp(std::get<N>(x), std::get<N>(y));
	}

	template<typename T, typename T1, typename T2>
	bool operator()(const T& x, const std::pair<T1, T2>& y) const {
		return cmp(x, std::get<N>(y));
	}

	template<typename T, typename T1, typename T2>
	bool operator()(const std::pair<T1, T2>& x, const T& y) const {
		return cmp(std::get<N>(x), y);
	}

private:
	CMP cmp;
};

// Similar to CmpTupleElement above, but uses the less-than operator.
template<int N> using LessTupleElement = CmpTupleElement<N, LessThan>;


// Check whether the N-the element of a tuple is equal to the given value.
template<int N, typename T> struct EqualTupleValueImpl
{
	explicit EqualTupleValueImpl(const T& t_) : t(t_) {}
	template<typename TUPLE>
	bool operator()(const TUPLE& tup) const {
		return std::get<N>(tup) == t;
	}
private:
	const T& t;
};
template<int N, typename T>
EqualTupleValueImpl<N, T> EqualTupleValue(const T& t) {
	return EqualTupleValueImpl<N, T>(t);
}


/** Check if a range contains a given value, using linear search.
  * Equivalent to 'find(first, last, val) != last', though this algorithm
  * is more convenient to use.
  * Note: we don't need a variant that uses 'find_if' instead of 'find' because
  * STL already has the 'any_of' algorithm.
  */
template<typename ITER, typename VAL>
inline bool contains(ITER first, ITER last, const VAL& val)
{
	return std::find(first, last, val) != last;
}
template<typename RANGE, typename VAL>
inline bool contains(const RANGE& range, const VAL& val)
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
template<typename ITER, typename VAL>
inline ITER find_unguarded(ITER first, ITER last, const VAL& val)
{
	(void)last;
	while (1) {
		assert(first != last);
		if (*first == val) return first;
		++first;
	}
}
template<typename RANGE, typename VAL>
inline auto find_unguarded(RANGE& range, const VAL& val)
-> decltype(std::begin(range))
{
	return find_unguarded(std::begin(range), std::end(range), val);
}

/** Faster alternative to 'find_if' when it's guaranteed that the predicate
  * will be true for at least one element in the given range.
  * See also 'find_unguarded'.
  */
template<typename ITER, typename PRED>
inline ITER find_if_unguarded(ITER first, ITER last, PRED pred)
{
	(void)last;
	while (1) {
		assert(first != last);
		if (pred(*first)) return first;
		++first;
	}
}
template<typename RANGE, typename PRED>
inline auto find_if_unguarded(RANGE& range, PRED pred)
-> decltype(std::begin(range))
{
	return find_if_unguarded(std::begin(range), std::end(range), pred);
}

/** Similar to the find(_if)_unguarded functions above, but searches from the
  * back to front.
  * Note that we only need to provide range versions. Because for the iterator
  * versions it is already possible to pass reverse iterators.
  */
template<typename RANGE, typename VAL>
inline auto rfind_unguarded(RANGE& range, const VAL& val)
-> decltype(std::begin(range))
{
	//auto it = find_unguarded(std::rbegin(range), std::rend(range), val); // c++14
	auto it = find_unguarded(range.rbegin(), range.rend(), val);
	++it;
	return it.base();
}

template<typename RANGE, typename PRED>
inline auto rfind_if_unguarded(RANGE& range, PRED pred)
-> decltype(std::begin(range))
{
	auto it = find_if_unguarded(range.rbegin(), range.rend(), pred);
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
std::pair<OutputIt, ForwardIt> partition_copy_remove(
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
	return std::make_pair(out_true, out_false);
}

#endif
