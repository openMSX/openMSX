// $Id$

#include "CheckedRam.hh"
#include "Ram.hh"
#include "MSXCPU.hh"
#include "MSXMotherBoard.hh"
#include "StringOp.hh"
#include "likely.hh"
#include <algorithm>
#include <iostream>

namespace openmsx {

static std::bitset<CPU::CACHE_LINE_SIZE> getBitSetAllTrue()
{
	std::bitset<CPU::CACHE_LINE_SIZE> result;
	result.set();
	return result;
}

CheckedRam::CheckedRam(MSXMotherBoard& motherBoard, const std::string& name,
                       const std::string& description, unsigned size)
	: completely_initialized_cacheline(size / CPU::CACHE_LINE_SIZE, false)
	, uninitialized(size / CPU::CACHE_LINE_SIZE, getBitSetAllTrue())
	, ram(new Ram(motherBoard, name, description, size))
	, msxcpu(motherBoard.getCPU())
{
	umrcount = 0;
}

CheckedRam::~CheckedRam()
{
//	std::cout << "(Destructor) UMRs detected: " << umrcount << std::endl;
}

byte CheckedRam::read(unsigned addr)
{
	if (unlikely(uninitialized[addr >> CPU::CACHE_LINE_BITS]
	                          [addr &  CPU::CACHE_LINE_LOW])) {
//		std::cout << "UMR detected, reading from address 0x"
//		          << StringOp::toHexString(addr, 4) << std::endl;
		++umrcount;
	}
	return (*ram)[addr];
}

const byte* CheckedRam::getReadCacheLine(unsigned addr) const
{
	return (completely_initialized_cacheline[addr >> CPU::CACHE_LINE_BITS])
	     ? &(*ram)[addr] : NULL;
}

byte* CheckedRam::getWriteCacheLine(unsigned addr) const
{
	return (completely_initialized_cacheline[addr >> CPU::CACHE_LINE_BITS])
	     ? &(*ram)[addr] : NULL;
}

byte CheckedRam::peek(unsigned addr) const
{
	return (*ram)[addr];
}

void CheckedRam::write(unsigned addr, const byte value)
{
	unsigned line = addr >> CPU::CACHE_LINE_BITS;
	uninitialized[line][addr & CPU::CACHE_LINE_LOW] = false;
	if (unlikely(uninitialized[line].none())) {
		completely_initialized_cacheline[line] = true;
		msxcpu.invalidateMemCache(addr & CPU::CACHE_LINE_HIGH,
		                          CPU::CACHE_LINE_SIZE);
	}
	(*ram)[addr] = value;
}

void CheckedRam::clear()
{
//	std::cout << "UMRs detected: " << umrcount << std::endl;
	umrcount = 0;
	ram->clear();
	fill(uninitialized.begin(), uninitialized.end(), getBitSetAllTrue());
	fill(completely_initialized_cacheline.begin(),
	     completely_initialized_cacheline.end(), false);
}

unsigned CheckedRam::getSize() const
{
	return ram->getSize();
}

Ram& CheckedRam::getUncheckedRam() const
{
	return *ram;
}

} // namespace openmsx
