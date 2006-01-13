// $Id$

#ifndef MSXMULTIMEMDEVICE_HH
#define MSXMULTIMEMDEVICE_HH

#include "MSXMultiDevice.hh"
#include <vector>

namespace openmsx {

class MSXMultiMemDevice : public MSXMultiDevice
{
public:
	explicit MSXMultiMemDevice(MSXMotherBoard& motherboard);
	virtual ~MSXMultiMemDevice();

	bool add(MSXDevice& device, int base, int size);
	void remove(MSXDevice& device, int base, int size);
	bool empty() const;

	// MSXDevice
	virtual std::string getName() const;
	virtual byte readMem(word address, const EmuTime& time);
	virtual void writeMem(word address, byte value, const EmuTime& time);
	virtual const byte* getReadCacheLine(word start) const;
	virtual byte* getWriteCacheLine(word start) const;
	virtual byte peekMem(word address, const EmuTime& time) const;

private:
	struct Range {
		Range(unsigned base_, unsigned size_, MSXDevice& device_);
		bool operator==(const Range& other) const;

		unsigned base;
		unsigned size;
		MSXDevice* device;
	};

	MSXDevice* searchDevice(unsigned address);
	const MSXDevice* searchDevice(unsigned address) const;

	typedef std::vector<Range> Ranges;
	Ranges ranges;
};

} // namespace openmsx

#endif
