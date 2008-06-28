#ifndef REF_HH
#define REF_HH

// This utility is part of the TR1 C++ library.
//
// It's reimplemented here (a simplified version) because the TR1 library
// does not yet ship with all compilers. It seems gcc-4.x is the first gcc
// version that ships wth TR1 by default. So this file can be removed once
// we drop support for gcc-3.4.

template<typename T> class reference_wrapper
{
public:
	explicit reference_wrapper(T& t_) : t(&t_) {}
	operator T&() const { return *t; }
	T& get() const { return *t; }
	T* get_pointer() const { return t; }

private:
	T* t;
};

template<typename T> inline reference_wrapper<T> ref(T& t)
{
	return reference_wrapper<T>(t);
}

template<typename T> inline reference_wrapper<const T> cref(const T& t)
{
	return reference_wrapper<const T>(t);
}

#endif
