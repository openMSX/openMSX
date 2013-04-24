#ifndef MSXMIRRORDEVICE_HH
#define MSXMIRRORDEVICE_HH

#include "MSXDevice.hh"

namespace openmsx {

class MSXCPUInterface;

class MSXMirrorDevice : public MSXDevice
{
public:
	explicit MSXMirrorDevice(const DeviceConfig& config);

	virtual byte readMem(word address, EmuTime::param time);
	virtual void writeMem(word address, byte value, EmuTime::param time);
	virtual const byte* getReadCacheLine(word start) const;
	virtual byte* getWriteCacheLine(word start) const;
	virtual byte peekMem(word address, EmuTime::param time) const;

	template<typename Archive>
	void serialize(Archive& ar, unsigned version);

private:
	MSXCPUInterface& interface;
	const unsigned addressHigh;
};

} // namespace openmsx

#endif
