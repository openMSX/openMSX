#include "Version.hh"
#include "strCat.hh"

namespace openmsx {

#include "Version.ii"

std::string Version::full()
{
	std::string result = strCat("openMSX ", VERSION);
	if constexpr (!RELEASE) strAppend(result, '-', REVISION);
	return result;
}

} // namespace openmsx
