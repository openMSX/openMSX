// $Id$

#ifndef __MSXPSG_HH__
#define __MSXPSG_HH__

#include <memory>
#include "MSXDevice.hh"
#include "AY8910.hh"

using std::auto_ptr;

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
	virtual void writeIO(byte port, byte value, const EmuTime& time);

private:
	int registerLatch;
	auto_ptr<AY8910> ay8910;

// AY8910Interface
public:
	virtual byte readA(const EmuTime& time);
	virtual byte readB(const EmuTime& time);
	virtual void writeA(byte value, const EmuTime& time);
	virtual void writeB(byte value, const EmuTime& time);

private:
	CassettePortInterface& cassette;
	bool keyLayoutBit;

// joystick ports
private:
	int selectedPort;
	auto_ptr<JoystickPort> ports[2];
};

} // namespace openmsx
#endif
