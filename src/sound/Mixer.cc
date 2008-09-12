// $Id$

#include "Mixer.hh"
#include "MSXMixer.hh"
#include "NullSoundDriver.hh"
#include "SDLSoundDriver.hh"
#include "DirectXSoundDriver.hh"
#include "CommandController.hh"
#include "CliComm.hh"
#include "GlobalSettings.hh"
#include "IntegerSetting.hh"
#include "BooleanSetting.hh"
#include "EnumSetting.hh"
#include "MSXException.hh"
#include <cassert>

namespace openmsx {

#ifdef _WIN32
static const int defaultsamples = 2048;
#else
static const int defaultsamples = 1024;
#endif

Mixer::Mixer(CommandController& commandController_)
	: commandController(commandController_)
	, muteSetting(new BooleanSetting(commandController,
		"mute", "(un)mute the emulation sound", false,
		Setting::DONT_SAVE))
	, masterVolume(new IntegerSetting(commandController,
		"master_volume", "master volume", 75, 0, 100))
	, frequencySetting(new IntegerSetting(commandController,
		"frequency", "mixer frequency", 44100, 11025, 48000))
	, samplesSetting(new IntegerSetting(commandController,
		"samples", "mixer samples", defaultsamples, 64, 8192))
	, pauseSetting(commandController.getGlobalSettings().getPauseSetting())
	, muteCount(0)
{
	EnumSetting<SoundDriverType>::Map soundDriverMap;
	soundDriverMap["null"]    = SND_NULL;
	soundDriverMap["sdl"]     = SND_SDL;
	SoundDriverType defaultSoundDriver = SND_SDL;

#ifdef _WIN32
	soundDriverMap["directx"] = SND_DIRECTX;
	defaultSoundDriver = SND_DIRECTX;
#endif

	soundDriverSetting.reset(new EnumSetting<SoundDriverType>(
		commandController, "sound_driver",
		"select the sound output driver",
		defaultSoundDriver, soundDriverMap));

	muteSetting->attach(*this);
	frequencySetting->attach(*this);
	samplesSetting->attach(*this);
	soundDriverSetting->attach(*this);
	pauseSetting.attach(*this);

	// Set correct initial mute state.
	if (muteSetting->getValue()) ++muteCount;
	if (pauseSetting.getValue()) ++muteCount;

	reloadDriver();
}

Mixer::~Mixer()
{
	assert(msxMixers.empty());
	driver.reset();

	pauseSetting.detach(*this);
	soundDriverSetting->detach(*this);
	samplesSetting->detach(*this);
	frequencySetting->detach(*this);
	muteSetting->detach(*this);
}

void Mixer::reloadDriver()
{
	driver.reset(new NullSoundDriver());

	try {
		switch (soundDriverSetting->getValue()) {
		case SND_NULL:
			driver.reset(new NullSoundDriver());
			break;
		case SND_SDL:
			driver.reset(new SDLSoundDriver(
				frequencySetting->getValue(),
				samplesSetting->getValue()));
			break;
#ifdef _WIN32
		case SND_DIRECTX:
			driver.reset(new DirectXSoundDriver(
				frequencySetting->getValue(),
				samplesSetting->getValue()));
			break;
#endif
		default:
			assert(false);
		}
	} catch (MSXException& e) {
		commandController.getCliComm().printWarning(e.getMessage());
	}

	muteHelper();
}

void Mixer::registerMixer(MSXMixer& mixer)
{
	assert(count(msxMixers.begin(), msxMixers.end(), &mixer) == 0);
	msxMixers.push_back(&mixer);

	muteHelper();
}

void Mixer::unregisterMixer(MSXMixer& mixer)
{
	assert(count(msxMixers.begin(), msxMixers.end(), &mixer) == 1);
	msxMixers.erase(find(msxMixers.begin(), msxMixers.end(), &mixer));

	muteHelper();
}


void Mixer::mute()
{
	if (muteCount++ == 0) {
		muteHelper();
	}
}

void Mixer::unmute()
{
	assert(muteCount);
	if (--muteCount == 0) {
		muteHelper();
	}
}

void Mixer::muteHelper()
{
	bool mute = muteCount || msxMixers.empty();
	for (MSXMixers::iterator it = msxMixers.begin();
	     it != msxMixers.end(); ++it) {
		unsigned samples = mute ? 0 : driver->getSamples();
		unsigned frequency = driver->getFrequency();
		(*it)->setMixerParams(samples, frequency);
	}

	if (mute) {
		driver->mute();
	} else {
		driver->unmute();
	}
}

IntegerSetting& Mixer::getMasterVolume() const
{
	return *masterVolume;
}

double Mixer::uploadBuffer(MSXMixer& /*msxMixer*/, short* buffer, unsigned len)
{
	// can only handle one MSXMixer ATM
	assert(!msxMixers.empty());

	return driver->uploadBuffer(buffer, len);
}

void Mixer::update(const Setting& setting)
{
	if (&setting == muteSetting.get()) {
		if (muteSetting->getValue()) {
			mute();
		} else {
			unmute();
		}
	} else if (&setting == &pauseSetting) {
		if (pauseSetting.getValue()) {
			mute();
		} else {
			unmute();
		}
	} else if ((&setting == samplesSetting.get()) ||
	           (&setting == soundDriverSetting.get()) ||
	           (&setting == frequencySetting.get())) {
		reloadDriver();
	} else {
		assert(false);
	}
}

} // namespace openmsx
