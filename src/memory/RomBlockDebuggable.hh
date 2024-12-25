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

	// For the 'Debuggable' interface we need to have an 8-bit read(). For most mappers that's sufficient.
	[[nodiscard]] byte read(unsigned address) override {
		return narrow_cast<byte>(readExt(address));
	}

	// To support larger than 8 bit segment numbers.
	[[nodiscard]] virtual unsigned readExt(unsigned address) = 0;

protected:
	~RomBlockDebuggableBase() = default;
};

// Generic implementation. This (only) works when the segment numbers are stored
// in some byte-array (so limited to 8-bit segment numbers).
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
		assert((mappedSize >> bankSizeShift) == blockNr_.size()); // no need to include 'debugMask' here
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
		assert(((mappedSize >> bankSizeShift) & debugMask) < blockNr_.size()); // here we do need 'debugMask'
	}

	[[nodiscard]] unsigned readExt(unsigned address) override
	{
		unsigned addr = address - startAddress;
		if (addr < mappedSize) {
			byte tmp = blockNr[(addr >> bankSizeShift) & debugMask];
			return (tmp != 255) ? (tmp >> debugShift) : tmp;
		} else {
			return unsigned(-1); // outside mapped address space
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
