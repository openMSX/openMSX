#ifndef MSXMEGARAM_HH
#define MSXMEGARAM_HH

#include "MSXDevice.hh"
#include <memory>

namespace openmsx {

class Ram;
class Rom;
class RomBlockDebuggable;

class MSXMegaRam final : public MSXDevice
{
public:
	explicit MSXMegaRam(const DeviceConfig& config);
	~MSXMegaRam();

	void powerUp(EmuTime::param time) override;
	void reset(EmuTime::param time) override;
	byte readMem(word address, EmuTime::param time) override;
	const byte* getReadCacheLine(word address) const override;
	void writeMem(word address, byte value,
	              EmuTime::param time) override;
	byte* getWriteCacheLine(word address) const override;

	byte readIO(word port, EmuTime::param time) override;
	byte peekIO(word port, EmuTime::param time) const override;
	void writeIO(word port, byte value, EmuTime::param time) override;

	template<typename Archive>
	void serialize(Archive& ar, unsigned version);

private:
	void setBank(byte page, byte block);

	const unsigned numBlocks; // must come before ram
	const std::unique_ptr<Ram> ram;
	const std::unique_ptr<Rom> rom;
	const std::unique_ptr<RomBlockDebuggable> romBlockDebug;
	const byte maskBlocks;
	byte bank[4];
	bool writeMode;
	bool romMode;
};

} // namespace openmsx

#endif
