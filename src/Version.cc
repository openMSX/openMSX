// $Id$

#include "Version.hh"

namespace openmsx {

#include "Version.ii"

const std::string Version::FULL_VERSION
	= "openMSX " + Version::VERSION
	+ ( Version::RELEASE ? "" : "-dev" + Version::CHANGELOG_REVISION );

} // namespace openmsx
