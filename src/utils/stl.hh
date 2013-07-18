#ifndef STL_HH
#define STL_HH

#include <tuple>
#include <utility>

// Dereference the two given (pointer-like) parameters and then compare
// them with the less-than operator.
struct LessDeref
{
	template<typename PTR>
	bool operator()(PTR p1, PTR p2) const { return *p1 < *p2; }
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

#endif
