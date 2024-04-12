#ifndef STRINGSTORAGE_HH
#define STRINGSTORAGE_HH

#include "ranges.hh"
#include <cstdlib>
#include <memory>
#include <string_view>

/** StringStorage:
 * Acts like a 'const char*', but in addition calls free() when the pointer
 * goes out of scope.
 */
struct FreeStringStorage
{
	void operator()(char* p) const { free(p); }
};
using StringStorage = std::unique_ptr<char, FreeStringStorage>;


/** Allocate a 'StringStorage' large enough for 'size' characters.
  */
inline StringStorage allocate_string_storage(size_t size)
{
	return StringStorage(static_cast<char*>(malloc(size)));
}

/** Allocate memory for and copy a c-string (zero-terminated string).
  * This function is similar to strdup(), except that this function:
  * - takes a 'string_view' instead of a 'const char*' parameter.
  * - returns a 'StringStorage' instead of 'char*'.
  */
inline StringStorage allocate_c_string(std::string_view s)
{
	auto result = allocate_string_storage(s.size() + 1);
	char* p = result.get();
	char* z = ranges::copy(s, p);
	*z = '\0';
	return result;
}

#endif
