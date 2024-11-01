#include "ResampledSoundDevice.hh"

#include "ResampleTrivial.hh"
#include "ResampleHQ.hh"
#include "ResampleBlip.hh"

#include "EnumSetting.hh"
#include "GlobalSettings.hh"
#include "MSXMotherBoard.hh"
#include "Reactor.hh"

#include "unreachable.hh"

#include <cassert>
#include <memory>

namespace openmsx {

ResampledSoundDevice::ResampledSoundDevice(
		MSXMotherBoard& motherBoard, std::string_view name_,
		static_string_view description_, unsigned channels,
		unsigned inputSampleRate_, bool stereo_)
	: SoundDevice(motherBoard.getMSXMixer(), name_, description_,
	              channels, inputSampleRate_, stereo_)
	, resampleSetting(motherBoard.getReactor().getGlobalSettings().getResampleSetting())
{
	resampleSetting.attach(*this);
}

ResampledSoundDevice::~ResampledSoundDevice()
{
	resampleSetting.detach(*this);
}

void ResampledSoundDevice::setOutputRate(unsigned /*hostSampleRate*/, double /*speed*/)
{
	createResampler();
}

bool ResampledSoundDevice::updateBuffer(size_t length, float* buffer,
                                        EmuTime::param time)
{
	return algo->generateOutput(buffer, length, time);
}

bool ResampledSoundDevice::generateInput(float* buffer, size_t num)
{
	return mixChannels(buffer, num);
}

void ResampledSoundDevice::update(const Setting& setting) noexcept
{
	(void)setting;
	assert(&setting == &resampleSetting);
	createResampler();
}

void ResampledSoundDevice::createResampler()
{
	const DynamicClock& hostClock = getHostSampleClock();
	EmuDuration outputPeriod = hostClock.getPeriod();
	EmuDuration inputPeriod(getEffectiveSpeed() / double(getInputRate()));
	emuClock.reset(hostClock.getTime());
	emuClock.setPeriod(inputPeriod);

	if (outputPeriod == inputPeriod) {
		algo = std::make_unique<ResampleTrivial>(*this);
	} else {
		switch (resampleSetting.getEnum()) {
		case ResampleType::HQ:
			if (!isStereo()) {
				algo = std::make_unique<ResampleHQ<1>>(*this, hostClock);
			} else {
				algo = std::make_unique<ResampleHQ<2>>(*this, hostClock);
			}
			break;
		case ResampleType::BLIP:
			if (!isStereo()) {
				algo = std::make_unique<ResampleBlip<1>>(*this, hostClock);
			} else {
				algo = std::make_unique<ResampleBlip<2>>(*this, hostClock);
			}
			break;
		default:
			UNREACHABLE;
		}
	}
}

} // namespace openmsx
