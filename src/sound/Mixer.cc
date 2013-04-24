#include "Mixer.hh"
#include "MSXMixer.hh"
#include "NullSoundDriver.hh"
#include "SDLSoundDriver.hh"
#include "DirectXSoundDriver.hh"
#include "LibAOSoundDriver.hh"
#include "CommandController.hh"
#include "CliComm.hh"
#include "IntegerSetting.hh"
#include "BooleanSetting.hh"
#include "EnumSetting.hh"
#include "MSXException.hh"
#include "unreachable.hh"
#include "memory.hh"
#include "components.hh"
#include "build-info.hh"
#include <cassert>

namespace openmsx {

#if defined(_WIN32)
static const int defaultsamples = 2048;
#elif PLATFORM_ANDROID
static const int defaultsamples = 2560;
#else
static const int defaultsamples = 1024;
#endif

Mixer::Mixer(Reactor& reactor_, CommandController& commandController_)
	: reactor(reactor_)
	, commandController(commandController_)
	, muteSetting(make_unique<BooleanSetting>(
		commandController, "mute",
		"(un)mute the emulation sound", false, Setting::DONT_SAVE))
	, masterVolume(make_unique<IntegerSetting>(
		commandController, "master_volume",
		"master volume", 75, 0, 100))
	, frequencySetting(make_unique<IntegerSetting>(
		commandController, "frequency",
		"mixer frequency", 44100, 11025, 48000))
	, samplesSetting(make_unique<IntegerSetting>(
		commandController, "samples",
		"mixer samples", defaultsamples, 64, 8192))
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
#if COMPONENT_AO
	soundDriverMap["libao"] = SND_LIBAO;
#endif

	soundDriverSetting = make_unique<EnumSetting<SoundDriverType>>(
		commandController, "sound_driver",
		"select the sound output driver",
		defaultSoundDriver, soundDriverMap);

	muteSetting->attach(*this);
	frequencySetting->attach(*this);
	samplesSetting->attach(*this);
	soundDriverSetting->attach(*this);

	// Set correct initial mute state.
	if (muteSetting->getValue()) ++muteCount;

	reloadDriver();
}

Mixer::~Mixer()
{
	assert(msxMixers.empty());
	driver.reset();

	soundDriverSetting->detach(*this);
	samplesSetting->detach(*this);
	frequencySetting->detach(*this);
	muteSetting->detach(*this);
}

void Mixer::reloadDriver()
{
	driver = make_unique<NullSoundDriver>();

	try {
		switch (soundDriverSetting->getValue()) {
		case SND_NULL:
			driver = make_unique<NullSoundDriver>();
			break;
		case SND_SDL:
			driver = make_unique<SDLSoundDriver>(
				reactor,
				frequencySetting->getValue(),
				samplesSetting->getValue());
			break;
#ifdef _WIN32
		case SND_DIRECTX:
			driver = make_unique<DirectXSoundDriver>(
				frequencySetting->getValue(),
				samplesSetting->getValue());
			break;
#endif
#if COMPONENT_AO
		case SND_LIBAO:
			driver = make_unique<LibAOSoundDriver>(
				frequencySetting->getValue(),
				samplesSetting->getValue());
			break;
#endif
		default:
			UNREACHABLE;
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
	unsigned samples = mute ? 0 : driver->getSamples();
	unsigned frequency = driver->getFrequency();
	for (auto& m : msxMixers) {
		m->setMixerParams(samples, frequency);
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

void Mixer::uploadBuffer(MSXMixer& /*msxMixer*/, short* buffer, unsigned len)
{
	// can only handle one MSXMixer ATM
	assert(!msxMixers.empty());

	driver->uploadBuffer(buffer, len);
}

void Mixer::update(const Setting& setting)
{
	if (&setting == muteSetting.get()) {
		if (muteSetting->getValue()) {
			mute();
		} else {
			unmute();
		}
	} else if ((&setting == samplesSetting.get()) ||
	           (&setting == soundDriverSetting.get()) ||
	           (&setting == frequencySetting.get())) {
		reloadDriver();
	} else {
		UNREACHABLE;
	}
}

} // namespace openmsx
