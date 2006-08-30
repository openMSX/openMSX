// $Id$

#ifndef CHECKED_CAST_HH
#define CHECKED_CAST_HH

/**
 * Based on checked_cast implementation from the book:
 *    C++ Coding Standard
 *    item 93: Avoid using static_cast on pointers
 */

#include <cassert>

template<typename T> struct remove_reference
{
	typedef T type;
};
template<typename T> struct remove_reference<T&>
{
	typedef T type;
};

template<typename TO, typename FROM>
static TO checked_cast(FROM* from)
{
	assert(dynamic_cast<TO>(from) == static_cast<TO>(from));
	return static_cast<TO>(from);
}
template<typename TO, typename FROM>
static TO checked_cast(FROM& from)
{
	typedef typename remove_reference<TO>::type* TO_PTR;
	assert(dynamic_cast<TO_PTR>(&from) == static_cast<TO_PTR>(&from));
	return static_cast<TO>(from);
}

#endif
