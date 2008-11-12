// $Id$

#ifndef MSXYAMAHASFG_HH
#define MSXYAMAHASFG_HH

#include "MSXDevice.hh"
#include <memory>

namespace openmsx {

class Rom;
class YM2151;
class YM2148;

class MSXYamahaSFG : public MSXDevice
{
public:
	MSXYamahaSFG(MSXMotherBoard& motherBoard, const XMLElement& config);
	virtual ~MSXYamahaSFG();

	virtual void reset(EmuTime::param time);
	virtual byte readMem(word address, EmuTime::param time);
	virtual void writeMem(word address, byte value, EmuTime::param time);
	virtual byte readIRQVector();

	template<typename Archive>
	void serialize(Archive& ar, unsigned version);

private:
	void writeRegisterPort(byte value, EmuTime::param time);
	void writeDataPort(byte value, EmuTime::param time);

	const std::auto_ptr<Rom> rom;
	const std::auto_ptr<YM2151> ym2151;
	const std::auto_ptr<YM2148> ym2148;
	int registerLatch;
	byte irqVector;
};

} // namespace openmsx

#endif
