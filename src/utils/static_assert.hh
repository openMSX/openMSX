// $Id$

#ifndef STATIC_ASSERT_HH
#define STATIC_ASSERT_HH

// Static assert macro. For documentation see
//    http://www.boost.org/doc/html/boost_staticassert.html
// Implementation replaced with version from
//    http://www.pixelbeat.org/programming/gcc/static_assert.html
// because the boost version gave compiler warnings with recent gcc versions.
//
// Once we switch to c++11, we should use the builtin 'static_assert' feature
// instead of this macro.

#define STATIC_ASSERT_JOIN(X, Y) STATIC_ASSERT_DO_JOIN(X, Y)
#define STATIC_ASSERT_DO_JOIN(X, Y) X##Y
#define STATIC_ASSERT(B) \
	enum { STATIC_ASSERT_JOIN(assert_line_, __LINE__) = 1 / (!!(B)) }

#endif
