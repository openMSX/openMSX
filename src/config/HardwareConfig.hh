// $Id$

#ifndef HARDWARECONFIG_HH
#define HARDWARECONFIG_HH

#include "XMLElement.hh"

namespace openmsx {

class HardwareConfig : public XMLElement
{
public:
	static HardwareConfig& instance();

	void loadHardware(XMLElement& root, const std::string& path,
	                  const std::string& hwName);
	std::string makeUnique(const std::string& str);

private:
	HardwareConfig();
	~HardwareConfig();

	std::map<std::string, unsigned> idMap;
};

} // namespace openmsx

#endif
