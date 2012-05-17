// $Id$

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
class DiskChanger;
class DiskContainer;
class NowindHost;

class NowindInterface : public MSXDevice
{
public:
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
	void deleteDrives();

	const std::auto_ptr<Rom> rom;
	const std::auto_ptr<AmdFlash> flash;
	const std::auto_ptr<NowindHost> host;
	std::auto_ptr<NowindCommand> command;
	typedef std::vector<DiskContainer*> Drives;
	Drives drives;
	std::string basename;
	byte bank;

	friend class NowindCommand;
};

} // namespace openmsx

#endif
