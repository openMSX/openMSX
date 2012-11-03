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
	explicit MSXTurboRPCM(const DeviceConfig& config);
	virtual ~MSXTurboRPCM();

	virtual void reset(EmuTime::param time);
	virtual byte readIO(word port, EmuTime::param time);
	virtual byte peekIO(word port, EmuTime::param time) const;
	virtual void writeIO(word port, byte value, EmuTime::param time);

	template<typename Archive>
	void serialize(Archive& ar, unsigned version);

private:
	byte getSample(EmuTime::param time) const;
	bool getComp(EmuTime::param time) const;
	void hardwareMute(bool mute);

	MSXMixer& mixer;
	const std::unique_ptr<AudioInputConnector> connector;
	const std::unique_ptr<DACSound8U> dac;
	Clock<15750> reference;
	byte DValue;
	byte status;
	byte hold;
	bool hwMute;
};

} // namespace openmsx

#endif
