// $Id$ 

// Various utility functions and classes.

#ifndef __UTIL_HH__
#define __UTIL_HH__

#include <cstring>

namespace openmsx {

/** Fill a boolean array with a single value.
  * Optimised for byte-sized booleans, but correct for every size.
  */
inline static void fillBool(bool *ptr, bool value, int nr)
{
	if (sizeof(bool) == 1) {
		memset(ptr, value, nr);
	} else {
		while (nr--) *ptr++ = value;
	}
}

} // namespace openmsx
 
#endif // __UTIL_HH__
