// $Id$

#ifndef __KEYCLICK_HH__
#define __KEYCLICK_HH__

#include "openmsx.hh"
#include "DACSound8U.hh"

namespace openmsx {

class EmuTime;

class KeyClick
{
public:
	KeyClick(const XMLElement& config, const EmuTime& time);
	virtual ~KeyClick(); 

	void reset(const EmuTime& time);
	void setClick(bool status, const EmuTime& time);

private:
	DACSound8U dac;
	bool status;
};

} // namespace openmsx
#endif
