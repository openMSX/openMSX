#ifndef OUTER_HH
#define OUTER_HH

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


// Similar to the c++ 'offsetof' macro, though reimplemented to avoid compiler
// warnings (gcc would produce a warning about non-standard compliant behavior
// in the example at the top).
// The constant '5' is arbitrary (can be anything except 0), but it's needed
// to avoid the gcc warning.
#define MY_OFFSETOF(type, member) (reinterpret_cast<size_t>(&reinterpret_cast<type*>(5)->member) - 5)


// Adjust the current 'this' pointer (pointer to the inner object) to a
// reference to the outer object. The first parameter is the type of the outer
// object, the second parameter is the name of the 'this' member variable in
// the outer object.
#define OUTER(type, member) *reinterpret_cast<type*>(reinterpret_cast<uintptr_t>(this) - MY_OFFSETOF(type, member));

#endif
