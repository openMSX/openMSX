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

#endif
