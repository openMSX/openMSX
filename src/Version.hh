#ifndef VERSION_HH
#define VERSION_HH

#include <string>

namespace openmsx {

class Version
{
public:
	// Defined by build system:
	static const bool RELEASE;
	static const char* const VERSION;
	static const char* const REVISION;
	static const char* const BUILD_FLAVOUR;

	// Computed using constants above:
	static std::string full();

	static const char* const COPYRIGHT;
};

} // namespace openmsx

#endif
