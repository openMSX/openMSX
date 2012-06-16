// $Id$

#ifndef STATIC_ASSERT_HH
#define STATIC_ASSERT_HH

// Static assert macro. For documentation see
//    http://www.boost.org/doc/html/boost_staticassert.html
// Implementation replaced with version from
//    http://www.pixelbeat.org/programming/gcc/static_assert.html
// because the boost version gave compiler warnings with recent gcc versions.
// ... But this new version gives warnings on vc++. So on vc++ we already use
// the c++11 static_assert feature (supported since vc2010). Gcc also has
// support for static_assert, but only in c++11 mode (and we don't enable that
// mode yet). Gcc has _Static_assert that is also available in non-c++11 mode,
// but only starting from gcc-4.6.
//
// Once we switch to c++11, we should use the builtin 'static_assert' feature
// instead of this macro.

#ifdef _MSC_VER

#define STATIC_ASSERT(B) static_assert(B, "static assert failure")

#else

#define STATIC_ASSERT_JOIN(X, Y) STATIC_ASSERT_DO_JOIN(X, Y)
#define STATIC_ASSERT_DO_JOIN(X, Y) X##Y
#define STATIC_ASSERT(B) \
	enum { STATIC_ASSERT_JOIN(assert_line_, __LINE__) = 1 / (!!(B)) }

#endif

#endif
