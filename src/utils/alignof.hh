#ifndef ALIGNOF_HH
#define ALIGNOF_HH

// Portable alignof operator
//
// C++11 has a new alignof operator (much like the sizeof operator). Many
// compilers already offered a similar feature in non-c++11 mode as an
// extension (e.g. gcc has the __alignof__ operator). This file implements the
// same functionality for pre-c++11 compilers in a portable way. Once we switch
// to c++11 we can drop this header.
//
// Usage:
//  c++11:  alignof(SomeType)
//  this:   ALIGNOF(SomeType)

template<typename T> struct AlignOf
{
	struct S {
		char c;
		T t;
	};
	static const unsigned value = sizeof(S) - sizeof(T);
};

#define ALIGNOF(T) AlignOf<T>::value


// c++11 offers std::max_align_t, a type whose alignment is at least as great
// as that of any standard type. The malloc() function guarantees to return
// memory with at least this alignment.
//  gcc-4.7, gcc-4.8  : only offer  ::max_align_t but not std::max_align_t
//  gcc-4.9           : offers both ::max_align_t   and   std::max_align_t
//  visual studio 2013: offers only std::max_align_t (as per the c++ standard)
struct MAX_ALIGN_T
{
	long long ll;
	long double ld;
};

#endif
