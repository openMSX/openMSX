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
	MSXPSG(MSXMotherBoard& motherBoard, const XMLElement& config);
	virtual ~MSXPSG();

	virtual void reset(EmuTime::param time);
	virtual void powerDown(EmuTime::param time);
	virtual byte readIO(word port, EmuTime::param time);
	virtual byte peekIO(word port, EmuTime::param time) const;
	virtual void writeIO(word port, byte value, EmuTime::param time);

	template<typename Archive>
	void serialize(Archive& ar, unsigned version);

private:
	// AY8910Periphery: port A input, port B output
	virtual byte readA(EmuTime::param time);
	virtual void writeB(byte value, EmuTime::param time);

	std::auto_ptr<AY8910> ay8910;
	std::auto_ptr<JoystickPort> ports[2];
	CassettePortInterface& cassette;
	RenShaTurbo& renShaTurbo;

	int registerLatch;
	int selectedPort;
	byte prev;
	const bool keyLayoutBit;
};

} // namespace openmsx

#endif
