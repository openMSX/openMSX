#ifndef NOWINDINTERFACE_HH
#define NOWINDINTERFACE_HH

#include "NowindCommand.hh"
#include "NowindHost.hh"

#include "AmdFlash.hh"
#include "MSXDevice.hh"
#include "Rom.hh"

#include <bitset>
#include <optional>
#include <string>

namespace openmsx {

class NowindInterface final : public MSXDevice
{
public:
	explicit NowindInterface(DeviceConfig& config);
	~NowindInterface() override;

	void reset(EmuTime time) override;
	[[nodiscard]] byte peekMem(uint16_t address, EmuTime time) const override;
	[[nodiscard]] byte readMem(uint16_t address, EmuTime time) override;
	void writeMem(uint16_t address, byte value, EmuTime time) override;
	[[nodiscard]] const byte* getReadCacheLine(uint16_t address) const override;
	[[nodiscard]] byte* getWriteCacheLine(uint16_t address) override;

	template<typename Archive>
	void serialize(Archive& ar, unsigned version);

private:
	Rom rom;
	AmdFlash flash;
	NowindHost::Drives drives;
	NowindHost host;
	std::optional<NowindCommand> command; // because of delayed initialization
	std::string basename;
	byte bank;

	static constexpr unsigned MAX_NOWINDS = 8; // a-h
	using NowindsInUse = std::bitset<MAX_NOWINDS>;
	std::shared_ptr<NowindsInUse> nowindsInUse;

	friend class NowindCommand;
};

} // namespace openmsx

#endif
