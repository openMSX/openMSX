// $Id$

#ifndef SRAM_HH
#define SRAM_HH

#include "Ram.hh"

namespace openmsx {

class MSXMotherBoard;
class XMLElement;

class SRAM : public Ram
{
public:
	SRAM(MSXMotherBoard& motherBoard, const std::string& name, int size,
	     const XMLElement& config, const char* header = NULL);
	SRAM(MSXMotherBoard& motherBoard, const std::string& name,
	     const std::string& description, int size,
	     const XMLElement& config, const char* header = NULL);
	virtual ~SRAM();

private:
	void init();

	const XMLElement& config;
	const char* header;
};

} // namespace openmsx

#endif
