#ifndef MSXMULTIMEMDEVICE_HH
#define MSXMULTIMEMDEVICE_HH

#include "MSXMultiDevice.hh"
#include <vector>

namespace openmsx {

class MSXMultiMemDevice final : public MSXMultiDevice
{
public:
	explicit MSXMultiMemDevice(const HardwareConfig& hwConf);
	~MSXMultiMemDevice();

	bool canAdd(int base, int size);
	void add(MSXDevice& device, int base, int size);
	void remove(MSXDevice& device, int base, int size);
	bool empty() const { return ranges.size() == 1; }
	std::vector<MSXDevice*> getDevices() const;

	// MSXDevice
	std::string getName() const override;
	void getNameList(TclObject& result) const override;
	byte readMem(word address, EmuTime::param time) override;
	byte peekMem(word address, EmuTime::param time) const override;
	void writeMem(word address, byte value, EmuTime::param time) override;
	const byte* getReadCacheLine(word start) const override;
	byte* getWriteCacheLine(word start) const override;

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

	std::vector<Range> ranges; // ordered (sentinel at the back)
};

} // namespace openmsx

#endif
