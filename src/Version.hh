// $Id$

#ifndef __VERSION_HH__
#define __VERSION_HH__

#include <string>

namespace openmsx {

class Version {
public:
	// Defined by build system:
	static const bool Version::RELEASE;
	static const std::string VERSION;
	static const std::string CHANGELOG_REVISION;

	// Computed using constants above:
	static const std::string FULL_VERSION;
	static const std::string WINDOW_TITLE;
};

} // namespace openmsx

#endif //__VERSION_HH__
