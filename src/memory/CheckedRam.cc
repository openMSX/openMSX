// $Id$

#include "MSXMotherBoard.hh"
#include "MSXCPU.hh"
#include "CheckedRam.hh"
#include "Ram.hh"
#include "likely.hh"
#include <algorithm>
#include <iostream>
#include <iomanip>
#include <bitset>

using std::bitset;

namespace openmsx {

static bitset<CPU::CACHE_LINE_SIZE> getBitSetAllTrue()
{
  bitset<CPU::CACHE_LINE_SIZE> result;
  result.set();
  return result;
}
	
CheckedRam::CheckedRam(MSXMotherBoard& motherBoard, const std::string& name,
         const std::string& description, unsigned size)
	: completely_initialized_cacheline (size / CPU::CACHE_LINE_SIZE, false)
  	, uninitialized(size / CPU::CACHE_LINE_SIZE, getBitSetAllTrue())
	, msxcpu(motherBoard.getCPU())

{
	ram = new Ram(motherBoard, name, description, size);
	umrcount = 0;
}

CheckedRam::~CheckedRam()
{
//	std::cout << "(Destructor) UMRs detected: " << umrcount << std::endl << std::flush;
	delete ram;
}

const byte CheckedRam::read(unsigned addr)
{
//	if (uninitialized[addr >> CPU::CACHE_LINE_BITS][addr & CPU::CACHE_LINE_LOW]) std::cout << "UMR detected, reading from address 0x" << std::hex << std::setw(4) << std::setfill('0') << addr << std::endl;
	if (uninitialized[addr >> CPU::CACHE_LINE_BITS][addr & CPU::CACHE_LINE_LOW]) ++umrcount;
	return (*ram)[addr];	
}

const byte* CheckedRam::getReadCacheLine(unsigned addr)
{
	return (completely_initialized_cacheline[addr >> CPU::CACHE_LINE_BITS]) ? &((*ram)[addr]) : NULL;
}

byte* CheckedRam::getWriteCacheLine(unsigned addr)
{
	return (completely_initialized_cacheline[addr >> CPU::CACHE_LINE_BITS]) ? &((*ram)[addr]) : NULL;
}

const byte CheckedRam::peek(unsigned addr) const
{
	return (*ram)[addr];	
}

void CheckedRam::write(unsigned addr, const byte value)
{
	unsigned line = addr >> CPU::CACHE_LINE_BITS;
	uninitialized[line][addr & CPU::CACHE_LINE_LOW] = false;
	if (unlikely(uninitialized[line].none())) {
		completely_initialized_cacheline[line] = true;
		msxcpu.invalidateMemCache(addr & CPU::CACHE_LINE_HIGH, CPU::CACHE_LINE_SIZE);
	}
	(*ram)[addr] = value;
}

void CheckedRam::clear()
{
//	std::cout << "UMRs detected: " << umrcount << std::endl;
	umrcount = 0;
	ram->clear();
	std::fill(uninitialized.begin(), uninitialized.end(), getBitSetAllTrue());
	std::fill(completely_initialized_cacheline.begin(), completely_initialized_cacheline.end(), false);
}

unsigned CheckedRam::getSize()
{
	return ram->getSize();
}	

Ram* CheckedRam::getUncheckedRam()
{
	return ram;
}

} // namespace openmsx
