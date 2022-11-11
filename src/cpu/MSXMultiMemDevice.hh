#ifndef MSXMULTIMEMDEVICE_HH
#define MSXMULTIMEMDEVICE_HH

#include "MSXMultiDevice.hh"
#include <vector>

namespace openmsx {

class MSXMultiMemDevice final : public MSXMultiDevice
{
public:
	explicit MSXMultiMemDevice(const HardwareConfig& hwConf);
	~MSXMultiMemDevice() override;

	[[nodiscard]] bool canAdd(unsigned base, unsigned size);
	void add(MSXDevice& device, unsigned base, unsigned size);
	void remove(MSXDevice& device, unsigned base, unsigned size);
	[[nodiscard]] bool empty() const { return ranges.size() == 1; }
	[[nodiscard]] std::vector<MSXDevice*> getDevices() const;

	// MSXDevice
	[[nodiscard]] const std::string& getName() const override;
	void getNameList(TclObject& result) const override;
	[[nodiscard]] byte readMem(word address, EmuTime::param time) override;
	[[nodiscard]] byte peekMem(word address, EmuTime::param time) const override;
	void writeMem(word address, byte value, EmuTime::param time) override;
	[[nodiscard]] const byte* getReadCacheLine(word start) const override;
	[[nodiscard]] byte* getWriteCacheLine(word start) const override;

private:
	struct Range {
		Range(unsigned base_, unsigned size_, MSXDevice& device_);
		[[nodiscard]] constexpr bool operator==(const Range&) const = default;

		unsigned base;
		unsigned size;
		MSXDevice* device;
	};

	[[nodiscard]] const Range& searchRange(unsigned address) const;
	[[nodiscard]] MSXDevice* searchDevice(unsigned address) const;

	std::vector<Range> ranges; // ordered (sentinel at the back)
};

} // namespace openmsx

#endif
