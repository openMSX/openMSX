// Note: this device is actually called SCC-I. But this would take a lot of
// renaming, which isn't worth it right now. TODO rename this :)

#ifndef MSXSCCPLUSCART_HH
#define MSXSCCPLUSCART_HH

#include "MSXDevice.hh"
#include <memory>

namespace openmsx {

class SCC;
class Ram;
class RomBlockDebuggable;

class MSXSCCPlusCart final : public MSXDevice
{
public:
	explicit MSXSCCPlusCart(const DeviceConfig& config);
	~MSXSCCPlusCart();

	virtual void powerUp(EmuTime::param time);
	virtual void reset(EmuTime::param time);
	virtual byte readMem(word address, EmuTime::param time);
	virtual byte peekMem(word address, EmuTime::param time) const;
	virtual void writeMem(word address, byte value, EmuTime::param time);
	virtual const byte* getReadCacheLine(word start) const;
	virtual byte* getWriteCacheLine(word start) const;

	template<typename Archive>
	void serialize(Archive& ar, unsigned version);

private:
	void setMapper(int regio, byte value);
	void setModeRegister(byte value);
	void checkEnable();

	const std::unique_ptr<Ram> ram;
	const std::unique_ptr<SCC> scc;
	const std::unique_ptr<RomBlockDebuggable> romBlockDebug;
	byte* internalMemoryBank[4]; // 4 blocks of 8kB starting at #4000
	enum SCCEnable {EN_NONE, EN_SCC, EN_SCCPLUS} enable;
	byte modeRegister;
	bool isRamSegment[4];
	bool isMapped[4];
	byte mapper[4];
	/*const*/ byte mapperMask;
	/*const*/ bool lowRAM, highRAM;
};

} // namespace openmsx

#endif
