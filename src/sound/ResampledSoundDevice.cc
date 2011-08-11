// $Id$

#include "ResampledSoundDevice.hh"
#include "ResampleTrivial.hh"
#include "ResampleHQ.hh"
#include "ResampleLQ.hh"
#include "ResampleBlip.hh"
#include "MSXMotherBoard.hh"
#include "Reactor.hh"
#include "GlobalSettings.hh"
#include "EnumSetting.hh"
#include "unreachable.hh"
#include <cassert>

namespace openmsx {

ResampledSoundDevice::ResampledSoundDevice(
		MSXMotherBoard& motherBoard, const std::string& name,
		const std::string& description, unsigned channels,
		bool stereo)
	: SoundDevice(motherBoard.getMSXMixer(), name, description, channels, stereo)
	, resampleSetting(motherBoard.getReactor().getGlobalSettings().getResampleSetting())
{
	resampleSetting.attach(*this);
}

ResampledSoundDevice::~ResampledSoundDevice()
{
	resampleSetting.detach(*this);
}

void ResampledSoundDevice::setOutputRate(unsigned sampleRate)
{
	outputRate = sampleRate;
	createResampler();
}

bool ResampledSoundDevice::updateBuffer(
		unsigned length, int* buffer,
		EmuTime::param /*start*/, EmuDuration::param /*sampDur*/)
{
	return algo->generateOutput(buffer, length);
}

bool ResampledSoundDevice::generateInput(int* buffer, unsigned num)
{
	return mixChannels(buffer, num);
}


void ResampledSoundDevice::update(const Setting& setting)
{
	(void)setting;
	assert(&setting == &resampleSetting);
	createResampler();
}

void ResampledSoundDevice::createResampler()
{
	double ratio = double(getInputRate()) / outputRate;
	if (ratio == 1.0) {
		algo.reset(new ResampleTrivial(*this));
	} else {
		switch (resampleSetting.getValue()) {
		case RESAMPLE_HQ:
			if (!isStereo()) {
				algo.reset(new ResampleHQ<1>(*this, ratio));
			} else {
				algo.reset(new ResampleHQ<2>(*this, ratio));
			}
			break;
		case RESAMPLE_LQ:
			if (!isStereo()) {
				algo = ResampleLQ<1>::create(*this, ratio);
			} else {
				algo = ResampleLQ<2>::create(*this, ratio);
			}
			break;
		case RESAMPLE_BLIP:
			if (!isStereo()) {
				algo.reset(new ResampleBlip<1>(*this, ratio));
			} else {
				algo.reset(new ResampleBlip<2>(*this, ratio));
			}
			break;
		default:
			UNREACHABLE;
		}
	}
}

} // namespace openmsx
