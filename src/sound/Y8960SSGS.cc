#include "Y8960SSGS.hh"

#include "DeviceConfig.hh"
#include "MSXMotherBoard.hh"
#include "serialize.hh"

#include "outer.hh"
#include "xrange.hh"

#include <algorithm>

namespace openmsx {

// The cores run at the same rate the AY8910 does.
static constexpr int NATIVE_FREQ_INT = 3579545 / 2 / 8;

Y8960SSGS::Y8960SSGS(const std::string& name_, const DeviceConfig& config, EmuTime time)
	: ResampledSoundDevice(config.getMotherBoard(), name_, "Y8960 SSGS",
	                       NUM_CHANNELS, NATIVE_FREQ_INT, true)
	, unit{Y8960SsgCore(name_ + " unit0", config, time),
	       Y8960SsgCore(name_ + " unit1", config, time)}
	, volume{}
	, pan{}
	, gain{}
	, dc{}
	, debuggable(config.getMotherBoard(), getName())
{
	reset(time);
	registerSound(config);
}

Y8960SSGS::~Y8960SSGS()
{
	unregisterSound();
}

void Y8960SSGS::reset(EmuTime time)
{
	for (auto& u : unit) u.reset(time);
	std::ranges::fill(volume, 0x80);
	std::ranges::fill(pan, 128);
	for (auto ch : xrange(NUM_CHANNELS)) updateGain(ch);
	measureDc();
}

void Y8960SSGS::measureDc()
{
	// Generate a single sample with everything silent to find out where the
	// zero level of each channel sits.
	for (auto u : xrange(NUM_UNITS)) {
		std::array<float, CHANNELS_PER_UNIT> sample = {};
		std::array<float*, CHANNELS_PER_UNIT> bufs = {
			&sample[0], &sample[1], &sample[2]};
		unit[u].generateChannels(bufs, 1);
		for (auto c : xrange(CHANNELS_PER_UNIT)) {
			// a nullptr buffer means the core decided the channel is silent
			dc[u * CHANNELS_PER_UNIT + c] = bufs[c] ? sample[c] : 0.0f;
		}
	}
}

void Y8960SSGS::updateGain(unsigned channel)
{
	auto g = y8960::panGain(pan[channel]);
	float v = y8960::volumeGain(volume[channel]);
	gain[channel] = {.left = g.left * v, .right = g.right * v};
}

std::optional<unsigned> Y8960SSGS::mixChannel(unsigned reg)
{
	// 20h-22h / 24h-26h and 28h-2Ah / 2Ch-2Eh: unit in bit 2, channel in
	// bits 0-1, with channel 3 the unconnected slot
	unsigned slot = reg & 7;
	if ((slot & 3) == 3) return std::nullopt;
	return (slot >> 2) * CHANNELS_PER_UNIT + (slot & 3);
}

void Y8960SSGS::writeRegister(unsigned reg, uint8_t value, EmuTime time)
{
	if (reg >= REG_END) return;

	updateStream(time);

	if (reg < VOLUME_BASE) {
		unit[reg / UNIT_REGS].writeRegister(reg % UNIT_REGS, value, time);
	} else if (auto ch = mixChannel(reg)) {
		((reg >= PAN_BASE) ? pan : volume)[*ch] = value;
		updateGain(*ch);
	}
}

uint8_t Y8960SSGS::peekRegister(unsigned reg, EmuTime time) const
{
	if (reg >= REG_END) return 0xFF;

	if (reg < VOLUME_BASE) {
		return unit[reg / UNIT_REGS].peekRegister(reg % UNIT_REGS, time);
	} else if (auto ch = mixChannel(reg)) {
		return ((reg >= PAN_BASE) ? pan : volume)[*ch];
	}
	return 0xFF;
}

float Y8960SSGS::getAmplificationFactorImpl() const
{
	return unit[0].getAmplificationFactor();
}

void Y8960SSGS::generateChannels(std::span<float*> bufs, unsigned num)
{
	// The cores produce mono; collect that first and then spread it over
	// the stereo output with the volume and the pan.
	std::vector<float> scratch(size_t(num) * CHANNELS_PER_UNIT);

	for (auto u : xrange(NUM_UNITS)) {
		std::ranges::fill(scratch, 0.0f);
		std::array<float*, CHANNELS_PER_UNIT> mono = {
			&scratch[0], &scratch[num], &scratch[size_t(2) * num]};
		unit[u].generateChannels(mono, num);

		for (auto c : xrange(CHANNELS_PER_UNIT)) {
			unsigned ch = u * CHANNELS_PER_UNIT + c;
			if (!mono[c]) {
				// silent channel, the core left the buffer alone
				bufs[ch] = nullptr;
				continue;
			}
			const auto& g = gain[ch];
			const float* src = &scratch[size_t(c) * num];
			float offset = dc[ch];
			for (auto i : xrange(num)) {
				float v = src[i] - offset;
				bufs[ch][2 * i + 0] += v * g.left;
				bufs[ch][2 * i + 1] += v * g.right;
			}
		}
	}
}

// SimpleDebuggable

Y8960SSGS::Debuggable::Debuggable(MSXMotherBoard& motherBoard_, const std::string& name_)
	: SimpleDebuggable(motherBoard_, name_ + " regs", "Y8960 SSGS", REG_END)
{
}

uint8_t Y8960SSGS::Debuggable::read(unsigned address, EmuTime time)
{
	const auto& ssg = OUTER(Y8960SSGS, debuggable);
	return ssg.peekRegister(address, time);
}

void Y8960SSGS::Debuggable::write(unsigned address, uint8_t value, EmuTime time)
{
	auto& ssg = OUTER(Y8960SSGS, debuggable);
	ssg.writeRegister(address, value, time);
}

template<typename Archive>
void Y8960SSGS::serialize(Archive& ar, unsigned /*version*/)
{
	ar.serialize("unit0",  unit[0],
	             "unit1",  unit[1],
	             "volume", volume,
	             "pan",    pan);
	if constexpr (Archive::IS_LOADER) {
		for (auto ch : xrange(NUM_CHANNELS)) updateGain(ch);
		measureDc();
	}
}
INSTANTIATE_SERIALIZE_METHODS(Y8960SSGS);

} // namespace openmsx
