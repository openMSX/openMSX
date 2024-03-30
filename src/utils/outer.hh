#ifndef OUTER_HH
#define OUTER_HH

#include <bit>
#include <cstddef>

// Example usage:
//   class Foo {
//       ...
//       struct Bar : public SomeBaseClass {
//           ...
//           void f() override {
//               // transform the 'this' pointer to the 'bar' inner object
//               // into a reference to the outer object of type Foo.
//               Foo& foo = OUTER(Foo, bar);
//           }
//       } bar;
//   };
//
// This avoids having to store a back-pointer from the inner 'bar' object to
// the outer 'Foo' object.
//
// The 'bar' object lives at a fixed offset inside the 'Foo' object, so
// changing the bar-this-pointer into a Foo-reference is a matter of
// subtracting this (compile-time-constant) offset.
//
// At least the offset is constant in the absence of virtual inheritance (we
// don't use this c++ feature in openMSX). Also strictly speaking the c++
// standard doesn't guarantee the offset is constant in the presence of virtual
// functions, though it is in all practical implementations. See here for more
// background info:
//   http://stackoverflow.com/questions/1129894/why-cant-you-use-offsetof-on-non-pod-structures-in-c


// Adjust the current 'this' pointer (pointer to the inner object) to a
// reference to the outer object. The first parameter is the type of the outer
// object, the second parameter is the name of the 'this' member variable in
// the outer object.

template<typename T> int checkInvalidOuterUsage(T) { return 0; }

#define OUTER(type, member) (checkInvalidOuterUsage<const decltype(std::declval<type&>().member)*>(this), \
                             *std::bit_cast<type*>(std::bit_cast<uintptr_t>(this) - offsetof(type, member)))

#endif
