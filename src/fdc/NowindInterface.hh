#ifndef NOWINDINTERFACE_HH
#define NOWINDINTERFACE_HH

#include "MSXDevice.hh"
#include <bitset>
#include <memory>
#include <vector>
#include <string>

namespace openmsx {

class NowindCommand;
class Rom;
class AmdFlash;
class DiskContainer;
class NowindHost;

class NowindInterface final : public MSXDevice
{
public:
	typedef std::vector<std::unique_ptr<DiskContainer>> Drives;

	explicit NowindInterface(const DeviceConfig& config);
	~NowindInterface();

	void reset(EmuTime::param time) override;
	byte peekMem(word address, EmuTime::param time) const override;
	byte readMem(word address, EmuTime::param time) override;
	void writeMem(word address, byte value, EmuTime::param time) override;
	const byte* getReadCacheLine(word address) const override;
	byte* getWriteCacheLine(word address) const override;

	template<typename Archive>
	void serialize(Archive& ar, unsigned version);

private:
	const std::unique_ptr<Rom> rom;
	const std::unique_ptr<AmdFlash> flash;
	const std::unique_ptr<NowindHost> host;
	std::unique_ptr<NowindCommand> command;
	Drives drives;
	std::string basename;
	byte bank;

	static const unsigned MAX_NOWINDS = 8; // a-h
	typedef std::bitset<MAX_NOWINDS> NowindsInUse;
	std::shared_ptr<NowindsInUse> nowindsInUse;

	friend class NowindCommand;
};

} // namespace openmsx

#endif
