#ifndef NOWINDINTERFACE_HH
#define NOWINDINTERFACE_HH

#include "MSXDevice.hh"
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
	virtual ~NowindInterface();

	virtual void reset(EmuTime::param time);
	virtual byte peek(word address, EmuTime::param time) const;
	virtual byte readMem(word address, EmuTime::param time);
	virtual void writeMem(word address, byte value, EmuTime::param time);
	virtual const byte* getReadCacheLine(word address) const;
	virtual byte* getWriteCacheLine(word address) const;

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

	friend class NowindCommand;
};

} // namespace openmsx

#endif
