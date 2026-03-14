#include "Version.hh"

#include "strCat.hh"

namespace openmsx {

#include "Version.ii"

TemporaryString Version::full()
{
	return tmpStrCat("openMSX ", VERSION, strCat_if(!RELEASE, '-', REVISION));
}

} // namespace openmsx
