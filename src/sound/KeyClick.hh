#ifndef KEYCLICK_HH
#define KEYCLICK_HH

#include "DACSound8U.hh"
#include "EmuTime.hh"

namespace openmsx {

class DeviceConfig;

class KeyClick
{
public:
	explicit KeyClick(const DeviceConfig& config);

	void reset(EmuTime::param time);
	void setClick(bool status, EmuTime::param time);

private:
	DACSound8U dac;
	bool status = false;
};

} // namespace openmsx

#endif
