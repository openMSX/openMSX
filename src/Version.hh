// $Id$

#ifndef VERSION_HH
#define VERSION_HH

#include <string>

namespace openmsx {

class Version {
public:
	// Defined by build system:
	static const bool RELEASE;
	static const std::string VERSION;
	static const std::string CHANGELOG_REVISION;

	// Computed using constants above:
	static const std::string FULL_VERSION;
};

} // namespace openmsx

#endif
