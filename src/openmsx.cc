// $Id$

#include "openmsx.hh"

#ifdef _WIN32
#include <windows.h>
#endif

namespace openmsx {

#ifdef _WIN32

void DebugPrint(const char* output)
{
	OutputDebugStringA(output);
	OutputDebugStringA("\n");
}

#endif

} // namespace openmsx
