// $Id$

#ifndef __SRAM_HH__
#define __SRAM_HH__

#include "Ram.hh"

namespace openmsx {

class Config;

class SRAM : public Ram
{
public:
	SRAM(const string& name, int size,
	     Config* config, const char* header = NULL);
	SRAM(const string& name, const string& description, int size,
	     Config* config, const char* header = NULL);
	virtual ~SRAM();

private:
	void init();
	
	Config* config;
	const char* header;
};

} // namespace openmsx

#endif
