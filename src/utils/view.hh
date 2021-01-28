#ifndef VIEW_HH
#define VIEW_HH

#include "semiregular.hh"
#include <algorithm>
#include <iterator>
#include <tuple>
#include <type_traits>

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
class Drop
{
public:
	constexpr Drop(Range&& range_, size_t n_)
		: range(std::forward<Range>(range_))
		, n(n_)
	{
	}

	[[nodiscard]] constexpr auto begin() const {
		using Iterator = decltype(std::begin(range));
		return safe_next(std::begin(range), std::end(range), n,
		                 typename std::iterator_traits<Iterator>::iterator_category());
	}

	[[nodiscard]] constexpr auto end() const {
		return std::end(range);
	}

private:
	Range range;
	size_t n;
};


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


template<typename Range>
class Reverse
{
public:
	constexpr explicit Reverse(Range&& range_)
		: range(std::forward<Range>(range_))
	{
	}

	[[nodiscard]] constexpr auto begin()  const { return range.rbegin(); }
	[[nodiscard]] constexpr auto begin()        { return range.rbegin(); }
	[[nodiscard]] constexpr auto end()    const { return range.rend(); }
	[[nodiscard]] constexpr auto end()          { return range.rend(); }
	[[nodiscard]] constexpr auto rbegin() const { return range.begin(); }
	[[nodiscard]] constexpr auto rbegin()       { return range.begin(); }
	[[nodiscard]] constexpr auto rend()   const { return range.end(); }
	[[nodiscard]] constexpr auto rend()         { return range.end(); }

private:
	Range range;
};


template<typename Iterator, typename UnaryOp> class TransformIterator
{
public:
	using return_type       = decltype(std::declval<UnaryOp>()(*std::declval<Iterator>()));
	using value_type        = std::remove_reference_t<return_type>;
	using reference         = value_type;
	using pointer           = value_type*;
	using difference_type   = typename std::iterator_traits<Iterator>::difference_type;
	using iterator_category = typename std::iterator_traits<Iterator>::iterator_category;

public:
	constexpr TransformIterator() = default;

	constexpr TransformIterator(Iterator it_, UnaryOp op_)
		: storage(it_, op_)
	{
	}

	// InputIterator, ForwardIterator

	[[nodiscard]] constexpr return_type operator*() const { return op()(*it()); }

	// pointer operator->() const   not defined

	constexpr TransformIterator& operator++()
	{
		++it();
		return *this;
	}

	constexpr TransformIterator operator++(int)
	{
		auto copy = *this;
		++it();
		return copy;
	}

	[[nodiscard]] constexpr friend bool operator==(const TransformIterator& x, const TransformIterator& y)
	{
		return x.it() == y.it();
	}

	[[nodiscard]] constexpr friend bool operator!=(const TransformIterator& x, const TransformIterator& y)
	{
		return x.it() != y.it();
	}

	// BidirectionalIterator

	constexpr TransformIterator& operator--()
	{
		--it();
		return *this;
	}

	constexpr TransformIterator operator--(int)
	{
		auto copy = *this;
		--it();
		return copy;
	}

	// RandomAccessIterator

	constexpr TransformIterator& operator+=(difference_type n)
	{
		it() += n;
		return *this;
	}

	constexpr TransformIterator& operator-=(difference_type n)
	{
		it() -= n;
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
		return x.it() - y.it();
	}

	[[nodiscard]] constexpr reference operator[](difference_type n)
	{
		return *(*this + n);
	}

	[[nodiscard]] constexpr friend bool operator<(const TransformIterator& x, const TransformIterator& y)
	{
		return x.it() < y.it();
	}
	[[nodiscard]] constexpr friend bool operator<=(const TransformIterator& x, const TransformIterator& y)
	{
		return x.it() <= y.it();
	}
	[[nodiscard]] constexpr friend bool operator>(const TransformIterator& x, const TransformIterator& y)
	{
		return x.it() > y.it();
	}
	[[nodiscard]] constexpr friend bool operator>=(const TransformIterator& x, const TransformIterator& y)
	{
		return x.it() >= y.it();
	}

private:
	std::tuple<Iterator, semiregular_t<UnaryOp>> storage;

	[[nodiscard]] constexpr       Iterator& it()       { return std::get<0>(storage); }
	[[nodiscard]] constexpr const Iterator& it() const { return std::get<0>(storage); }
	[[nodiscard]] constexpr       UnaryOp&  op()       { return std::get<1>(storage); }
	[[nodiscard]] constexpr const UnaryOp&  op() const { return std::get<1>(storage); }
};

template<typename Range, typename UnaryOp> class Transform
{
public:
	constexpr Transform(Range&& range_, UnaryOp op_)
	        : storage(std::forward<Range>(range_), op_)
	{
	}

	[[nodiscard]] constexpr auto begin() const
	{
		return TransformIterator(std::begin(range()), op());
	}
	[[nodiscard]] constexpr auto end() const
	{
		return TransformIterator(std::end(range()), op());
	}
	[[nodiscard]] constexpr auto rbegin() const
	{
		return TransformIterator(std::rbegin(range()), op());
	}
	[[nodiscard]] constexpr auto rend() const
	{
		return TransformIterator(std::rend(range()), op());
	}

	[[nodiscard]] constexpr auto size()  const { return range().size(); }
	[[nodiscard]] constexpr auto empty() const { return range().empty(); }

	[[nodiscard]] constexpr auto front() const { return op()(range().front()); }
	[[nodiscard]] constexpr auto back()  const { return op()(range().back()); }

	[[nodiscard]] constexpr auto operator[](size_t idx) const { return op()(range()[idx]); }

private:
	std::tuple<Range, UnaryOp> storage;

	[[nodiscard]] constexpr const Range& range() const { return std::get<0>(storage); }
	[[nodiscard]] constexpr const UnaryOp& op()  const { return std::get<1>(storage); }
};


} // namespace detail

template<typename Range>
[[nodiscard]] constexpr auto drop(Range&& range, size_t n)
{
	return detail::Drop<Range>(std::forward<Range>(range), n);
}

template<typename Range>
[[nodiscard]] constexpr auto drop_back(Range&& range, size_t n)
{
	return detail::DropBack<Range>(std::forward<Range>(range), n);
}

template<typename Range>
[[nodiscard]] constexpr auto reverse(Range&& range)
{
	return detail::Reverse<Range>(std::forward<Range>(range));
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

} // namespace view

#endif
