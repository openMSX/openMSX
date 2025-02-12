#ifndef SETENV_HH
#define SETENV_HH

#include <cstdlib> // already sufficient for Linux

#ifdef _WIN32

// Windows doesn't have setenv(), reimplement (a slightly simplified version)
// in terms of _putenv_s().
static int setenv(const char* name, const char* value, int overwrite)
{
	if (!overwrite && getenv(name)) {
		return 0;
	}
	return _putenv_s(name, value);
}

#endif // _WIN32

#endif
