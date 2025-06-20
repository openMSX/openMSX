#ifndef STL_HH
#define STL_HH

#include <algorithm>
#include <cassert>
#include <functional>
#include <iterator>
#include <initializer_list>
#include <map>
#include <memory>
#include <ranges>
#include <utility>
#include <variant>
#include <vector>

// shared between various other classes
struct uninitialized_tag {};

// Predicate that can be called with any number of parameters (of any type) and
// just always returns 'true'. This can be useful as a default parameter value.
struct always_true {
	template<typename ...Args>
	bool operator()(Args&& ...) const {
		return true;
	}
};

/** Check if a range contains a given value, using linear search.
  * Equivalent to 'find(first, last, val) != last', though this algorithm
  * is more convenient to use.
  * Note: we don't need a variant that uses 'find_if' instead of 'find' because
  * STL already has the 'any_of' algorithm.
  */
template<std::ranges::input_range Range, typename VAL, typename Proj = std::identity>
[[nodiscard]] constexpr bool contains(Range&& range, const VAL& val, Proj proj = {})
{
	auto first = std::ranges::begin(range);
	auto last = std::ranges::end(range);
	return std::ranges::find(first, last, val, proj) != last;
}

/** Faster alternative to 'find' when it's guaranteed that the value will be
  * found (if not the behavior is undefined).
  * When asserts are enabled we check whether we really don't move beyond the
  * end of the range. And this check is the only reason why you need to pass
  * the 'last' parameter. Sometimes you see 'find_unguarded' without a 'last'
  * parameter, we could consider providing such an overload as well.
  */
template<std::input_iterator Iterator, std::sentinel_for<Iterator> Sentinel, typename Val, typename Proj = std::identity>
[[nodiscard]] constexpr Iterator find_unguarded(Iterator first, Sentinel last, const Val& val, Proj proj = {})
{
	return find_if_unguarded(first, last,
		[&](const auto& e) { return std::invoke(proj, e) == val; });
}
template<std::ranges::input_range Range, typename Val, typename Proj = std::identity>
[[nodiscard]] constexpr auto find_unguarded(Range&& range, const Val& val, Proj proj = {})
{
	return find_unguarded(std::ranges::begin(range), std::ranges::end(range), val, proj);
}

/** Faster alternative to 'find_if' when it's guaranteed that the predicate
  * will be true for at least one element in the given range.
  * See also 'find_unguarded'.
  */
template<std::input_iterator Iterator, std::sentinel_for<Iterator> Sentinel, std::indirect_unary_predicate<Iterator> Predicate>
[[nodiscard]] constexpr Iterator find_if_unguarded(Iterator first, Sentinel last, Predicate pred)
{
	(void)last;
	while (true) {
		assert(first != last);
		if (pred(*first)) return first;
		++first;
	}
}
template<std::ranges::input_range Range, typename Predicate>
[[nodiscard]] constexpr auto find_if_unguarded(Range&& range, Predicate pred)
{
	return find_if_unguarded(std::ranges::begin(range), std::ranges::end(range), pred);
}

/** Similar to the find(_if)_unguarded functions above, but searches from the
  * back to front.
  * Note that we only need to provide range versions. Because for the iterator
  * versions it is already possible to pass reverse iterators.
  */
template<std::ranges::bidirectional_range Range, typename Val, typename Proj = std::identity>
[[nodiscard]] constexpr auto rfind_unguarded(Range&& range, const Val& val, Proj proj = {})
{
	auto it = find_unguarded(std::ranges::rbegin(range), std::ranges::rend(range), val, proj);
	++it;
	return it.base();
}

template<std::ranges::bidirectional_range Range, typename Predicate>
[[nodiscard]] constexpr auto rfind_if_unguarded(Range&& range, Predicate pred)
{
	auto it = find_if_unguarded(std::ranges::rbegin(range), std::ranges::rend(range), pred);
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
	if (std::to_address(it) != &v.back()) {
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
template<std::forward_iterator ForwardIt, std::sentinel_for<ForwardIt> Sentinel,
         std::output_iterator<std::iter_value_t<ForwardIt>> OutputIt,
         std::indirect_unary_predicate<ForwardIt> Predicate>
[[nodiscard]] std::pair<OutputIt, ForwardIt> partition_copy_remove(
	ForwardIt first, Sentinel last, OutputIt out_true, Predicate p)
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

template<std::ranges::forward_range Range,
         std::output_iterator<std::ranges::range_value_t<Range>> Output,
         std::predicate<std::ranges::range_value_t<Range>> UnaryPredicate>
[[nodiscard]] auto partition_copy_remove(Range&& range, Output out_true, UnaryPredicate p)
{
	return partition_copy_remove(std::ranges::begin(range), std::ranges::end(range), out_true, p);
}


// Like range::transform(), but with equal source and destination.
template<std::ranges::forward_range Range,
         std::invocable<std::ranges::range_value_t<Range>> Operation>
auto transform_in_place(Range&& range, Operation op)
{
	return std::ranges::transform(range, std::ranges::begin(range), op);
}


// Returns (a copy of) the minimum value in [first, last).
// Requires: first != last.
template<std::input_iterator Iterator, std::sentinel_for<Iterator> Sentinel, typename Proj = std::identity>
[[nodiscard]] constexpr auto min_value(Iterator first, Sentinel last, Proj proj = {})
{
	assert(first != last);
	auto result = std::invoke(proj, *first++);
	while (first != last) {
		result = std::min(result, std::invoke(proj, *first++));
	}
	return result;
}

template<std::ranges::input_range Range, typename Proj = std::identity>
[[nodiscard]] constexpr auto min_value(Range&& range, Proj proj = {})
{
	return min_value(std::ranges::begin(range), std::ranges::end(range), proj);
}

// Returns (a copy of) the maximum value in [first, last).
// Requires: first != last.
template<std::input_iterator Iterator, std::sentinel_for<Iterator> Sentinel, typename Proj = std::identity>
[[nodiscard]] constexpr auto max_value(Iterator first, Sentinel last, Proj proj = {})
{
	assert(first != last);
	auto result = std::invoke(proj, *first++);
	while (first != last) {
		result = std::max(result, std::invoke(proj, *first++));
	}
	return result;
}

template<std::ranges::input_range Range, typename Proj = std::identity>
[[nodiscard]] constexpr auto max_value(Range&& range, Proj proj = {})
{
	return max_value(std::ranges::begin(range), std::ranges::end(range), proj);
}


// Returns the sum of the elements in the given range.
// Assumes: elements can be summed via operator+, with a default constructed
// value being the identity-element for this operator.
template<std::ranges::input_range Range, typename Proj = std::identity>
[[nodiscard]] constexpr auto sum(Range&& range, Proj proj = {})
{
	using Iter = decltype(std::begin(range));
	using VT = typename std::iterator_traits<Iter>::value_type;
	using RT = decltype(std::invoke(proj, std::declval<VT>()));

	auto first = std::ranges::begin(range);
	auto last = std::ranges::end(range);
	RT init{};
	while (first != last) {
		init = std::move(init) + std::invoke(proj, *first++);
	}
	return init;
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
//   auto v1 = to_vector(std::views::drop(my_list, 3));
//   auto v2 = to_vector<Base*>(getDerivedPtrs());
template<typename T = void, std::ranges::common_range Range,
         typename V = std::conditional_t<std::is_void_v<T>, std::ranges::range_value_t<Range>, T>>
[[nodiscard]] std::vector<V> to_vector(Range&& range)
{
#if defined __cpp_lib_ranges_to_container
	return std::ranges::to<std::vector<V>>(std::forward<Range>(range));
#elif defined __cpp_lib_containers_ranges
	return {std::from_range, std::forward<Range>(range)};
#else
	// Cross fingers and hope the range's iterators meet Cpp17InputIterator requirements...
	return {std::ranges::begin(range), std::ranges::end(range)};
#endif
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
	// Range can be a std::views::transform
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
//   append(v, std::views::reverse(w));
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
	auto extra = detail::sum_of_sizes(tail...);
	auto current = v.size();
	if (auto required = current + extra;
	    v.capacity() < required) {
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


// Concatenate two std::arrays (at compile time).
template<typename T, size_t X, size_t Y>
constexpr auto concatArray(const std::array<T, X>& x, const std::array<T, Y>& y)
{
	std::array<T, X + Y> result = {};
	// c++20:  std::ranges::copy(x, &result[0]);
	// c++20:  std::ranges::copy(y, &result[X]);
	for (size_t i = 0; i < X; ++i) result[0 + i] = x[i];
	for (size_t i = 0; i < Y; ++i) result[X + i] = y[i];
	return result;
}
// TODO implement in a generic way for any number of arrays
template<typename T, size_t X, size_t Y, size_t Z>
constexpr auto concatArray(const std::array<T, X>& x,
                           const std::array<T, Y>& y,
                           const std::array<T, Z>& z)
{
	std::array<T, X + Y + Z> result = {};
	for (size_t i = 0; i < X; ++i) result[        i] = x[i];
	for (size_t i = 0; i < Y; ++i) result[X     + i] = y[i];
	for (size_t i = 0; i < Z; ++i) result[X + Y + i] = z[i];
	return result;
}


// lookup in std::map
template<typename Key, typename Value, typename Comp, typename Key2>
[[nodiscard]] const Value* lookup(const std::map<Key, Value, Comp>& m, const Key2& k)
{
	auto it = m.find(k);
	return (it != m.end()) ? &it->second : nullptr;
}

template<typename Key, typename Value, typename Comp, typename Key2>
[[nodiscard]] Value* lookup(std::map<Key, Value, Comp>& m, const Key2& k)
{
	auto it = m.find(k);
	return (it != m.end()) ? &it->second : nullptr;
}

// will likely become part of future c++ standard
template<class... Ts> struct overloaded : Ts... { using Ts::operator()...; };
// explicit deduction guide (not needed as of C++20)
template<class... Ts> overloaded(Ts...) -> overloaded<Ts...>;


// --- Utility to retrieve the index for a given type from a std::variant ---

template<typename> struct get_index_tag {};

template<typename T, typename V> struct get_index;

template<typename T, typename... Ts>
struct get_index<T, std::variant<Ts...>>
    : std::integral_constant<size_t, std::variant<get_index_tag<Ts>...>(get_index_tag<T>()).index()> {};


// Utility to initialize an array via a generator function,
// without first default constructing the elements.

template<typename T, typename F, size_t... Is>
[[nodiscard]] static constexpr auto generate_array(F f, std::index_sequence<Is...>)
   -> std::array<T, sizeof...(Is)>
{
    return {{f(Is)...}};
}

template<size_t N, typename F>
[[nodiscard]] static constexpr auto generate_array(F f)
{
    using T = decltype(f(0));
    return generate_array<T>(f, std::make_index_sequence<N>{});
}


// Like std::array, but operator[] takes an enum
template<typename Enum, typename T, size_t S = size_t(-1)>
struct array_with_enum_index {
private:
	[[nodiscard]] static constexpr size_t get_size_helper() {
		if constexpr (requires { Enum::NUM; }) {
			return (S == size_t(-1)) ? static_cast<size_t>(Enum::NUM) : S;
		} else {
			assert(S != -1);
			return S;
		}
	}

public:
	std::array<T, get_size_helper()> storage;

	// Note: explicitly NO constructors, we want aggregate initialization

	[[nodiscard]] constexpr const auto& operator[](Enum e) const { return storage[std::to_underlying(e)]; }
	[[nodiscard]] constexpr auto& operator[](Enum e) { return storage[std::to_underlying(e)]; }

	// This list is incomplete. Add more when needed.
	[[nodiscard]] constexpr auto begin() const { return storage.begin(); }
	[[nodiscard]] constexpr auto begin() { return storage.begin(); }
	[[nodiscard]] constexpr auto end() const { return storage.end(); }
	[[nodiscard]] constexpr auto end() { return storage.end(); }

	[[nodiscard]] constexpr auto empty() const { return storage.empty(); }
	[[nodiscard]] constexpr auto size() const { return storage.size(); }

	[[nodiscard]] constexpr const auto* data() const { return storage.data(); }
	[[nodiscard]] constexpr auto* data() { return storage.data(); }

	[[nodiscard]] friend constexpr auto operator<=>(const array_with_enum_index& x, const array_with_enum_index& y) = default;
};

#endif
