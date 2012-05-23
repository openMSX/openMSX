// $Id$

#include "SimpleDebuggable.hh"
#include "MSXDevice.hh"
#include <string>

namespace openmsx {

class RomBlockDebuggableBase : public SimpleDebuggable
{
public:
	RomBlockDebuggableBase(const MSXDevice& device)
		: SimpleDebuggable(
			device.getMotherBoard(),
			device.getName() + " romblocks",
		        "Shows for each byte of the mapper which memory block is selected.",
		        0x10000)
	{
	}
};

class RomBlockDebuggable : public RomBlockDebuggableBase
{
public:
	RomBlockDebuggable(const MSXDevice& device, const byte* blockNr_,
	                   unsigned startAddress_, unsigned mappedSize_,
	                   unsigned bankSizeShift_, unsigned debugShift_ = 0)
		: RomBlockDebuggableBase(device)
		, blockNr(blockNr_), startAddress(startAddress_)
		, mappedSize(mappedSize_), bankSizeShift(bankSizeShift_)
		, debugShift(debugShift_), debugMask(~((1 << debugShift) - 1))
	{
	}
	RomBlockDebuggable(const MSXDevice& device, const byte* blockNr_,
	                   unsigned startAddress_, unsigned mappedSize_,
	                   unsigned bankSizeShift_, unsigned debugShift_,
	                   unsigned debugMask_)
		: RomBlockDebuggableBase(device)
		, blockNr(blockNr_), startAddress(startAddress_)
		, mappedSize(mappedSize_), bankSizeShift(bankSizeShift_)
		, debugShift(debugShift_), debugMask(debugMask_)
	{
	}

	virtual byte read(unsigned address)
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
