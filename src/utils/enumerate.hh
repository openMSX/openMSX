#ifndef ENUMERATE_HH
#define ENUMERATE_HH

/** Heavily inspired by Nathan Reed's blog post:
  * Python-Like enumerate() In C++17
  * http://reedbeta.com/blog/python-like-enumerate-in-cpp17/
  *
  * Can be used to simplify loops like:
  *    for (size_t i = 0; i < std::size(my_array); ++i) {
  *        auto& elem = my_array[i];
  *        ... do stuff with 'i' and 'elem'
  *    }
  *
  * With enumerate() that becomes:
  *    for (auto [i, elem] : enumerate(my_array)) {
  *        ... do stuff with 'i' and 'elem'
  *    }
  */

#include <cstddef>
#include <iterator>
#include <tuple>
#include <utility>

template<typename Iterable,
         typename Iter = decltype(std::begin(std::declval<Iterable>())),
         typename      = decltype(std::end  (std::declval<Iterable>()))>
constexpr auto enumerate(Iterable&& iterable)
{
	struct iterator {
		size_t i;
		Iter iter;

		constexpr bool operator==(const iterator& other) const {
			return iter == other.iter;
		}

		constexpr bool operator==(const Iter& other) const {
			return iter == other;
		}

		constexpr void operator++() {
			++i;
			++iter;
		}
		constexpr auto operator*() const {
			return std::tie(i, *iter);
		}
	};

	struct iterable_wrapper {
		Iterable iterable;

		constexpr auto begin() {
			return iterator{0, std::begin(iterable)};
		}
		constexpr auto end() {
			return std::end(iterable);
		}
	};

	return iterable_wrapper{std::forward<Iterable>(iterable)};
}

#endif
