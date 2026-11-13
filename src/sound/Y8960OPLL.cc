#include "Y8960OPLL.hh"

#include "DeviceConfig.hh"
#include "MSXMotherBoard.hh"
#include "serialize.hh"

#include "cstd.hh"
#include "narrow.hh"
#include "outer.hh"
#include "xrange.hh"

#include <cassert>
#include <vector>

namespace openmsx {

static constexpr auto INPUT_RATE = unsigned(cstd::round(YM2413Core::CLOCK_FREQ / 72.0));

// Which melody channel's pan each of the core's 9+5 output buffers takes.
// The core puts the rhythm sounds out in the order of its time slots,
// HH, TOM, BD, SD, TC (channelOutput() in the core), not in the order of
// the key bits.
static constexpr std::array<uint8_t, Y8960OPLL::NUM_CHANNELS> PAN_OF_OUTPUT = {
	0, 1, 2, 3, 4, 5, 6, 7, 8,
	7, 8, 6, 7, 8,
};

Y8960OPLL::Y8960OPLL(const std::string& name_, const DeviceConfig& config)
	: ResampledSoundDevice(config.getMotherBoard(), name_, "Y8960 OPLL",
	                       NUM_CHANNELS, INPUT_RATE, true)
	, latch(0)
	, pan{}
	, gain{}
	, debuggable(config.getMotherBoard(), getName())
{
	for (auto ch : xrange(NUM_MELODY)) setPan(ch, 128);
	registerSound(config);
}

Y8960OPLL::~Y8960OPLL()
{
	unregisterSound();
}

void Y8960OPLL::reset(EmuTime time)
{
	updateStream(time);
	core.reset();
	latch = 0;
	for (auto ch : xrange(NUM_MELODY)) setPan(ch, 128);
}

void Y8960OPLL::setPan(unsigned channel, uint8_t value)
{
	pan[channel] = value;
	gain[channel] = y8960::panGain(value);
}

void Y8960OPLL::writePort(bool port, uint8_t value, EmuTime time)
{
	updateStream(time);

	if (!port) {
		latch = value;
	} else if ((latch >= PAN_BASE) && (latch < PAN_BASE + NUM_MELODY)) {
		setPan(latch - PAN_BASE, value);
	}

	// The core sees every write, so its register mirror holds the pan
	// registers too; it does nothing else with them.
	auto [integral, fractional] = getEmuClock().getTicksTillAsIntFloat(time);
	auto offset = narrow_cast<int>(18 * fractional);
	assert(integral == 0);
	assert(offset >= 0);
	assert(offset < 18);
	core.writePort(port, value, offset);
}

void Y8960OPLL::pokeReg(uint8_t reg, uint8_t value, EmuTime time)
{
	updateStream(time);
	if ((reg >= PAN_BASE) && (reg < PAN_BASE + NUM_MELODY)) {
		setPan(reg - PAN_BASE, value);
	}
	core.pokeReg(reg, value);
}

uint8_t Y8960OPLL::peekReg(uint8_t reg) const
{
	if ((reg >= PAN_BASE) && (reg < PAN_BASE + NUM_MELODY)) {
		return pan[reg - PAN_BASE];
	}
	auto regs = core.peekAllRegs();
	return (reg < regs.size()) ? regs[reg] : 0xFF;
}

void Y8960OPLL::setOutputRate(unsigned hostSampleRate, double speed)
{
	ResampledSoundDevice::setOutputRate(hostSampleRate, speed);
	core.setSpeed(speed);
}

float Y8960OPLL::getAmplificationFactorImpl() const
{
	return core.getAmplificationFactor();
}

void Y8960OPLL::generateChannels(std::span<float*> bufs, unsigned num)
{
	// The core produces mono; collect that first and then spread it over
	// the stereo output with the pan.
	std::vector<float> scratch(size_t(num) * NUM_CHANNELS);
	std::array<float*, NUM_CHANNELS> mono;
	for (auto i : xrange(NUM_CHANNELS)) {
		mono[i] = &scratch[size_t(i) * num];
	}
	// The core works on a copy of the pointers, so unlike the other cores it
	// never reports a silent channel by nulling its pointer; every channel
	// is spread.
	core.generateChannels(mono, num);

	for (auto i : xrange(NUM_CHANNELS)) {
		const auto& g = gain[PAN_OF_OUTPUT[i]];
		// The core puts the rhythm sounds out at twice the melody, as every
		// other OPLL emulator does. That is kept, although the Y8960 hardware
		// currently mixes them 1:1; the hardware is expected to follow the
		// real chip.
		const float* src = &scratch[size_t(i) * num];
		for (auto s : xrange(num)) {
			bufs[i][2 * s + 0] += src[s] * g.left;
			bufs[i][2 * s + 1] += src[s] * g.right;
		}
	}
}

// SimpleDebuggable

Y8960OPLL::Debuggable::Debuggable(MSXMotherBoard& motherBoard_, const std::string& name_)
	: SimpleDebuggable(motherBoard_, name_ + " regs", "Y8960 OPLL", REG_COUNT)
{
}

uint8_t Y8960OPLL::Debuggable::read(unsigned address, EmuTime /*time*/)
{
	const auto& opll = OUTER(Y8960OPLL, debuggable);
	return opll.peekReg(narrow<uint8_t>(address));
}

void Y8960OPLL::Debuggable::write(unsigned address, uint8_t value, EmuTime time)
{
	auto& opll = OUTER(Y8960OPLL, debuggable);
	opll.pokeReg(narrow<uint8_t>(address), value, time);
}

template<typename Archive>
void Y8960OPLL::serialize(Archive& ar, unsigned /*version*/)
{
	ar.serialize("core",  core,
	             "latch", latch,
	             "pan",   pan);
	if constexpr (Archive::IS_LOADER) {
		for (auto ch : xrange(NUM_MELODY)) setPan(ch, pan[ch]);
	}
}
INSTANTIATE_SERIALIZE_METHODS(Y8960OPLL);

} // namespace openmsx
