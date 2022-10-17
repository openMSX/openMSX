#ifndef ROMBLOCKDEBUGGABLE_HH
#define ROMBLOCKDEBUGGABLE_HH

#include "SimpleDebuggable.hh"
#include "MSXDevice.hh"
#include <span>
#include <string>

namespace openmsx {

class RomBlockDebuggableBase : public SimpleDebuggable
{
public:
	explicit RomBlockDebuggableBase(const MSXDevice& device)
		: SimpleDebuggable(
			device.getMotherBoard(),
			device.getName() + " romblocks",
		        "Shows for each byte of the mapper which memory block is selected.",
		        0x10000)
	{
	}
protected:
	~RomBlockDebuggableBase() = default;
};

class RomBlockDebuggable final : public RomBlockDebuggableBase
{
public:
	RomBlockDebuggable(const MSXDevice& device, std::span<const byte> blockNr_,
	                   unsigned startAddress_, unsigned mappedSize_,
	                   unsigned bankSizeShift_, unsigned debugShift_ = 0)
		: RomBlockDebuggableBase(device)
		, blockNr(blockNr_.data()), startAddress(startAddress_)
		, mappedSize(mappedSize_), bankSizeShift(bankSizeShift_)
		, debugShift(debugShift_), debugMask(~((1 << debugShift) - 1))
	{
		assert((mappedSize >> bankSizeShift) == blockNr_.size());
	}
	RomBlockDebuggable(const MSXDevice& device, std::span<const byte> blockNr_,
	                   unsigned startAddress_, unsigned mappedSize_,
	                   unsigned bankSizeShift_, unsigned debugShift_,
	                   unsigned debugMask_)
		: RomBlockDebuggableBase(device)
		, blockNr(blockNr_.data()), startAddress(startAddress_)
		, mappedSize(mappedSize_), bankSizeShift(bankSizeShift_)
		, debugShift(debugShift_), debugMask(debugMask_)
	{
		assert((mappedSize >> bankSizeShift) == blockNr_.size());
	}

	[[nodiscard]] byte read(unsigned address) override
	{
		unsigned addr = address - startAddress;
		if (addr < mappedSize) {
			byte tmp = blockNr[(addr >> bankSizeShift) & debugMask];
			return (tmp != 255) ? (tmp >> debugShift) : tmp;
		} else {
			return 255; // outside mapped address space
		}
	}

private:
	const byte* blockNr;
	const unsigned startAddress;
	const unsigned mappedSize;
	const unsigned bankSizeShift;
	const unsigned debugShift;
	const unsigned debugMask;
};

} // namespace openmsx

#endif
