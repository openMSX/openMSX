// $Id$

#include "SimpleDebuggable.hh"
#include "MSXDevice.hh"
#include <string>

namespace openmsx {

class RomBlockDebuggable : public SimpleDebuggable
{
public:
	RomBlockDebuggable(const MSXDevice& device, const byte* blockNr_,
	                   unsigned startAddress_, unsigned mappedSize_,
	                   unsigned bankSizeShift_, unsigned debugShift_ = 1)
		: SimpleDebuggable(
			device.getMotherBoard(),
			device.getName() + " romblocks",
		        "Shows for each byte of the mapper which memory block is selected.",
		        0x10000)
		, blockNr(blockNr_), startAddress(startAddress_)
		, mappedSize(mappedSize_), bankSizeShift(bankSizeShift_)
		, debugShift(debugShift_), debugMask(~((1 << debugShift) - 1))
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
