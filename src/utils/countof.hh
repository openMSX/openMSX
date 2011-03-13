// countof(<array>) returns the number of elements in a statically
// allocated array. For example:
//
//    int a[10];
//    f(a, countof(a)); // f will be called with value 10 as 2nd parameter
//
//
// A naive implementation is this:
//
//    #define countof(array) (sizeof(array) / sizeof(array[0]))
//
// This implementation has a problem. The following example compiles fine, but
// it gives the wrong result:
//
//    int a[10];
//    int* p = a;
//    countof(p);    // wrong result when called with pointer argument
//
//
// A better implementation is this:
//
//    template<typename T, unsigned N>
//    unsigned countof(T(&)[N])
//    {
//        return N;
//    }
//
// The above example now results in a compilation error (which is good).
// Though in this implementation, the result of countof() is no longer a
// compile-time constant. So now this example fails to compile:
//
//    int a[10];
//    int b[countof(a)]; // error: array bound is not a constant
//
//
// The implementation below fixes both problems.

#ifndef COUNTOF_HH
#define COUNTOF_HH

// The following line uses a very obscure syntax:
//   It declares a templatized function 'countofHelper'.
//   It has one (unnamed) parameter: a reference to an array T[N].
//   The return type is a reference to an array char[N]
// Note that this function does not need an implementation.
template<typename T, unsigned N> char (&countofHelper(T(&)[N]))[N];

#define countof(array) (sizeof(countofHelper(array)))

#endif
