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

// Note: LessTupleElement and CmpTupleElement can be made a lot more general
// (and uniform). This can be done relatively easily with variadic templates.
// Unfortunately vc++ doesn't support this yet. So for now these classes are
// 'just enough' to make the current users of these utilities happy.

// Compare the N-th element of a tuple using the less-than operator. Also
// provides overloads to compare the N-the element with a single value of the
// same type.
template<int N> struct LessTupleElement
{
	template<typename TUPLE>
	bool operator()(const TUPLE& x, const TUPLE& y) const {
		return std::get<N>(x) < std::get<N>(y);
	}

	template<typename TUPLE>
	bool operator()(const typename std::tuple_element<N, TUPLE>::type& x,
	                const TUPLE& y) const {
		return x < std::get<N>(y);
	}

	template<typename TUPLE>
	bool operator()(const TUPLE& x,
	                const typename std::tuple_element<N, TUPLE>::type& y) const {
		return std::get<N>(x) < y;
	}
};

// Similar to LessTupleElement, but with a custom comparison functor. ATM the
// functor cannot take constructor arguments, possibly refactor this in the
// future.
template<int N, typename CMP> struct CmpTupleElement
{
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


// Check whether the N-the element of a tuple is equal to the given value.
template<int N, typename T> struct EqualTupleValueImpl
{
	EqualTupleValueImpl(const T& t_) : t(t_) {}
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

#endif
