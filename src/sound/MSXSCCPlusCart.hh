// $Id$

// Note: this device is actually called SCC-II. But this would take a lot of
// renaming, which isn't worth it right now. TODO rename this :)

#ifndef __MSXSCCPLUSCART_HH__
#define __MSXSCCPLUSCART_HH__

#include "MSXDevice.hh"
#include <memory>

namespace openmsx {

class SCC;
class Ram;

class MSXSCCPlusCart : public MSXDevice
{
public:
	MSXSCCPlusCart(const XMLElement& config, const EmuTime& time);
	virtual ~MSXSCCPlusCart();

	virtual void reset(const EmuTime& time);
	virtual byte readMem(word address, const EmuTime& time);
	virtual void writeMem(word address, byte value, const EmuTime& time);
	virtual const byte* getReadCacheLine(word start) const;
	virtual byte* getWriteCacheLine(word start) const;

private:
	void setMapper(int regio, byte value);
	void setModeRegister(byte value);
	void checkEnable();
	
	const std::auto_ptr<Ram> ram;
	std::auto_ptr<SCC> scc;
	byte modeRegister;
	enum SCCEnable {EN_NONE, EN_SCC, EN_SCCPLUS} enable;
	bool isRamSegment[4];
	bool isMapped[4];
	byte* internalMemoryBank[4];	// 4 blocks of 8kB starting at #4000
	byte mapper[4];
	byte mapperMask;
	bool lowRAM, highRAM; 
};

} // namespace openmsx

#endif //__MSXSCCPlusCart_HH__
