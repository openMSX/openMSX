#ifndef VIEW_HH
#define VIEW_HH

#include <iterator>
#include <ranges>
#include <tuple>

namespace view {
namespace detail {

template<typename... Ts>
std::tuple<decltype(std::begin(std::declval<Ts>()))...>
iterators_tuple_helper(const std::tuple<Ts...>&);

template<typename... Ts>
std::tuple<decltype(*std::begin(std::declval<Ts>()))...>
iterators_deref_tuple_helper(const std::tuple<Ts...>&);

template<bool CHECK_ALL, typename RangesTuple, size_t... Is>
class Zip
{
	RangesTuple ranges;

	using IteratorsTuple     = decltype(iterators_tuple_helper      (std::declval<RangesTuple>()));
	using IteratorDerefTuple = decltype(iterators_deref_tuple_helper(std::declval<RangesTuple>()));

	class Iterator {
	public:
		using iterator_category = std::input_iterator_tag;
		using value_type = IteratorDerefTuple;
		using difference_type = ptrdiff_t;
		using pointer = value_type*;
		using reference = value_type&;

		explicit Iterator(IteratorsTuple&& iterators_) : iterators(std::move(iterators_)) {}

		Iterator& operator++() {
			(++std::get<Is>(iterators), ...);
			return *this;
		}

		Iterator operator++(int) {
			auto r = *this;
			++(*this);
			return r;
		}

		[[nodiscard]] bool operator==(const Iterator& other) const {
			if (CHECK_ALL) {
				return (... || (std::get<Is>(iterators) == std::get<Is>(other.iterators)));
			} else {
				return std::get<0>(iterators) == std::get<0>(other.iterators);
			}
		}

		[[nodiscard]] auto operator*() {
			return value_type((*std::get<Is>(iterators))...);
		}

	private:
		IteratorsTuple iterators;
	};

public:
	Zip(Zip&&) noexcept = default;
	explicit Zip(RangesTuple&& ranges_) : ranges(std::move(ranges_)) {}

	[[nodiscard]] auto begin() const {
		return Iterator{IteratorsTuple(std::begin(std::get<Is>(ranges))...)};
	}
	[[nodiscard]] auto end() const {
		return Iterator{IteratorsTuple(std::end(std::get<Is>(ranges))...)};
	}
};

template<typename RangesTuple, size_t ...Is>
[[nodiscard]] auto zip(RangesTuple&& ranges, std::index_sequence<Is...>){
	return Zip<true, RangesTuple, Is...>{std::forward<RangesTuple>(ranges)};
}

template<typename RangesTuple, size_t ...Is>
[[nodiscard]] auto zip_equal(RangesTuple&& ranges, std::index_sequence<Is...>)
{
	auto size0 = std::size(std::get<0>(ranges)); (void)size0;
	assert((... && (std::size(std::get<Is>(ranges)) == size0)));
	return Zip<false, RangesTuple, Is...>{std::forward<RangesTuple>(ranges)};
}

} // namespace detail

// Similar to c++23 std::ranges::views::zip()
template<typename ...Ranges>
[[nodiscard]] auto zip(Ranges&&... ranges)
{
	return detail::zip(std::tuple<Ranges...>(std::forward<Ranges>(ranges)...),
	                   std::index_sequence_for<Ranges...>());
}

// similar to zip() but with precondition: all ranges must have the size size
template<typename ...Ranges>
[[nodiscard]] auto zip_equal(Ranges&&... ranges)
{
	return detail::zip_equal(std::tuple<Ranges...>(std::forward<Ranges>(ranges)...),
	                         std::index_sequence_for<Ranges...>());
}


template<std::ranges::view V>
	requires std::ranges::sized_range<V>
class drop_back_view
	: public std::ranges::view_interface<drop_back_view<V>>
{
private:
	V base_;
	std::size_t n_;

public:
	drop_back_view() = default;
	constexpr drop_back_view(V base, std::size_t n)
		: base_(std::move(base)), n_(n) {}

	constexpr V base() const& noexcept { return base_; }
	constexpr V base() && { return std::move(base_); }

	constexpr auto begin() { return std::ranges::begin(base_); }

	constexpr auto end() {
		auto size = std::ranges::size(base_);
		if (n_ >= static_cast<std::size_t>(size)) {
			return std::ranges::begin(base_); // skip everything
		}
		return std::ranges::next(std::ranges::begin(base_), size - n_);
	}
};

template<typename R>
drop_back_view(R&&, std::size_t) -> drop_back_view<std::views::all_t<R>>;

struct drop_back_fn /*: public std::ranges::range_adaptor_closure<drop_back_fn>*/
{
	constexpr auto operator()(std::size_t n) const {
		return [n]<typename Range>(Range&& range) {
			return drop_back_view{
			        std::forward<Range>(range), n};
		};
	}

	template<std::ranges::viewable_range R>
	constexpr auto operator()(R&& r, std::size_t n) const {
		return drop_back_view{std::forward<R>(r), n};
	}
};

inline constexpr drop_back_fn drop_back;

} // namespace view

#endif
