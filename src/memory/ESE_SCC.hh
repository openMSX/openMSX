// $Id$

#ifndef ESE_SCC_HH
#define ESE_SCC_HH

#include "MSXDevice.hh"
#include <memory>

namespace openmsx {

class SRAM;
class SCC;
class MB89352;
class MSXCPU;

class ESE_SCC : public MSXDevice
{
public:
	ESE_SCC(MSXMotherBoard& motherBoard, const XMLElement& config,
	         const EmuTime& time, bool withSCSI);
	virtual ~ESE_SCC();

	virtual void reset(const EmuTime& time);

	virtual byte readMem(word address, const EmuTime& time);
	virtual void writeMem(word address, byte value, const EmuTime& time);
	virtual const byte* getReadCacheLine(word start) const;
	virtual byte* getWriteCacheLine(word start) const;

private: 
	void setMapperLow(unsigned page, byte value);
	void setMapperHigh(byte value);

	std::auto_ptr<SRAM> sram;
	std::auto_ptr<SCC> scc;
	std::auto_ptr<MB89352> spc;
	MSXCPU& cpu;

	byte mapper[4];
	byte mapperHigh;
	byte mapperMask;
	bool spcEnable;
	bool sccEnable;
	bool writeEnable;
};

} // namespace openmsx

#endif
