#ifndef VIEW_HH
#define VIEW_HH

#include <iterator>
#include <ranges>

namespace view {

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
		return [n](auto&& range) {
			return drop_back_view{
			        std::forward<decltype(range)>(range), n};
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
