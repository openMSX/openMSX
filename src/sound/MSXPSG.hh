// $Id$

#ifndef __MSXPSG_HH__
#define __MSXPSG_HH__

#include <memory>
#include "MSXDevice.hh"
#include "AY8910.hh"

namespace openmsx {

class CassettePortInterface;
class JoystickPort;

class MSXPSG : public MSXDevice, public AY8910Interface
{
// MSXDevice
public:
	MSXPSG(const XMLElement& config, const EmuTime& time);
	virtual ~MSXPSG();

	virtual void reset(const EmuTime& time);
	virtual byte readIO(byte port, const EmuTime& time);
	virtual byte peekIO(byte port, const EmuTime& time) const;
	virtual void writeIO(byte port, byte value, const EmuTime& time);

private:
	int registerLatch;
	std::auto_ptr<AY8910> ay8910;

// AY8910Interface
public:
	virtual byte readA(const EmuTime& time);
	virtual byte readB(const EmuTime& time);
	virtual byte peekA(const EmuTime& time) const;
	virtual byte peekB(const EmuTime& time) const;
	virtual void writeA(byte value, const EmuTime& time);
	virtual void writeB(byte value, const EmuTime& time);

private:
	CassettePortInterface& cassette;
	bool keyLayoutBit;

// joystick ports
private:
	int selectedPort;
	std::auto_ptr<JoystickPort> ports[2];
};

} // namespace openmsx
#endif
