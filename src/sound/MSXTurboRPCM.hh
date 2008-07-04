// $Id$

#ifndef MSXTURBORPCM_HH
#define MSXTURBORPCM_HH

#include "MSXDevice.hh"
#include "Clock.hh"
#include <memory>

namespace openmsx {

class MSXMixer;
class AudioInputConnector;
class DACSound8U;

class MSXTurboRPCM : public MSXDevice
{
public:
	MSXTurboRPCM(MSXMotherBoard& motherBoard, const XMLElement& config);
	virtual ~MSXTurboRPCM();

	virtual void reset(const EmuTime& time);
	virtual byte readIO(word port, const EmuTime& time);
	virtual byte peekIO(word port, const EmuTime& time) const;
	virtual void writeIO(word port, byte value, const EmuTime& time);

	template<typename Archive>
	void serialize(Archive& ar, unsigned version);

private:
	byte getSample(const EmuTime& time) const;
	bool getComp(const EmuTime& time) const;
	void hardwareMute(bool mute);

	MSXMixer& mixer;
	const std::auto_ptr<AudioInputConnector> connector;
	const std::auto_ptr<DACSound8U> dac;
	Clock<15750> reference;
	byte DValue;
	byte status;
	byte hold;
	bool hwMute;
};

REGISTER_MSXDEVICE(MSXTurboRPCM, "TurboR-PCM");

} // namespace openmsx

#endif
