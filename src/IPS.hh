// $Id$

#ifndef IPS_HH
#define IPS_HH

#include "openmsx.hh"
#include <string>

namespace openmsx {

class IPS
{
public:
	static void applyPatch(byte* dest, unsigned size,
	                       const std::string& filename);
};

} // namespace openmsx

#endif
