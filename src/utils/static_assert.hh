// $Id$

#ifndef STATIC_ASSERT_HH
#define STATIC_ASSERT_HH

// Static assert macro
// Based upon Boost Static Assert, see
//    http://www.boost.org/doc/html/boost_staticassert.html
// for documentation and implementation details

template<bool> struct STATIC_ASSERT_FAILURE;
template<>     struct STATIC_ASSERT_FAILURE<true> {};
template<int>  struct STATIC_ASSERT_TEST {};
#define STATIC_ASSERT_JOIN(X, Y) STATIC_ASSERT_DO_JOIN(X, Y)
#define STATIC_ASSERT_DO_JOIN(X, Y) X##Y
#define STATIC_ASSERT(B) \
  typedef STATIC_ASSERT_TEST<sizeof(STATIC_ASSERT_FAILURE<bool(B)>)> \
    STATIC_ASSERT_JOIN(STATIC_ASSERT_, __LINE__)

#endif
