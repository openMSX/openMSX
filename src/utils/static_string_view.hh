#ifndef STATIC_STRING_VIEW_HH
#define STATIC_STRING_VIEW_HH

#include "ranges.hh"
#include "StringStorage.hh"
#include <cassert>
#include <string_view>
#include <utility>

/** static_string_view

 * This is like std::string_view, but points to a string with static lifetime
 * (e.g. a string literal). In other words, the pointed-to-string is guaranteed
 * to remain valid. This allows to store just a reference to the string instead
 * of having to copy it into e.g. a std::string object.
 *
 * The intention is that objects of this class can only be constructed from
 * string-literals. For example constructing from std::string, std::string_view
 * or 'const char*' will result in compile errors. (See unittest for more
 * details).
 *
 * In some (rare?) cases you may want something like this, but string literals
 * are too restrictive. Using the 'make_string_storage()' function below might
 * be a solution. But then the programmer takes responsibility of correctly
 * managing lifetimes (then the c++ compiler doesn't help anymore).
 */
class static_string_view
{
public:
	// Disallow construction from non-const char array.
	template<size_t N>
	constexpr static_string_view(char (&buf)[N]) = delete;

	// Construct from string-literal (a const char array).
	template<size_t N>
	constexpr static_string_view(const char (&buf)[N])
		: s(buf, N-1)
	{
		assert(buf[N - 1] == '\0');
	}

	// Used by make_string_storage().
	struct lifetime_ok_tag {};
	constexpr static_string_view(lifetime_ok_tag, std::string_view v)
		: s(v) {}

	// Allow (implicit) conversion to std::string_view.
	explicit(false) constexpr operator std::string_view() const { return s; }

private:
	std::string_view s;
};

/** Take a string_view, make a copy of it, and return a pair of
  *  - Storage for the copy.
  *  - A static_string_view object pointing to that storage.
  * The view only remains valid for as long as the storage is kept alive.
  *
  * Note: using a std::string for the string-storage is not correct when that
  * std::string object can be moved. This implementation is correct even when
  * the storage-handler object gets moved.
  */
inline auto make_string_storage(std::string_view sv)
{
	auto storage = allocate_string_storage(sv.size());
	char* p = storage.get();
	ranges::copy(sv, p);
	return std::pair{std::move(storage),
	                 static_string_view(static_string_view::lifetime_ok_tag{},
			                    std::string_view(p, sv.size()))};
}

#endif
