#ifndef MSXMULTIMEMDEVICE_HH
#define MSXMULTIMEMDEVICE_HH

#include "MSXMultiDevice.hh"
#include <span>
#include <vector>

namespace openmsx {

class MSXMultiMemDevice final : public MSXMultiDevice
{
public:
	struct Range {
		Range(unsigned base_, unsigned size_, MSXDevice& device_);
		[[nodiscard]] constexpr bool operator==(const Range&) const = default;

		unsigned base;
		unsigned size;
		MSXDevice* device;
	};

public:
	explicit MSXMultiMemDevice(HardwareConfig& hwConf);
	~MSXMultiMemDevice() override;

	[[nodiscard]] bool canAdd(unsigned base, unsigned size);
	void add(MSXDevice& device, unsigned base, unsigned size);
	void remove(MSXDevice& device, unsigned base, unsigned size);
	[[nodiscard]] bool empty() const { return ranges.size() == 1; }
	[[nodiscard]] std::vector<MSXDevice*> getDevices() const;

	/** The address ranges actually in use, in no particular order.
	  * The sentinel is not included. */
	[[nodiscard]] std::span<const Range> getRanges() const {
		return {ranges.data(), ranges.size() - 1};
	}

	// MSXDevice
	[[nodiscard]] const std::string& getName() const override;
	void getNameList(TclObject& result) const override;
	[[nodiscard]] uint8_t readMem(uint16_t address, EmuTime time) override;
	[[nodiscard]] uint8_t peekMem(uint16_t address, EmuTime time) const override;
	void writeMem(uint16_t address, uint8_t value, EmuTime time) override;
	[[nodiscard]] const uint8_t* getReadCacheLine(uint16_t start) const override;
	[[nodiscard]] uint8_t* getWriteCacheLine(uint16_t start) override;

private:
	[[nodiscard]] const Range& searchRange(unsigned address) const;
	[[nodiscard]] MSXDevice* searchDevice(unsigned address) const;

	// In no particular order, except that the sentinel is at the back: it
	// covers the whole address space, so searching front-to-back finds the
	// real ranges first and always finds something.
	std::vector<Range> ranges;
};

} // namespace openmsx

#endif
