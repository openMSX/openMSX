#ifndef KEYCLICK_HH
#define KEYCLICK_HH

#include "DACSound8U.hh"
#include "EmuTime.hh"
#include "noncopyable.hh"

namespace openmsx {

class DeviceConfig;

class KeyClick : private noncopyable
{
public:
	explicit KeyClick(const DeviceConfig& config);

	void reset(EmuTime::param time);
	void setClick(bool status, EmuTime::param time);

private:
	DACSound8U dac;
	bool status;
};

} // namespace openmsx

#endif
