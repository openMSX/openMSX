// $Id$

#ifndef HARDWARECONFIG_HH
#define HARDWARECONFIG_HH

#include "XMLElement.hh"

namespace openmsx {

class HardwareConfig : public XMLElement
{
public:
	HardwareConfig();

	void loadHardware(XMLElement& root, const std::string& path,
	                  const std::string& hwName);
	std::string makeUnique(const std::string& str);

private:
	std::map<std::string, unsigned> idMap;
};

} // namespace openmsx

#endif
