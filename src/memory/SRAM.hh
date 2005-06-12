// $Id$

#ifndef SRAM_HH
#define SRAM_HH

#include "Ram.hh"

namespace openmsx {

class XMLElement;

class SRAM : public Ram
{
public:
	SRAM(const std::string& name, int size,
	     const XMLElement& config, const char* header = NULL);
	SRAM(const std::string& name, const std::string& description, int size,
	     const XMLElement& config, const char* header = NULL);
	virtual ~SRAM();

private:
	void init();

	const XMLElement& config;
	const char* header;
};

} // namespace openmsx

#endif
