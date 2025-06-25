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

	void powerUp(EmuTime time) override;
	void reset(EmuTime time) override;
	[[nodiscard]] byte readIO(uint16_t port, EmuTime time) override;
	[[nodiscard]] byte peekIO(uint16_t port, EmuTime time) const override;
	void writeIO(uint16_t port, byte value, EmuTime time) override;
	[[nodiscard]] byte readMem(uint16_t address, EmuTime time) override;
	[[nodiscard]] byte peekMem(uint16_t address, EmuTime time) const override;
	void writeMem(uint16_t address, byte value, EmuTime time) override;
	[[nodiscard]] const byte* getReadCacheLine(uint16_t start) const override;
	[[nodiscard]] byte* getWriteCacheLine(uint16_t start) override;

	template<typename Archive>
	void serialize(Archive& ar, unsigned version);

private:
	void enableDAC(bool enable, EmuTime time);

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
