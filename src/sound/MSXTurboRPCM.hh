// $Id$

#ifndef __MSXTURBORPCM_HH__
#define __MSXTURBORPCM_HH__

#include <memory>
#include "MSXIODevice.hh"
#include "EmuTime.hh"
#include "AudioInputConnector.hh"

using std::auto_ptr;

namespace openmsx {

class DACSound8U;

class MSXTurboRPCM : public MSXIODevice, private AudioInputConnector
{
public:
	MSXTurboRPCM(const XMLElement& config, const EmuTime& time);
	virtual ~MSXTurboRPCM();

	virtual void reset(const EmuTime& time);
	virtual byte readIO(byte port, const EmuTime& time);
	virtual void writeIO(byte port, byte value, const EmuTime& time);

private:
	byte getSample(const EmuTime& time);
	bool getComp(const EmuTime& time);

	EmuTimeFreq<15750> reference;
	byte DValue;
	byte status;
	byte hold;
	auto_ptr<DACSound8U> dac;
};

} // namespace openmsx

#endif
