#ifndef NARROW_HH
#define NARROW_HH

#include "inline.hh"
#include <cassert>
#include <type_traits>
#include <utility>

// Inspired by GSL narrow
//     https://github.com/microsoft/GSL/blob/main/include/gsl/narrow
//
// narrow_cast(): a searchable way to do narrowing casts of values
// narrow(): a checked version of narrow_cast(), asserts that the
//           numerical value remains unchanged
//
// The main difference between this version and the GSL version is that we
// assert (we don't want any overhead in non-assert build), while GSL throws an
// exception.

template<typename To, typename From>
constexpr To narrow_cast(From&& from) noexcept
{
	return static_cast<To>(std::forward<From>(from));
}

ALWAYS_INLINE constexpr void narrow_assert(bool ok)
{
	if (!ok) {
#ifdef UNITTEST
		throw "narrow() error";
#else
		assert(false && "narrow() error");
#endif
	}
}

template<typename To, typename From> constexpr To narrow(From from)
#ifndef UNITTEST
	noexcept
#endif
{
	static_assert(std::is_arithmetic_v<From>);
	static_assert(std::is_arithmetic_v<To>);

	const To to = narrow_cast<To>(from);

	narrow_assert(static_cast<From>(to) == from);
	if constexpr (std::is_signed_v<From> != std::is_signed_v<To>) {
		narrow_assert((to < To{}) == (from < From{}));
	}

	return to;
}

#endif
