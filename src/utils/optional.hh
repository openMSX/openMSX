// This implements the c++17 std::optional class.
//   https://en.cppreference.com/w/cpp/utility/optional
// Thus remove this code once we switch to a c++17 compiler.
//
// This is a simplified (stripped down) adaptation from: 
//   https://github.com/akrzemi1/Optional/blob/master/optional.hpp
//
// Original file header:
//   Copyright (C) 2011 - 2012 Andrzej Krzemienski.
//
//   Use, modification, and distribution is subject to the Boost Software
//   License, Version 1.0. (See accompanying file LICENSE_1_0.txt or copy at
//   http://www.boost.org/LICENSE_1_0.txt)
//
//   The idea and interface is based on Boost.Optional library
//   authored by Fernando Luis Cacciola Carballal

#ifndef OPTIONAL_HH
#define OPTIONAL_HH

#include <cassert>
#include <functional>
#include <initializer_list>
#include <stdexcept>
#include <type_traits>
#include <utility>

template<typename T> class optional;

constexpr struct in_place_t {
} in_place{};

struct nullopt_t {
	struct init {};
	constexpr explicit nullopt_t(init) {}
};
constexpr nullopt_t nullopt{nullopt_t::init()};

struct bad_optional_access : std::logic_error
{
	explicit bad_optional_access(const char* what_arg)
	        : logic_error{what_arg} {}
};

namespace opt_impl {

// workaround: std utility functions aren't constexpr yet
template<typename T>
inline constexpr T&&
constexpr_forward(std::remove_reference_t<T>& t) noexcept
{
	return static_cast<T&&>(t);
}

template<typename T>
inline constexpr T&&
constexpr_forward(std::remove_reference_t<T>&& t) noexcept
{
	static_assert(!std::is_lvalue_reference<T>::value, "!!");
	return static_cast<T&&>(t);
}

template<typename T>
inline constexpr std::remove_reference_t<T>&&
constexpr_move(T&& t) noexcept
{
	return static_cast<std::remove_reference_t<T>&&>(t);
}

// static_addressof: a constexpr version of addressof
template<typename T> struct has_overloaded_addressof {
	template<typename X> constexpr static bool has_overload(...)
	{
		return false;
	}

	template<typename X, size_t S = sizeof(std::declval<X&>().operator&())>
	constexpr static bool has_overload(bool)
	{
		return true;
	}

	constexpr static bool value = has_overload<T>(true);
};

template<typename T,
         std::enable_if_t<!has_overloaded_addressof<T>::value>* = nullptr>
constexpr T* static_addressof(T& ref)
{
	return &ref;
}

template<typename T,
         std::enable_if_t<has_overloaded_addressof<T>::value>* = nullptr>
T* static_addressof(T& ref)
{
	return std::addressof(ref);
}

// the call to convert<A>(b) has return type A and converts b to type A iff b
// decltype(b) is implicitly convertible to A
template<typename U> constexpr U convert(U v)
{
	return v;
}

namespace swap_ns {
using std::swap;
template<typename T> void adl_swap(T& t, T& u) noexcept(noexcept(swap(t, u)))
{
	swap(t, u);
}
} // namespace swap_ns

constexpr struct trivial_init_t {
} trivial_init{};

template<typename T> union storage_t {
	unsigned char dummy_;
	T value_;

	constexpr storage_t(trivial_init_t) noexcept : dummy_() {}

	template<typename... Args>
	constexpr storage_t(Args&&... args)
	        : value_(constexpr_forward<Args>(args)...)
	{
	}

	~storage_t() {}
};

template<typename T> union constexpr_storage_t {
	unsigned char dummy_;
	T value_;

	constexpr constexpr_storage_t(trivial_init_t) noexcept : dummy_() {}

	template<typename... Args>
	constexpr constexpr_storage_t(Args&&... args)
	        : value_(constexpr_forward<Args>(args)...)
	{
	}

	~constexpr_storage_t() = default;
};

template<typename T> struct optional_base {
	bool init_;
	storage_t<T> storage_;

	constexpr optional_base() noexcept
	        : init_(false), storage_(trivial_init)
	{
	}

	explicit constexpr optional_base(const T& v) : init_(true), storage_(v)
	{
	}

	explicit constexpr optional_base(T&& v)
	        : init_(true), storage_(constexpr_move(v))
	{
	}

	template<typename... Args>
	explicit optional_base(in_place_t, Args&&... args)
	        : init_(true), storage_(constexpr_forward<Args>(args)...)
	{
	}

	template<typename U, typename... Args,
	         std::enable_if_t<std::is_constructible<
	                 T, std::initializer_list<U>>::value>* = nullptr>
	explicit optional_base(in_place_t, std::initializer_list<U> il,
	                       Args&&... args)
	        : init_(true), storage_(il, std::forward<Args>(args)...)
	{
	}

	~optional_base()
	{
		if (init_) storage_.value_.T::~T();
	}
};

template<typename T> struct constexpr_optional_base {
	bool init_;
	constexpr_storage_t<T> storage_;

	constexpr constexpr_optional_base() noexcept
	        : init_(false), storage_(trivial_init)
	{
	}

	explicit constexpr constexpr_optional_base(const T& v)
	        : init_(true), storage_(v)
	{
	}

	explicit constexpr constexpr_optional_base(T&& v)
	        : init_(true), storage_(constexpr_move(v))
	{
	}

	template<typename... Args>
	explicit constexpr constexpr_optional_base(in_place_t, Args&&... args)
	        : init_(true), storage_(constexpr_forward<Args>(args)...)
	{
	}

	template<typename U, typename... Args,
	         std::enable_if_t<std::is_constructible<
	                 T, std::initializer_list<U>>::value>* = nullptr>
	constexpr explicit constexpr_optional_base(in_place_t,
	                                           std::initializer_list<U> il,
	                                           Args&&... args)
	        : init_(true), storage_(il, std::forward<Args>(args)...)
	{
	}

	~constexpr_optional_base() = default;
};

template<typename T>
using OptionalBase = std::conditional_t<
        std::is_trivially_destructible<T>::value,        // if possible
        constexpr_optional_base<std::remove_const_t<T>>, // use base with trivial destructor
        optional_base<std::remove_const_t<T>>>;

} // namespace opt_impl

template<typename T> class optional : private opt_impl::OptionalBase<T>
{
	using Base = opt_impl::OptionalBase<T>;

	static_assert(!std::is_same<std::decay_t<T>, nullopt_t>::value,
	              "bad T");
	static_assert(!std::is_same<std::decay_t<T>, in_place_t>::value,
	              "bad T");

	constexpr bool initialized() const noexcept
	{
		return Base::init_;
	}
	std::remove_const_t<T>* dataptr()
	{
		return std::addressof(Base::storage_.value_);
	}
	constexpr const T* dataptr() const
	{
		return opt_impl::static_addressof(Base::storage_.value_);
	}

	constexpr const T& contained_val() const&
	{
		return Base::storage_.value_;
	}
	constexpr T&& contained_val() &&
	{
		return std::move(Base::storage_.value_);
	}
	constexpr T& contained_val() &
	{
		return Base::storage_.value_;
	}

	void clear() noexcept
	{
		if (initialized()) dataptr()->T::~T();
		Base::init_ = false;
	}

	template<typename... Args>
	void initialize(Args&&... args) noexcept(
	        noexcept(T(std::forward<Args>(args)...)))
	{
		assert(!Base::init_);
		::new (static_cast<void*>(dataptr()))
		        T(std::forward<Args>(args)...);
		Base::init_ = true;
	}

	template<typename U, typename... Args>
	void initialize(std::initializer_list<U> il, Args&&... args) noexcept(
	        noexcept(T(il, std::forward<Args>(args)...)))
	{
		assert(!Base::init_);
		::new (static_cast<void*>(dataptr()))
		        T(il, std::forward<Args>(args)...);
		Base::init_ = true;
	}

public:
	using value_type = T;

	constexpr optional() noexcept : Base() {}
	constexpr optional(nullopt_t) noexcept : Base() {}

	optional(const optional& rhs) : Base()
	{
		if (rhs.initialized()) {
			::new (static_cast<void*>(dataptr())) T(*rhs);
			Base::init_ = true;
		}
	}

	optional(optional&& rhs) noexcept(
	        std::is_nothrow_move_constructible<T>::value)
	        : Base()
	{
		if (rhs.initialized()) {
			::new (static_cast<void*>(dataptr()))
			        T(std::move(*rhs));
			Base::init_ = true;
		}
	}

	constexpr optional(const T& v) : Base(v) {}

	constexpr optional(T&& v) : Base(opt_impl::constexpr_move(v)) {}

	template<typename... Args>
	explicit constexpr optional(in_place_t, Args&&... args)
	        : Base(in_place_t{}, opt_impl::constexpr_forward<Args>(args)...)
	{
	}

	template<typename U, typename... Args,
	         std::enable_if_t<std::is_constructible<
	                 T, std::initializer_list<U>>::value>* = nullptr>
	constexpr explicit optional(in_place_t, std::initializer_list<U> il,
	                            Args&&... args)
	        : Base(in_place_t{}, il,
	               opt_impl::constexpr_forward<Args>(args)...)
	{
	}

	~optional() = default;

	optional& operator=(nullopt_t) noexcept
	{
		clear();
		return *this;
	}

	optional& operator=(const optional& rhs)
	{
		if (initialized() == true && rhs.initialized() == false)
			clear();
		else if (initialized() == false && rhs.initialized() == true)
			initialize(*rhs);
		else if (initialized() == true && rhs.initialized() == true)
			contained_val() = *rhs;
		return *this;
	}

	optional& operator=(optional&& rhs) noexcept(
	        std::is_nothrow_move_assignable<T>::value&&
	                std::is_nothrow_move_constructible<T>::value)
	{
		if (initialized() == true && rhs.initialized() == false)
			clear();
		else if (initialized() == false && rhs.initialized() == true)
			initialize(std::move(*rhs));
		else if (initialized() == true && rhs.initialized() == true)
			contained_val() = std::move(*rhs);
		return *this;
	}

	template<typename U>
	auto operator=(U&& v) -> std::enable_if_t<
	        std::is_same<std::decay_t<U>, T>::value, optional&>
	{
		if (initialized()) {
			contained_val() = std::forward<U>(v);
		} else {
			initialize(std::forward<U>(v));
		}
		return *this;
	}

	template<typename... Args> void emplace(Args&&... args)
	{
		clear();
		initialize(std::forward<Args>(args)...);
	}

	template<typename U, typename... Args>
	void emplace(std::initializer_list<U> il, Args&&... args)
	{
		clear();
		initialize<U, Args...>(il, std::forward<Args>(args)...);
	}

	void swap(optional<T>& rhs) noexcept(
	        std::is_nothrow_move_constructible<T>::value&& noexcept(
	                opt_impl::swap_ns::adl_swap(std::declval<T&>(),
	                                            std::declval<T&>())))
	{
		if (initialized() == true && rhs.initialized() == false) {
			rhs.initialize(std::move(**this));
			clear();
		} else if (initialized() == false &&
		           rhs.initialized() == true) {
			initialize(std::move(*rhs));
			rhs.clear();
		} else if (initialized() == true && rhs.initialized() == true) {
			using std::swap;
			swap(**this, *rhs);
		}
	}

	explicit constexpr operator bool() const noexcept
	{
		return initialized();
	}
	constexpr bool has_value() const noexcept
	{
		return initialized();
	}

	constexpr const T* operator->() const
	{
		assert(initialized());
		return dataptr();
	}

	constexpr T* operator->()
	{
		assert(initialized());
		return dataptr();
	}

	constexpr const T& operator*() const&
	{
		assert(initialized());
		return contained_val();
	}

	constexpr T& operator*() &
	{
		assert(initialized());
		return contained_val();
	}

	constexpr T&& operator*() &&
	{
		assert(initialized());
		return opt_impl::constexpr_move(contained_val());
	}

	constexpr const T& value() const&
	{
		return initialized() ? contained_val()
		                     : (throw bad_optional_access(
		                                "bad optional access"),
		                        contained_val());
	}

	constexpr T& value() &
	{
		return initialized() ? contained_val()
		                     : (throw bad_optional_access(
		                                "bad optional access"),
		                        contained_val());
	}

	constexpr T&& value() &&
	{
		if (!initialized()) {
			throw bad_optional_access("bad optional access");
		}
		return std::move(contained_val());
	}

	template<typename V> constexpr T value_or(V&& v) const&
	{
		return *this ? **this
		             : opt_impl::convert<T>(
		                       opt_impl::constexpr_forward<V>(v));
	}

	template<typename V> constexpr T value_or(V&& v) &&
	{
		return *this ? opt_impl::constexpr_move(
		                       const_cast<optional<T>&>(*this)
		                               .contained_val())
		             : opt_impl::convert<T>(
		                       opt_impl::constexpr_forward<V>(v));
	}

	void reset() noexcept { clear(); }
};

template<typename T> class optional<T&>
{
	static_assert(sizeof(T) == 0, "optional references disallowed");
};

// Relational operators
template<typename T>
constexpr bool operator==(const optional<T>& x, const optional<T>& y)
{
	return bool(x) != bool(y) ? false : bool(x) == false ? true : *x == *y;
}

template<typename T>
constexpr bool operator!=(const optional<T>& x, const optional<T>& y)
{
	return !(x == y);
}

template<typename T>
constexpr bool operator<(const optional<T>& x, const optional<T>& y)
{
	return (!y) ? false : (!x) ? true : *x < *y;
}

template<typename T>
constexpr bool operator>(const optional<T>& x, const optional<T>& y)
{
	return (y < x);
}

template<typename T>
constexpr bool operator<=(const optional<T>& x, const optional<T>& y)
{
	return !(y < x);
}

template<typename T>
constexpr bool operator>=(const optional<T>& x, const optional<T>& y)
{
	return !(x < y);
}

// Comparison with nullopt
template<typename T>
constexpr bool operator==(const optional<T>& x, nullopt_t) noexcept
{
	return (!x);
}

template<typename T>
constexpr bool operator==(nullopt_t, const optional<T>& x) noexcept
{
	return (!x);
}

template<typename T>
constexpr bool operator!=(const optional<T>& x, nullopt_t) noexcept
{
	return bool(x);
}

template<typename T>
constexpr bool operator!=(nullopt_t, const optional<T>& x) noexcept
{
	return bool(x);
}

template<typename T>
constexpr bool operator<(const optional<T>&, nullopt_t) noexcept
{
	return false;
}

template<typename T>
constexpr bool operator<(nullopt_t, const optional<T>& x) noexcept
{
	return bool(x);
}

template<typename T>
constexpr bool operator<=(const optional<T>& x, nullopt_t) noexcept
{
	return (!x);
}

template<typename T>
constexpr bool operator<=(nullopt_t, const optional<T>&) noexcept
{
	return true;
}

template<typename T>
constexpr bool operator>(const optional<T>& x, nullopt_t) noexcept
{
	return bool(x);
}

template<typename T>
constexpr bool operator>(nullopt_t, const optional<T>&) noexcept
{
	return false;
}

template<typename T>
constexpr bool operator>=(const optional<T>&, nullopt_t) noexcept
{
	return true;
}

template<typename T>
constexpr bool operator>=(nullopt_t, const optional<T>& x) noexcept
{
	return (!x);
}

// Comparison with T
template<typename T> constexpr bool operator==(const optional<T>& x, const T& v)
{
	return bool(x) ? *x == v : false;
}

template<typename T> constexpr bool operator==(const T& v, const optional<T>& x)
{
	return bool(x) ? v == *x : false;
}

template<typename T> constexpr bool operator!=(const optional<T>& x, const T& v)
{
	return bool(x) ? *x != v : true;
}

template<typename T> constexpr bool operator!=(const T& v, const optional<T>& x)
{
	return bool(x) ? v != *x : true;
}

template<typename T> constexpr bool operator<(const optional<T>& x, const T& v)
{
	return bool(x) ? *x < v : true;
}

template<typename T> constexpr bool operator>(const T& v, const optional<T>& x)
{
	return bool(x) ? v > *x : true;
}

template<typename T> constexpr bool operator>(const optional<T>& x, const T& v)
{
	return bool(x) ? *x > v : false;
}

template<typename T> constexpr bool operator<(const T& v, const optional<T>& x)
{
	return bool(x) ? v < *x : false;
}

template<typename T> constexpr bool operator>=(const optional<T>& x, const T& v)
{
	return bool(x) ? *x >= v : false;
}

template<typename T> constexpr bool operator<=(const T& v, const optional<T>& x)
{
	return bool(x) ? v <= *x : false;
}

template<typename T> constexpr bool operator<=(const optional<T>& x, const T& v)
{
	return bool(x) ? *x <= v : true;
}

template<typename T> constexpr bool operator>=(const T& v, const optional<T>& x)
{
	return bool(x) ? v >= *x : true;
}

// Comparison of optional<T&> with T
template<typename T>
constexpr bool operator==(const optional<T&>& x, const T& v)
{
	return bool(x) ? *x == v : false;
}

template<typename T>
constexpr bool operator==(const T& v, const optional<T&>& x)
{
	return bool(x) ? v == *x : false;
}

template<typename T>
constexpr bool operator!=(const optional<T&>& x, const T& v)
{
	return bool(x) ? *x != v : true;
}

template<typename T>
constexpr bool operator!=(const T& v, const optional<T&>& x)
{
	return bool(x) ? v != *x : true;
}

template<typename T> constexpr bool operator<(const optional<T&>& x, const T& v)
{
	return bool(x) ? *x < v : true;
}

template<typename T> constexpr bool operator>(const T& v, const optional<T&>& x)
{
	return bool(x) ? v > *x : true;
}

template<typename T> constexpr bool operator>(const optional<T&>& x, const T& v)
{
	return bool(x) ? *x > v : false;
}

template<typename T> constexpr bool operator<(const T& v, const optional<T&>& x)
{
	return bool(x) ? v < *x : false;
}

template<typename T>
constexpr bool operator>=(const optional<T&>& x, const T& v)
{
	return bool(x) ? *x >= v : false;
}

template<typename T>
constexpr bool operator<=(const T& v, const optional<T&>& x)
{
	return bool(x) ? v <= *x : false;
}

template<typename T>
constexpr bool operator<=(const optional<T&>& x, const T& v)
{
	return bool(x) ? *x <= v : true;
}

template<typename T>
constexpr bool operator>=(const T& v, const optional<T&>& x)
{
	return bool(x) ? v >= *x : true;
}

// Comparison of optional<const T&> with T
template<typename T>
constexpr bool operator==(const optional<const T&>& x, const T& v)
{
	return bool(x) ? *x == v : false;
}

template<typename T>
constexpr bool operator==(const T& v, const optional<const T&>& x)
{
	return bool(x) ? v == *x : false;
}

template<typename T>
constexpr bool operator!=(const optional<const T&>& x, const T& v)
{
	return bool(x) ? *x != v : true;
}

template<typename T>
constexpr bool operator!=(const T& v, const optional<const T&>& x)
{
	return bool(x) ? v != *x : true;
}

template<typename T>
constexpr bool operator<(const optional<const T&>& x, const T& v)
{
	return bool(x) ? *x < v : true;
}

template<typename T>
constexpr bool operator>(const T& v, const optional<const T&>& x)
{
	return bool(x) ? v > *x : true;
}

template<typename T>
constexpr bool operator>(const optional<const T&>& x, const T& v)
{
	return bool(x) ? *x > v : false;
}

template<typename T>
constexpr bool operator<(const T& v, const optional<const T&>& x)
{
	return bool(x) ? v < *x : false;
}

template<typename T>
constexpr bool operator>=(const optional<const T&>& x, const T& v)
{
	return bool(x) ? *x >= v : false;
}

template<typename T>
constexpr bool operator<=(const T& v, const optional<const T&>& x)
{
	return bool(x) ? v <= *x : false;
}

template<typename T>
constexpr bool operator<=(const optional<const T&>& x, const T& v)
{
	return bool(x) ? *x <= v : true;
}

template<typename T>
constexpr bool operator>=(const T& v, const optional<const T&>& x)
{
	return bool(x) ? v >= *x : true;
}

// Specialized algorithms
template<typename T>
void swap(optional<T>& x, optional<T>& y) noexcept(noexcept(x.swap(y)))
{
	x.swap(y);
}

template<typename T>
constexpr optional<std::decay_t<T>> make_optional(T&& v)
{
	return optional<std::decay_t<T>>(
	        opt_impl::constexpr_forward<T>(v));
}

namespace std {

template<typename T> struct hash<::optional<T>> {
	using result_type = typename hash<T>::result_type;
	using argument_type = ::optional<T>;

	constexpr result_type operator()(const argument_type& arg) const
	{
		return arg ? std::hash<T>{}(*arg) : result_type{};
	}
};

template<typename T> struct hash<::optional<T&>> {
	using result_type = typename hash<T>::result_type;
	using argument_type = ::optional<T&>;

	constexpr result_type operator()(const argument_type& arg) const
	{
		return arg ? std::hash<T>{}(*arg) : result_type{};
	}
};

} // namespace std

#endif
