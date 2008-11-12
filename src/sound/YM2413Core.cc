// $Id$

#include "YM2413Core.hh"
#include "MSXMotherBoard.hh"
#include "SimpleDebuggable.hh"
#include "serialize.hh"
#include <cstring>

namespace openmsx {

class YM2413Debuggable : public SimpleDebuggable
{
public:
	YM2413Debuggable(MSXMotherBoard& motherBoard, YM2413Core& ym2413);
	virtual byte read(unsigned address);
	virtual void write(unsigned address, byte value, EmuTime::param time);
private:
	YM2413Core& ym2413;
};


// YM2413Core

YM2413Core::YM2413Core(MSXMotherBoard& motherBoard, const std::string& name)
	: SoundDevice(motherBoard.getMSXMixer(), name, "MSX-MUSIC", 11)
	, Resample(motherBoard.getGlobalSettings(), 1)
	, debuggable(new YM2413Debuggable(motherBoard, *this))
{
	memset(reg, 0, sizeof(reg)); // avoid UMR
}

YM2413Core::~YM2413Core()
{
}

bool YM2413Core::generateInput(int* buffer, unsigned num)
{
	return mixChannels(buffer, num);
}

void YM2413Core::setOutputRate(unsigned sampleRate)
{
	double input = CLOCK_FREQ / 72.0;
	setInputRate(int(input + 0.5));
	setResampleRatio(input, sampleRate);
}

bool YM2413Core::updateBuffer(unsigned length, int* buffer,
     EmuTime::param /*time*/, EmuDuration::param /*sampDur*/)
{
	return generateOutput(buffer, length);
}


// YM2413Debuggable

YM2413Debuggable::YM2413Debuggable(
		MSXMotherBoard& motherBoard, YM2413Core& ym2413_)
	: SimpleDebuggable(motherBoard, ym2413_.getName() + " regs",
	                   "MSX-MUSIC", 0x40)
	, ym2413(ym2413_)
{
}

byte YM2413Debuggable::read(unsigned address)
{
	return ym2413.reg[address];
}

void YM2413Debuggable::write(unsigned address, byte value, EmuTime::param time)
{
	ym2413.writeReg(address, value, time);
}

template<typename Archive>
void YM2413Core::serialize(Archive& ar, unsigned /*version*/)
{
	ar.serialize("registers", reg);
}
INSTANTIATE_SERIALIZE_METHODS(YM2413Core);

} // namespace openmsx
