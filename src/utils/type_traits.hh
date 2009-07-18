// $Id$

#ifndef TYPE_TRAITS_HH
#define TYPE_TRAITS_HH

#include "static_assert.hh"
#include <string>

#if __GNUC__ >= 4
// tr1 library was added in gcc-4.0
#include <tr1/type_traits>
#endif


// if_<C, T, F>
template<bool C, class T, class F> struct if_c             : F {};
template<        class T, class F> struct if_c<true, T, F> : T {};
template<typename C, typename T, typename F> struct if_ : if_c<C::value, T, F> {};


// Type-queries

// constants
struct is_true  { static const bool value = true;  };
struct is_false { static const bool value = false; };

// is_primitive<T>
template<typename T> struct is_primitive           : is_false {};
template<> struct is_primitive<bool>               : is_true {};
template<> struct is_primitive<char>               : is_true {};
template<> struct is_primitive<signed char>        : is_true {};
template<> struct is_primitive<signed short>       : is_true {};
template<> struct is_primitive<signed int>         : is_true {};
template<> struct is_primitive<signed long>        : is_true {};
template<> struct is_primitive<unsigned char>      : is_true {};
template<> struct is_primitive<unsigned short>     : is_true {};
template<> struct is_primitive<unsigned int>       : is_true {};
template<> struct is_primitive<unsigned long>      : is_true {};
template<> struct is_primitive<float>              : is_true {};
template<> struct is_primitive<double>             : is_true {};
template<> struct is_primitive<long double>        : is_true {};
template<> struct is_primitive<long long>          : is_true {};
template<> struct is_primitive<unsigned long long> : is_true {};
template<> struct is_primitive<std::string>        : is_true {}; // specific for serialization

// is_floating<T>
template<typename T> struct is_floating    : is_false {};
template<> struct is_floating<float>       : is_true {};
template<> struct is_floating<double>      : is_true {};
template<> struct is_floating<long double> : is_true {};

// is_pointer<T>
template<typename T> struct is_pointer     : is_false {};
template<typename T> struct is_pointer<T*> : is_true {};

// is_polymorphic<T>
template<typename T> struct is_polymorphic
{
	STATIC_ASSERT(!is_primitive<T>::value);
	STATIC_ASSERT(!is_pointer<T>::value);
	struct d1 : public T
	{
		d1();
		char padding[256];
	};
	struct d2 : public T
	{
		d2();
		virtual ~d2() {};
		char padding[256];
	};
	static const bool value = sizeof(d1) == sizeof(d2);
};

// is_abstract<T>
template<typename T> struct is_abstract
{
#if __GNUC__ >= 4
	// The implementation below doesn't work on arm-gcc (for some unknown
	// reason). However the version in the tr1 library does work, so we
	// use that if it's available (gcc started shipping this library from
	// version gcc-4.0).
	// Once we drop support for gcc-3.4 and when VC++ has tr1, we can
	// unconditionally use the tr1 version. In that case we can even use
	// tr1 for all functions in this file, so we can remove this file
	// completely.
	static const bool value = std::tr1::is_abstract<T>::value;
#else
	// T must be a complete type (otherwise result is always false)
	STATIC_ASSERT(sizeof(T) != 0);

	struct yes { char a[1]; };
	struct no  { char a[2]; };
	template<typename U> static no  check(U(*ptr_to_array_of_U)[1]);
	template<typename U> static yes check(...);

	static const bool value = sizeof(check<T>(0)) == sizeof(yes);
#endif
};

// is_same_type<T1, T2>
template<typename T1, typename T2> struct is_same_type         : is_false {};
template<typename T1>              struct is_same_type<T1, T1> : is_true {};

// is_base_and_derived<Base, Derived>
template<typename B, typename D> struct is_base_and_derived
{
	// This only checks if D* can be converted to B*, good enough for now.
	// See boost::is_base_and_derived for a more complete implementation.
	struct yes { char a[1]; };
	struct no  { char a[2]; };
	static yes check(B*);
	static no  check(...);

	static const bool value = sizeof(check(static_cast<D*>(0))) == sizeof(yes);
};


// Type-operations

// remove_const<T>
template<typename T> struct remove_const          { typedef T type; };
template<typename T> struct remove_const<const T> { typedef T type; };

#endif
