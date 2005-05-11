// $Id$

#ifndef MSXTURBORPCM_HH
#define MSXTURBORPCM_HH

#include <memory>
#include "MSXDevice.hh"
#include "Clock.hh"
#include "AudioInputConnector.hh"

namespace openmsx {

class DACSound8U;

class MSXTurboRPCM : public MSXDevice, private AudioInputConnector
{
public:
	MSXTurboRPCM(const XMLElement& config, const EmuTime& time);
	virtual ~MSXTurboRPCM();

	virtual void reset(const EmuTime& time);
	virtual byte readIO(byte port, const EmuTime& time);
	virtual byte peekIO(byte port, const EmuTime& time) const;
	virtual void writeIO(byte port, byte value, const EmuTime& time);

private:
	byte getSample(const EmuTime& time) const;
	bool getComp(const EmuTime& time) const;

	Clock<15750> reference;
	byte DValue;
	byte status;
	byte hold;
	std::auto_ptr<DACSound8U> dac;
};

} // namespace openmsx

#endif
