// $Id$

#ifndef KEYCLICK_HH
#define KEYCLICK_HH

#include "noncopyable.hh"
#include <memory>

namespace openmsx {

class MSXMixer;
class XMLElement;
class EmuTime;
class DACSound8U;

class KeyClick : private noncopyable
{
public:
	KeyClick(MSXMixer& mixer, const XMLElement& config);
	~KeyClick();

	void reset(const EmuTime& time);
	void setClick(bool status, const EmuTime& time);

private:
	const std::auto_ptr<DACSound8U> dac;
	bool status;
};

} // namespace openmsx

#endif
