// $Id$

#ifndef MSXMULTIMEMDEVICE_HH
#define MSXMULTIMEMDEVICE_HH

#include "MSXMultiDevice.hh"
#include <vector>
#include <set>

namespace openmsx {

class MSXCPUInterface;

class MSXMultiMemDevice : public MSXMultiDevice
{
public:
	MSXMultiMemDevice(const HardwareConfig& hwConf);
	virtual ~MSXMultiMemDevice();

	bool add(MSXDevice& device, int base, int size);
	void remove(MSXDevice& device, int base, int size);
	bool empty() const;
	void getDevices(std::set<MSXDevice*>& result) const;

	// MSXDevice
	virtual std::string getName() const;
	virtual byte readMem(word address, EmuTime::param time);
	virtual byte peekMem(word address, EmuTime::param time) const;
	virtual void writeMem(word address, byte value, EmuTime::param time);
	virtual const byte* getReadCacheLine(word start) const;
	virtual byte* getWriteCacheLine(word start) const;

private:
	struct Range {
		Range(unsigned base_, unsigned size_, MSXDevice& device_);
		bool operator==(const Range& other) const;

		unsigned base;
		unsigned size;
		MSXDevice* device;
	};

	const Range& searchRange(unsigned address) const;
	MSXDevice* searchDevice(unsigned address) const;

	typedef std::vector<Range> Ranges;
	Ranges ranges;
};

} // namespace openmsx

#endif
