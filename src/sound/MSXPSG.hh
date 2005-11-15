// $Id$

#ifndef MSXPSG_HH
#define MSXPSG_HH

#include "MSXDevice.hh"
#include "AY8910Periphery.hh"
#include <memory>

namespace openmsx {

class AY8910;
class CassettePortInterface;
class RenShaTurbo;
class JoystickPort;

class MSXPSG : public MSXDevice, public AY8910Periphery
{
public:
	MSXPSG(MSXMotherBoard& motherBoard, const XMLElement& config,
	       const EmuTime& time);
	virtual ~MSXPSG();

	virtual void reset(const EmuTime& time);
	virtual void powerDown(const EmuTime& time);
	virtual byte readIO(byte port, const EmuTime& time);
	virtual byte peekIO(byte port, const EmuTime& time) const;
	virtual void writeIO(byte port, byte value, const EmuTime& time);

private:
	// AY8910Periphery: port A input, port B output
	virtual byte readA(const EmuTime& time);
	virtual void writeB(byte value, const EmuTime& time);

	std::auto_ptr<AY8910> ay8910;
	std::auto_ptr<JoystickPort> ports[2];
	CassettePortInterface& cassette;
	RenShaTurbo& renShaTurbo;

	int registerLatch;
	int selectedPort;
	byte prev;
	bool keyLayoutBit;
};

} // namespace openmsx

#endif
