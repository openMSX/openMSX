#ifndef NOWINDINTERFACE_HH
#define NOWINDINTERFACE_HH

#include "MSXDevice.hh"
#include "NowindHost.hh"
#include "Rom.hh"
#include "AmdFlash.hh"
#include <bitset>
#include <memory>
#include <string>
#include <vector>

namespace openmsx {

class NowindCommand;

class NowindInterface final : public MSXDevice
{
public:
	explicit NowindInterface(const DeviceConfig& config);
	~NowindInterface() override;

	void reset(EmuTime::param time) override;
	[[nodiscard]] byte peekMem(word address, EmuTime::param time) const override;
	[[nodiscard]] byte readMem(word address, EmuTime::param time) override;
	void writeMem(word address, byte value, EmuTime::param time) override;
	[[nodiscard]] const byte* getReadCacheLine(word address) const override;
	[[nodiscard]] byte* getWriteCacheLine(word address) const override;

	template<typename Archive>
	void serialize(Archive& ar, unsigned version);

private:
	Rom rom;
	const std::vector<AmdFlash::SectorInfo> flashConfig;
	AmdFlash flash;
	NowindHost host;
	std::unique_ptr<NowindCommand> command;
	NowindHost::Drives drives;
	std::string basename;
	byte bank;

	static constexpr unsigned MAX_NOWINDS = 8; // a-h
	using NowindsInUse = std::bitset<MAX_NOWINDS>;
	std::shared_ptr<NowindsInUse> nowindsInUse;

	friend class NowindCommand;
};

} // namespace openmsx

#endif
