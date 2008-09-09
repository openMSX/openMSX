// $Id$

#ifndef ESE_SCC_HH
#define ESE_SCC_HH

#include "MSXDevice.hh"
#include <memory>

namespace openmsx {

class SRAM;
class SCC;
class MB89352;

class ESE_SCC : public MSXDevice
{
public:
	ESE_SCC(MSXMotherBoard& motherBoard, const XMLElement& config,
	        bool withSCSI);
	virtual ~ESE_SCC();

	virtual void reset(const EmuTime& time);

	virtual byte readMem(word address, const EmuTime& time);
	virtual byte peekMem(word address, const EmuTime& time) const;
	virtual void writeMem(word address, byte value, const EmuTime& time);
	virtual const byte* getReadCacheLine(word start) const;
	virtual byte* getWriteCacheLine(word start) const;

	template<typename Archive>
	void serialize(Archive& ar, unsigned version);

private:
	void setMapperLow(unsigned page, byte value);
	void setMapperHigh(byte value);

	const std::auto_ptr<SRAM> sram;
	const std::auto_ptr<SCC> scc;
	const std::auto_ptr<MB89352> spc;

	const byte mapperMask;
	byte mapper[4];
	bool spcEnable;
	bool sccEnable;
	bool writeEnable;
};

REGISTER_MSXDEVICE(ESE_SCC, "ESE_SCC");

} // namespace openmsx

#endif
