#ifndef CHECKED_CAST_HH
#define CHECKED_CAST_HH

/**
 * Based on checked_cast implementation from the book:
 *    C++ Coding Standard
 *    item 93: Avoid using static_cast on pointers
 */

#include <cassert>

/* IMHO this implementation is simpler, but gcc-3.4 and below don't
 * correctly handle overloading with it (ambiguous overload).
 *
 * template<typename T> struct remove_reference
 * {
 *     typedef T type;
 * };
 * template<typename T> struct remove_reference<T&>
 * {
 *     typedef T type;
 * };
 *
 * template<typename TO, typename FROM>
 * static TO checked_cast(FROM* from)
 * {
 *     assert(dynamic_cast<TO>(from) == static_cast<TO>(from));
 *     return static_cast<TO>(from);
 * }
 * template<typename TO, typename FROM>
 * static TO checked_cast(FROM& from)
 * {
 *     typedef typename remove_reference<TO>::type* TO_PTR;
 *     assert(dynamic_cast<TO_PTR>(&from) == static_cast<TO_PTR>(&from));
 *     return static_cast<TO>(from);
 * }
 *
 * Implementation below can only handle const references, need to find a way
 * around that.
 */
template<typename TO, typename FROM> struct checked_cast_impl {};
template<typename TO, typename FROM> struct checked_cast_impl<TO*, FROM>
{
	inline TO* operator()(FROM from) {
		assert(dynamic_cast<TO*>(from) == static_cast<TO*>(from));
		return static_cast<TO*>(from);
	}
};
template<typename TO, typename FROM> struct checked_cast_impl<TO&, FROM>
{
	inline TO& operator()(const FROM& from) {
		assert(dynamic_cast<TO*>(&from) == static_cast<TO*>(&from));
		return static_cast<TO&>(from);
	}
};
template<typename TO, typename FROM>
static inline TO checked_cast(const FROM& from)
{
	checked_cast_impl<TO, FROM> caster;
	return caster(from);
}

#endif
