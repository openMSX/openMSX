// $Id$

#ifndef KEYCLICK_HH
#define KEYCLICK_HH

#include "EmuTime.hh"
#include "noncopyable.hh"
#include <memory>

namespace openmsx {

class MSXMixer;
class XMLElement;
class DACSound8U;

class KeyClick : private noncopyable
{
public:
	KeyClick(MSXMixer& mixer, const XMLElement& config);
	~KeyClick();

	void reset(EmuTime::param time);
	void setClick(bool status, EmuTime::param time);

private:
	const std::auto_ptr<DACSound8U> dac;
	bool status;
};

} // namespace openmsx

#endif
