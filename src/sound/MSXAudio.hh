#ifndef MSXAUDIO_HH
#define MSXAUDIO_HH

#include "MSXDevice.hh"
#include "Y8950.hh"
#include <memory>
#include <string>

namespace openmsx {

class Y8950Periphery;
class DACSound8U;

class MSXAudio final : public MSXDevice
{
public:
	explicit MSXAudio(const DeviceConfig& config);
	~MSXAudio() override;

	/** Creates a periphery object for this MSXAudio cartridge.
	  * The ownership of the object remains with the MSXAudio instance.
	  */
	[[nodiscard]] Y8950Periphery& createPeriphery(const std::string& soundDeviceName);

	void powerUp(EmuTime::param time) override;
	void reset(EmuTime::param time) override;
	[[nodiscard]] byte readIO(word port, EmuTime::param time) override;
	[[nodiscard]] byte peekIO(word port, EmuTime::param time) const override;
	void writeIO(word port, byte value, EmuTime::param time) override;
	[[nodiscard]] byte readMem(word address, EmuTime::param time) override;
	[[nodiscard]] byte peekMem(word address, EmuTime::param time) const override;
	void writeMem(word address, byte value, EmuTime::param time) override;
	[[nodiscard]] const byte* getReadCacheLine(word start) const override;
	[[nodiscard]] byte* getWriteCacheLine(word start) const override;

	template<typename Archive>
	void serialize(Archive& ar, unsigned version);

private:
	void enableDAC(bool enable, EmuTime::param time);

private:
	std::unique_ptr<Y8950Periphery> periphery; // polymorphic
	Y8950 y8950;
	std::unique_ptr<DACSound8U> dac; // can be nullptr
	byte registerLatch;
	byte dacValue = 0x80;
	bool dacEnabled = false;

	friend class MusicModulePeriphery;
	friend class PanasonicAudioPeriphery;
	friend class ToshibaAudioPeriphery;
};

} // namespace openmsx

#endif
