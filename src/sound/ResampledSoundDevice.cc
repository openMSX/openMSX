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
		MSXMotherBoard& motherBoard, string_ref name,
		string_ref description, unsigned channels,
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


void ResampledSoundDevice::setOutputRate(unsigned /*sampleRate*/)
{
	createResampler();
}

bool ResampledSoundDevice::updateBuffer(unsigned length, int* buffer,
                                        EmuTime::param time)
{
	return algo->generateOutput(buffer, length, time);
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
	const DynamicClock& hostClock = getHostSampleClock();
	unsigned outputRate = hostClock.getFreq();
	unsigned inputRate  = getInputRate() / getEffectiveSpeed();

	if (outputRate == inputRate) {
		algo.reset(new ResampleTrivial(*this));
	} else {
		switch (resampleSetting.getValue()) {
		case RESAMPLE_HQ:
			if (!isStereo()) {
				algo.reset(new ResampleHQ<1>(*this, hostClock, inputRate));
			} else {
				algo.reset(new ResampleHQ<2>(*this, hostClock, inputRate));
			}
			break;
		case RESAMPLE_LQ:
			if (!isStereo()) {
				algo = ResampleLQ<1>::create(*this, hostClock, inputRate);
			} else {
				algo = ResampleLQ<2>::create(*this, hostClock, inputRate);
			}
			break;
		case RESAMPLE_BLIP:
			if (!isStereo()) {
				algo.reset(new ResampleBlip<1>(*this, hostClock, inputRate));
			} else {
				algo.reset(new ResampleBlip<2>(*this, hostClock, inputRate));
			}
			break;
		default:
			UNREACHABLE;
		}
	}
}

} // namespace openmsx
