#ifndef ROMBLOCKDEBUGGABLE_HH
#define ROMBLOCKDEBUGGABLE_HH

#include "MSXDevice.hh"
#include "SimpleDebuggable.hh"

#include "outer.hh"

#include <span>

namespace openmsx {

class RomBlockDebuggableBase
{
public:
	explicit RomBlockDebuggableBase(const MSXDevice& device)
		: debuggable8 (device)
		, debuggable16(device) {}

	// To support larger than 8 bit segment numbers.
	[[nodiscard]] virtual unsigned readExt(unsigned address) = 0;

	struct Debuggable8 final : SimpleDebuggable {
		explicit Debuggable8(const MSXDevice& device)
			: SimpleDebuggable(device.getMotherBoard(),
			                   device.getName() + " romblocks",
			                   "Shows for each byte of the mapper which memory block is selected.",
			                   0x10000) {}

		[[nodiscard]] RomBlockDebuggableBase& getRomBlocks() {
			return OUTER(RomBlockDebuggableBase, debuggable8);
		}

		[[nodiscard]] uint8_t read(unsigned address) override {
			auto& outer = OUTER(RomBlockDebuggableBase, debuggable8);
			return narrow_cast<uint8_t>(outer.readExt(address));
		}
	} debuggable8;

	struct Debuggable16 final : SimpleDebuggable {
		explicit Debuggable16(const MSXDevice& device)
			: SimpleDebuggable(device.getMotherBoard(),
			                   device.getName() + " romblocks16",
			                   "Shows for each byte of the mapper which memory block is selected. "
			                   "16-bit version, bits 0-7 on even / bits 8-15 on odd addresses.",
			                   0x10000) {}

		[[nodiscard]] uint8_t read(unsigned address) override {
			auto& outer = OUTER(RomBlockDebuggableBase, debuggable16);
			auto full = outer.readExt(address);
			return narrow_cast<uint8_t>((address & 1) ? (full >> 8) : (full & 0xff));
		}
	} debuggable16;

protected:
	~RomBlockDebuggableBase() = default;
};

// Generic implementation. This (only) works when the segment numbers are stored
// in some byte-array (so limited to 8-bit segment numbers).
class RomBlockDebuggable final : public RomBlockDebuggableBase
{
public:
	RomBlockDebuggable(const MSXDevice& device, std::span<const uint8_t> blockNr_,
	                   unsigned startAddress_, unsigned mappedSize_,
	                   unsigned bankSizeShift_, unsigned debugShift_ = 0)
		: RomBlockDebuggableBase(device)
		, blockNr(blockNr_.data()), startAddress(startAddress_)
		, mappedSize(mappedSize_), bankSizeShift(bankSizeShift_)
		, debugShift(debugShift_), debugMask(~((1 << debugShift) - 1))
	{
		assert((mappedSize >> bankSizeShift) == blockNr_.size()); // no need to include 'debugMask' here
	}
	RomBlockDebuggable(const MSXDevice& device, std::span<const uint8_t> blockNr_,
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
			uint8_t tmp = blockNr[(addr >> bankSizeShift) & debugMask];
			return (tmp != 255) ? (tmp >> debugShift) : tmp;
		} else {
			return unsigned(-1); // outside mapped address space
		}
	}

private:
	const uint8_t* blockNr;
	const unsigned startAddress;
	const unsigned mappedSize;
	const unsigned bankSizeShift;
	const unsigned debugShift;
	const unsigned debugMask;
};

} // namespace openmsx

#endif
