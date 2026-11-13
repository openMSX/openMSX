#include "Y8960Mixer.hh"

#include "DeviceConfig.hh"
#include "MSXException.hh"
#include "MSXMixer.hh"
#include "MSXMotherBoard.hh"
#include "SoundDevice.hh"
#include "XMLElement.hh"
#include "Y8960Pan.hh"
#include "serialize.hh"

#include "StringOp.hh"
#include "stl.hh"
#include "xrange.hh"

#include <algorithm>
#include <cassert>

namespace openmsx {

using Output = Y8960Mixer::Output;

[[nodiscard]] static Output getDefaultOutput(const DeviceConfig& config)
{
	auto value = config.getChildData("default_output", "y8960");
	StringOp::casecmp cmp; // case-insensitive
	if (cmp(value, "y8960")) return Output::Y8960;
	if (cmp(value, "msx"))   return Output::Msx;
	if (cmp(value, "mix"))   return Output::Mix;
	throw MSXException(
		"Illegal default_output in the Y8960 mixer configuration: '", value,
		"', expected 'y8960', 'msx' or 'mix'.");
}

Y8960Mixer::Y8960Mixer(const DeviceConfig& config)
	: MSXDevice(config)
{
	for (const auto* e : config.getXML()->getChildren("channel")) {
		std::string_view idRef = e->getAttributeValue("idref", "");
		if (idRef == "") continue;

		int num = e->getAttributeValueAsInt("num", 0);
		if (num < 0 || num >= ChannelCount) {
			throw MSXException("Invalid channel number(", num, ") device '", idRef, "'");
		}

		channelDevices[num].push_back(idRef);
	}

	// Report a channel that names something which is not a sound device
	// here, rather than leaving it silently unmixed.
	auto& mixer = getMotherBoard().getMSXMixer();
	for (const auto& devices : channelDevices) {
		for (std::string_view device : devices) {
			if (!mixer.findDevice(device)) {
				throw MSXException("Unknown sound device '", device,
				                   "' in the Y8960 mixer configuration.");
			}
		}
	}

	outputSetting = std::make_unique<EnumSetting<Output>>(
		getCommandController(), getName() + "_output",
		"Which sound output to listen to: the Y8960's own, the MSX's, or both.",
		getDefaultOutput(config),
		EnumSetting<Output>::Map{
			{"y8960", Output::Y8960},
			{"msx",   Output::Msx  },
			{"mix",   Output::Mix  }});
	outputSetting->attach(*this);

	reset(getCurrentTime());
}

Y8960Mixer::~Y8960Mixer()
{
	outputSetting->detach(*this);

	// Hand every device back at unity. Without this, removing the cartridge
	// while listening to its output alone would leave the MSX muted.
	auto& mixer = getMotherBoard().getMSXMixer();
	for (const auto& info : mixer.getDeviceInfos()) {
		mixer.setDeviceGain(info.device->getName(), 1.0f, 1.0f);
	}
}

void Y8960Mixer::reset(EmuTime /*time*/)
{
	std::ranges::fill(volume, 0x80);
	// The specification gives no reset value for the master volume. It
	// starts at the maximum like the per-block volumes; anything lower
	// would leave every block quieter than its own volume says.
	masterVolume = 0x80;
	applyAllGains();
}

void Y8960Mixer::writeVolume(int channel, uint8_t value)
{
	assert(channel >= 0 && channel < ChannelCount);
	volume[channel] = value;
	applyAllGains();
}

void Y8960Mixer::writeMasterVolume(uint8_t value)
{
	masterVolume = value;
	applyAllGains();
}

std::optional<int> Y8960Mixer::findChannel(std::string_view name) const
{
	for (auto ch : xrange(ChannelCount)) {
		if (contains(channelDevices[ch], name)) return ch;
	}
	return {};
}

void Y8960Mixer::applyAllGains()
{
	// Silencing one of the two outputs is a gain of zero on the devices
	// behind it. The cartridge's own devices are the ones its channels
	// name; whatever else is registered belongs to the MSX.
	auto output = outputSetting->getEnum();
	// The master volume belongs to the cartridge's output only; the MSX's
	// sound never passes through it.
	float y8960Gain = (output != Output::Msx)
	                ? y8960::volumeGain(masterVolume) : 0.0f;
	float msxGain   = (output != Output::Y8960) ? 1.0f : 0.0f;

	auto& mixer = getMotherBoard().getMSXMixer();
	for (const auto& info : mixer.getDeviceInfos()) {
		std::string_view name = info.device->getName();
		if (auto channel = findChannel(name)) {
			float g = y8960::volumeGain(volume[*channel]) * y8960Gain;
			mixer.setDeviceGain(name, g, g);
		} else {
			mixer.setDeviceGain(name, msxGain, msxGain);
		}
	}
}

void Y8960Mixer::update(const Setting& /*setting*/) noexcept
{
	applyAllGains();
}

template<typename Archive>
void Y8960Mixer::serialize(Archive& ar, unsigned version)
{
	ar.template serializeBase<MSXDevice>(*this);
	if (ar.versionAtLeast(version, 5)) {
		ar.serialize("volume", volume);
	} else if (ar.versionAtLeast(version, 3)) {
		std::array<uint8_t, OldChannelCount> oldVolume;
		ar.serialize("volume", oldVolume);
		std::ranges::copy(oldVolume, volume.begin());
	}
	if (ar.versionAtLeast(version, 4)) {
		ar.serialize("masterVolume", masterVolume);
	}
	if constexpr (Archive::IS_LOADER) {
		applyAllGains();
	}
}
INSTANTIATE_SERIALIZE_METHODS(Y8960Mixer);
REGISTER_MSXDEVICE(Y8960Mixer, "Y8960MIXER");

} // namespace openmsx
