#ifndef VIEW_HH
#define VIEW_HH

#ifndef _MSC_VER
#include "semiregular.hh"
#endif
#include <algorithm>
#include <cassert>
#include <functional>
#include <iterator>
#include <tuple>
#include <type_traits>
#include <utility>

namespace view {
namespace detail {

template<typename Iterator>
[[nodiscard]] constexpr Iterator safe_next(Iterator first, Iterator last, size_t n, std::input_iterator_tag)
{
	while (n-- && (first != last)) ++first;
	return first;
}

template<typename Iterator>
[[nodiscard]] constexpr Iterator safe_next(Iterator first, Iterator last, size_t n, std::random_access_iterator_tag)
{
	return first + std::min<size_t>(n, last - first);
}

template<typename Iterator>
[[nodiscard]] constexpr Iterator safe_prev(Iterator first, Iterator last, size_t n, std::bidirectional_iterator_tag)
{
	while (n-- && (first != last)) --last;
	return last;
}

template<typename Iterator>
[[nodiscard]] constexpr Iterator safe_prev(Iterator first, Iterator last, size_t n, std::random_access_iterator_tag)
{
	return last - std::min<size_t>(n, last - first);
}


template<typename Range>
class DropBack
{
public:
	constexpr DropBack(Range&& range_, size_t n_)
		: range(std::forward<Range>(range_))
		, n(n_)
	{
	}

	[[nodiscard]] constexpr auto begin() const {
		return std::begin(range);
	}

	[[nodiscard]] constexpr auto end() const {
		using Iterator = decltype(std::begin(range));
		return safe_prev(std::begin(range), std::end(range), n,
		                 typename std::iterator_traits<Iterator>::iterator_category());
	}

private:
	Range range;
	size_t n;
};


template<typename Iterator, typename UnaryOp> class TransformIterator
{
public:
	using return_type       = std::invoke_result_t<UnaryOp, decltype(*std::declval<Iterator>())>;
	using value_type        = std::remove_reference_t<return_type>;
	using reference         = value_type;
	using pointer           = value_type*;
	using difference_type   = typename std::iterator_traits<Iterator>::difference_type;
	using iterator_category = typename std::iterator_traits<Iterator>::iterator_category;

public:
	constexpr TransformIterator() = default;

	constexpr TransformIterator(Iterator it_, UnaryOp op_)
		: it(it_), op(op_)
	{
	}

	// InputIterator, ForwardIterator

	[[nodiscard]] constexpr return_type operator*() const { return std::invoke(op, *it); }

	// pointer operator->() const   not defined

	constexpr TransformIterator& operator++()
	{
		++it;
		return *this;
	}

	constexpr TransformIterator operator++(int)
	{
		auto copy = *this;
		++it;
		return copy;
	}

	[[nodiscard]] constexpr friend bool operator==(const TransformIterator& x, const TransformIterator& y)
	{
		return x.it == y.it;
	}

	// BidirectionalIterator

	constexpr TransformIterator& operator--()
	{
		--it;
		return *this;
	}

	constexpr TransformIterator operator--(int)
	{
		auto copy = *this;
		--it;
		return copy;
	}

	// RandomAccessIterator

	constexpr TransformIterator& operator+=(difference_type n)
	{
		it += n;
		return *this;
	}

	constexpr TransformIterator& operator-=(difference_type n)
	{
		it -= n;
		return *this;
	}

	[[nodiscard]] constexpr friend TransformIterator operator+(TransformIterator x, difference_type n)
	{
		x += n;
		return x;
	}
	[[nodiscard]] constexpr friend TransformIterator operator+(difference_type n, TransformIterator x)
	{
		x += n;
		return x;
	}

	[[nodiscard]] constexpr friend TransformIterator operator-(TransformIterator x, difference_type n)
	{
		x -= n;
		return x;
	}

	[[nodiscard]] constexpr friend difference_type operator-(const TransformIterator& x, const TransformIterator& y)
	{
		return x.it - y.it;
	}

	[[nodiscard]] constexpr reference operator[](difference_type n)
	{
		return *(*this + n);
	}

	[[nodiscard]] constexpr auto operator<=>(const TransformIterator& other) const
	{
		return it <=> other.it;
	}

private:
	Iterator it;

#ifndef _MSC_VER
	// Wrapping in 'semiregular_t' is needed on libc++,
	// see commit e81b34b5ed9f for more details.
	using UO = semiregular_t<UnaryOp>;
#else
	// But it triggers compile errors on visual studio, though only in c++20
	// mode. Hopefully we can remove this workaround in the future when we
	// use std::views::transform().
	using UO = UnaryOp;
#endif
	[[no_unique_address]] UO op;
};

template<typename Range, typename UnaryOp> class Transform
{
public:
	constexpr Transform(Range&& range_, UnaryOp op_)
		: range(std::forward<Range>(range_)), op(op_)
	{
	}

	[[nodiscard]] constexpr auto begin() const
	{
		return TransformIterator(std::begin(range), op);
	}
	[[nodiscard]] constexpr auto end() const
	{
		return TransformIterator(std::end(range), op);
	}
	[[nodiscard]] constexpr auto rbegin() const
	{
		return TransformIterator(std::rbegin(range), op);
	}
	[[nodiscard]] constexpr auto rend() const
	{
		return TransformIterator(std::rend(range), op);
	}

	[[nodiscard]] constexpr auto size()  const { return range.size(); }
	[[nodiscard]] constexpr auto empty() const { return range.empty(); }

	[[nodiscard]] constexpr auto front() const { return op(range.front()); }
	[[nodiscard]] constexpr auto back()  const { return op(range.back()); }

	[[nodiscard]] constexpr auto operator[](size_t idx) const {
		return std::invoke(op, range[idx]);
	}

private:
	Range range;
	[[no_unique_address]] UnaryOp op;
};


template<typename Iterator, typename Sentinel, typename Predicate>
class FilteredIterator
{
public:
	using value_type = typename std::iterator_traits<Iterator>::value_type;
	using reference = typename std::iterator_traits<Iterator>::reference;
	using pointer = typename std::iterator_traits<Iterator>::pointer;
	using difference_type = typename std::iterator_traits<Iterator>::difference_type;
	using iterator_category = std::forward_iterator_tag;

	constexpr FilteredIterator(Iterator it_, Sentinel last_, Predicate pred_)
	        : it(it_), last(last_), pred(pred_)
	{
		while (isFiltered()) ++it;
	}

	[[nodiscard]] constexpr friend bool operator==(const FilteredIterator& x, const FilteredIterator& y)
	{
		return x.it == y.it;
	}

	[[nodiscard]] constexpr reference operator*() const { return *it; }
	[[nodiscard]] constexpr pointer operator->() const { return &*it; }

	constexpr FilteredIterator& operator++()
	{
		do {
			++it;
		} while (isFiltered());
		return *this;
	}
	constexpr FilteredIterator operator++(int)
	{
		FilteredIterator result = *this;
		++(*this);
		return result;
	}

private:
	[[nodiscard]] constexpr bool isFiltered()
	{
		return (it != last) && !std::invoke(pred, *it);
	}

private:
	Iterator it;
	Sentinel last;
	[[no_unique_address]] Predicate pred;
};

template<typename Range, typename Predicate>
class Filter
{
public:
	using Iterator = decltype(std::begin(std::declval<Range>()));
	using Sentinel = decltype(std::end  (std::declval<Range>()));
	using F_Iterator = FilteredIterator<Iterator, Sentinel, Predicate>;

	constexpr Filter(Range&& range_, Predicate pred_)
	        : range(std::forward<Range>(range_)), pred(pred_)
	{
	}

	[[nodiscard]] constexpr F_Iterator begin() const
	{
		return {std::begin(range), std::end(range), pred};
	}
	[[nodiscard]] constexpr F_Iterator end() const
	{
		// TODO should be a 'FilteredSentinel', but that only works well
		// in c++20 (and c++20 already has 'std::views::filter').
		return {std::end(range), std::end(range), pred};
	}

private:
	Range range;
	[[no_unique_address]] Predicate pred;
};


template<typename Iterator, typename Sentinel> class TakeIterator
{
public:
	using value_type        = typename std::iterator_traits<Iterator>::value_type;
	using difference_type   = typename std::iterator_traits<Iterator>::difference_type;

public:
	constexpr TakeIterator(Iterator it_, size_t n_)
		: it(it_), n(difference_type(n_)) {}

	[[nodiscard]] constexpr auto operator*() const { return *it; }

	constexpr TakeIterator& operator++() {
		++it;
		--n;
		return *this;
	}

	constexpr TakeIterator operator++(int) {
		auto copy = *this;
		++it;
		--n;
		return copy;
	}

	[[nodiscard]] constexpr friend bool operator==(const TakeIterator& x, const TakeIterator& y) {
		return (x.it == y.it) || (x.n == 0 && y.n == 0);
	}
	[[nodiscard]] constexpr friend bool operator==(const TakeIterator& x, const Sentinel& y) {
		return (x.it == y) || (x.n == 0);
	}

private:
	Iterator it;
	difference_type n;
};

} // namespace detail

template<typename Range>
[[nodiscard]] constexpr auto drop_back(Range&& range, size_t n)
{
	return detail::DropBack<Range>(std::forward<Range>(range), n);
}

template<typename Range, typename UnaryOp>
[[nodiscard]] constexpr auto transform(Range&& range, UnaryOp op)
{
	return detail::Transform<Range, UnaryOp>(std::forward<Range>(range), op);
}

template<typename Map> [[nodiscard]] constexpr auto keys(Map&& map)
{
	return transform(std::forward<Map>(map),
	                 [](const auto& t) -> auto& { return std::get<0>(t); });
}

template<typename Map> [[nodiscard]] constexpr auto values(Map&& map)
{
	return transform(std::forward<Map>(map),
	                 [](const auto& t) -> auto& { return std::get<1>(t); });
}

template<typename ForwardRange, typename Predicate>
[[nodiscard]] auto filter(ForwardRange&& range, Predicate pred)
{
    return detail::Filter<ForwardRange, Predicate>{std::forward<ForwardRange>(range), pred};
}

} // namespace view

#endif
