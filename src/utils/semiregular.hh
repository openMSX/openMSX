// semiregular_t<T>
//
// Background info:
//   See here for the definition of the SemiRegular concept
//     https://en.cppreference.com/w/cpp/concepts/Semiregular
//   In short: a type is Semiregular iff it is DefaultConstructible and Copyable
//   (and Copyable requires Movable, CopyConstructible, ...)
//
//   Regular (instead of SemiRegular) additionally requires EqualityComparable,
//   but we don't need that here.
//
// The semiregular_t<T> utility, takes a (possibly) non-SemiRegular type T and
// wraps it so that it does become SemiRegular. More specifically
// semiregular_t<T> is default constructible and assignable (but if T is
// move-only, then semiregular_t<T> remains move-only). Internally this works by
// wrapping T in an std::optional<T> (but only when needed).
// 
// This implementation is taken from (and simplified / stripped down):
//   https://github.com/ericniebler/range-v3/blob/master/include/range/v3/utility/semiregular.hpp
//
// Original file header:
//   Range v3 library
//
//    Copyright Eric Niebler 2013-present
//
//    Use, modification and distribution is subject to the
//    Boost Software License, Version 1.0. (See accompanying
//    file LICENSE_1_0.txt or copy at
//    http://www.boost.org/LICENSE_1_0.txt)
//
//   Project home: https://github.com/ericniebler/range-v3
//

#ifndef SEMIREGULAR_HH
#define SEMIREGULAR_HH

#include <cstddef>
#include <optional>
#include <string>
#include <type_traits>
#include <utility>

namespace sreg_impl {

template <typename T> struct semiregular_move_assign : std::optional<T> {
	using std::optional<T>::optional;

	semiregular_move_assign() = default;
	semiregular_move_assign(const semiregular_move_assign&) = default;
	semiregular_move_assign(semiregular_move_assign&&) = default;
	semiregular_move_assign&
	operator=(const semiregular_move_assign&) = default;
	semiregular_move_assign&
	operator=(semiregular_move_assign&& that) noexcept(
	        std::is_nothrow_move_constructible<T>::value)
	{
		this->reset();
		if (that) { this->emplace(std::move(*that)); }
		return *this;
	}
};

template <typename T>
using semiregular_move_layer =
        std::conditional_t<std::is_move_assignable<T>::value,
                           std::optional<T>,
                           semiregular_move_assign<T>>;

template <typename T>
struct semiregular_copy_assign : semiregular_move_layer<T> {
	using semiregular_move_layer<T>::semiregular_move_layer;

	semiregular_copy_assign() = default;
	semiregular_copy_assign(const semiregular_copy_assign&) = default;
	semiregular_copy_assign(semiregular_copy_assign&&) = default;
	semiregular_copy_assign&
	operator=(const semiregular_copy_assign& that) noexcept(
	        std::is_nothrow_copy_constructible<T>::value)
	{
		this->reset();
		if (that) { this->emplace(*that); }
		return *this;
	}
	semiregular_copy_assign& operator=(semiregular_copy_assign&&) = default;
};

template <typename T>
using semiregular_copy_layer =
        std::conditional_t<std::is_copy_assignable<T>::value,
                           std::optional<T>,
                           semiregular_copy_assign<T>>;

template <typename T> struct semiregular : semiregular_copy_layer<T> {
	using semiregular_copy_layer<T>::semiregular_copy_layer;

	semiregular() : semiregular(tag{}, std::is_default_constructible<T>{})
	{
	}

	T& get() & { return **this; }
	T const& get() const& { return **this; }
	T&& get() && { return *std::move(*this); }
	T const&& get() const&& { return *std::move(*this); }

	operator T&() & { return **this; }
	operator const T&() const& { return **this; }
	operator T &&() && { return *std::move(*this); }
	operator const T &&() const&& { return *std::move(*this); }

	template <typename... Args> auto operator()(Args&&... args) &
	{
		return (**this)(static_cast<Args&&>(args)...);
	}
	template <typename... Args> auto operator()(Args&&... args) const&
	{
		return (**this)(static_cast<Args&&>(args)...);
	}
	template <typename... Args> auto operator()(Args&&... args) &&
	{
		return (*std::move(*this))(static_cast<Args&&>(args)...);
	}
	template <typename... Args> auto operator()(Args&&... args) const&&
	{
		return (*std::move(*this))(static_cast<Args&&>(args)...);
	}

private:
	struct tag {};

	semiregular(tag, std::false_type) {}
	semiregular(tag, std::true_type)
	        : semiregular_copy_layer<T>{std::in_place}
	{
	}
};


template <typename T>
struct semiregular<T&> : private std::reference_wrapper<T&> {
	semiregular() = default;

	template <typename Arg, std::enable_if_t<(std::is_constructible<
	                                          std::reference_wrapper<T&>,
	                                          Arg&>::value)>* = nullptr>
	semiregular(std::in_place_t, Arg& arg) : std::reference_wrapper<T&>(arg)
	{
	}

	using std::reference_wrapper<T&>::reference_wrapper;
	using std::reference_wrapper<T&>::get;
	using std::reference_wrapper<T&>::operator T&;
	using std::reference_wrapper<T&>::operator();
};

template <typename T>
struct semiregular<T&&> : private std::reference_wrapper<T&&> {
	semiregular() = default;

	template <typename Arg, std::enable_if_t<(std::is_constructible<
	                                          std::reference_wrapper<T&&>,
	                                          Arg>::value)>* = nullptr>
	semiregular(std::in_place_t, Arg&& arg)
	        : std::reference_wrapper<T&>(static_cast<Arg&&>(arg))
	{
	}

	using std::reference_wrapper<T&&>::reference_wrapper;
	using std::reference_wrapper<T&&>::get;
	using std::reference_wrapper<T&&>::operator T&&;
	using std::reference_wrapper<T&&>::operator();
};

} // namespace sreg_impl

template <typename T>
using semiregular_t =
        std::conditional_t<std::is_default_constructible<T>::value &&
                                   std::is_copy_assignable<T>::value,
                           T, sreg_impl::semiregular<T>>;

#endif
