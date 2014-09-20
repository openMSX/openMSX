#include "YM2413.hh"
#include "YM2413Okazaki.hh"
#include "YM2413Burczynski.hh"
#include "SimpleDebuggable.hh"
#include "DeviceConfig.hh"
#include "serialize.hh"
#include "memory.hh"

namespace openmsx {

// YM2413Debuggable

class YM2413Debuggable final : public SimpleDebuggable
{
public:
	YM2413Debuggable(MSXMotherBoard& motherBoard, YM2413& ym2413);
	byte read(unsigned address) override;
	void write(unsigned address, byte value, EmuTime::param time) override;
private:
	YM2413& ym2413;
};


YM2413Debuggable::YM2413Debuggable(
		MSXMotherBoard& motherBoard, YM2413& ym2413_)
	: SimpleDebuggable(motherBoard, ym2413_.getName() + " regs",
	                   "MSX-MUSIC", 0x40)
	, ym2413(ym2413_)
{
}

byte YM2413Debuggable::read(unsigned address)
{
	return ym2413.core->peekReg(address);
}

void YM2413Debuggable::write(unsigned address, byte value, EmuTime::param time)
{
	ym2413.writeReg(address, value, time);
}


// YM2413

static std::unique_ptr<YM2413Core> createCore(const DeviceConfig& config)
{
	if (config.getChildDataAsBool("alternative", false)) {
		return make_unique<YM2413Burczynski::YM2413>();
	} else {
		return make_unique<YM2413Okazaki::YM2413>();
	}
}

YM2413::YM2413(const std::string& name, const DeviceConfig& config)
	: ResampledSoundDevice(config.getMotherBoard(), name, "MSX-MUSIC", 9 + 5)
	, core(createCore(config))
	, debuggable(make_unique<YM2413Debuggable>(
		config.getMotherBoard(), *this))
{
	double input = YM2413Core::CLOCK_FREQ / 72.0;
	setInputRate(int(input + 0.5));

	registerSound(config);
}

YM2413::~YM2413()
{
	unregisterSound();
}

void YM2413::reset(EmuTime::param time)
{
	updateStream(time);
	core->reset();
}

void YM2413::writeReg(byte reg, byte value, EmuTime::param time)
{
	updateStream(time);
	core->writeReg(reg, value);
}

void YM2413::generateChannels(int** bufs, unsigned num)
{
	core->generateChannels(bufs, num);
}

int YM2413::getAmplificationFactor() const
{
	return core->getAmplificationFactor();
}


template<typename Archive>
void YM2413::serialize(Archive& ar, unsigned /*version*/)
{
	ar.serializePolymorphic("ym2413", *core);
}
INSTANTIATE_SERIALIZE_METHODS(YM2413);

} // namespace openmsx
