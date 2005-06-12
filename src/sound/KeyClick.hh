// $Id$

#ifndef KEYCLICK_HH
#define KEYCLICK_HH

#include <memory>

namespace openmsx {

class XMLElement;
class EmuTime;
class DACSound8U;

class KeyClick
{
public:
	KeyClick(const XMLElement& config, const EmuTime& time);
	virtual ~KeyClick();

	void reset(const EmuTime& time);
	void setClick(bool status, const EmuTime& time);

private:
	const std::auto_ptr<DACSound8U> dac;
	bool status;
};

} // namespace openmsx

#endif
