// $Id$

// Make a class non-inheritable (a final class). When you do inherit from
// a non-inheritable class and you try to instantiate an object of this class,
// you get a compilation error
// 
// Usage: 
//    NON_INHERITABLE_PRE(Foo)
//    class Foo : NON_INHERITABLE(Foo) { ... };
//
// There is a small size penalty when you use this feature. However when
// the NDEBUG macro is defined the checking is disabled (and the penalty
// is gone).


#ifndef __NON_INHERITABLE_HH__
#define __NON_INHERITABLE_HH__


#ifndef NDEBUG

//
// Development version, performs checking at the cost of increasing the object
// size with one pointer (4 bytes).
//

#define NON_INHERITABLE_PRE(T) \
	class NonInheritable##T { \
	private: \
		~NonInheritable##T() {} \
		friend class T; \
	};

#define NON_INHERITABLE_PRE2(T1, T2) \
	template <typename T2> \
	class NonInheritable##T1 { \
	private: \
		~NonInheritable##T1() {} \
		template <typename T> \
		friend class T1; \
	};

#define NON_INHERITABLE(T) virtual private NonInheritable##T

#else

//
// Dummy version, no checking, no overhead.
//

#define NON_INHERITABLE_PRE(T) \
	class NonInheritable##T {};

#define NON_INHERITABLE_PRE2(T1, T2) \
	template <typename T2> \
	class NonInheritable##T1 {};

#define NON_INHERITABLE(T) private NonInheritable##T

#endif // NDEBUG


#endif // __NON_INHERITABLE_HH__
