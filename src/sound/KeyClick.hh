// $Id$

#ifndef KEYCLICK_HH
#define KEYCLICK_HH

#include "EmuTime.hh"
#include "noncopyable.hh"
#include <memory>

namespace openmsx {

class DeviceConfig;
class DACSound8U;

class KeyClick : private noncopyable
{
public:
	explicit KeyClick(const DeviceConfig& config);
	~KeyClick();

	void reset(EmuTime::param time);
	void setClick(bool status, EmuTime::param time);

private:
	const std::unique_ptr<DACSound8U> dac;
	bool status;
};

} // namespace openmsx

#endif
