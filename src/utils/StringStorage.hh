#ifndef STRINGSTORAGE_HH
#define STRINGSTORAGE_HH

#include <cstdlib>
#include <memory>

/** StringStorage:
 * Acts like a 'const char*', but in addition calls free() when the pointer
 * goes out of scope.
 */
struct FreeStringStorage
{
	void operator()(char* p) { free(p); }
};
using StringStorage = std::unique_ptr<char, FreeStringStorage>;


/** Allocate a 'StringStorage' large enough for 'size' characters.
  */
inline StringStorage allocate_string_storage(size_t size)
{
	return StringStorage(static_cast<char*>(malloc(size)));
}

#endif
