#include "Mixer.hh"
#include "MSXMixer.hh"
#include "NullSoundDriver.hh"
#include "SDLSoundDriver.hh"
#include "DirectXSoundDriver.hh"
#include "CommandController.hh"
#include "CliComm.hh"
#include "MSXException.hh"
#include "memory.hh"
#include "stl.hh"
#include "unreachable.hh"
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

static Mixer::SoundDriverType getDefaultSoundDriver()
{
#ifdef _WIN32
	return Mixer::SND_DIRECTX;
#else
	return Mixer::SND_SDL;
#endif
}

static EnumSetting<Mixer::SoundDriverType>::Map getSoundDriverMap()
{
	EnumSetting<Mixer::SoundDriverType>::Map soundDriverMap = {
		{ "null", Mixer::SND_NULL },
		{ "sdl",  Mixer::SND_SDL } };
#ifdef _WIN32
	soundDriverMap.emplace_back("directx", Mixer::SND_DIRECTX);
#endif
	return soundDriverMap;
}

Mixer::Mixer(Reactor& reactor_, CommandController& commandController_)
	: reactor(reactor_)
	, commandController(commandController_)
	, soundDriverSetting(
		commandController, "sound_driver",
		"select the sound output driver",
		getDefaultSoundDriver(), getSoundDriverMap())
	, muteSetting(
		commandController, "mute",
		"(un)mute the emulation sound", false, Setting::DONT_SAVE)
	, masterVolume(
		commandController, "master_volume",
		"master volume", 75, 0, 100)
	, frequencySetting(
		commandController, "frequency",
		"mixer frequency", 44100, 11025, 48000)
	, samplesSetting(
		commandController, "samples",
		"mixer samples", defaultsamples, 64, 8192)
	, muteCount(0)
{
	muteSetting       .attach(*this);
	frequencySetting  .attach(*this);
	samplesSetting    .attach(*this);
	soundDriverSetting.attach(*this);

	// Set correct initial mute state.
	if (muteSetting.getBoolean()) ++muteCount;

	reloadDriver();
}

Mixer::~Mixer()
{
	assert(msxMixers.empty());
	driver.reset();

	soundDriverSetting.detach(*this);
	samplesSetting    .detach(*this);
	frequencySetting  .detach(*this);
	muteSetting       .detach(*this);
}

void Mixer::reloadDriver()
{
	// Destroy old driver before attempting to create a new one. Though
	// this means we end up without driver if creating the new one failed
	// for some reason.

	driver = make_unique<NullSoundDriver>();

	try {
		switch (soundDriverSetting.getEnum()) {
		case SND_NULL:
			driver = make_unique<NullSoundDriver>();
			break;
		case SND_SDL:
			driver = make_unique<SDLSoundDriver>(
				reactor,
				frequencySetting.getInt(),
				samplesSetting.getInt());
			break;
#ifdef _WIN32
		case SND_DIRECTX:
			driver = make_unique<DirectXSoundDriver>(
				frequencySetting.getInt(),
				samplesSetting.getInt());
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
	assert(!contains(msxMixers, &mixer));
	msxMixers.push_back(&mixer);
	muteHelper();
}

void Mixer::unregisterMixer(MSXMixer& mixer)
{
	move_pop_back(msxMixers, rfind_unguarded(msxMixers, &mixer));
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
	bool isMuted = muteCount || msxMixers.empty();
	unsigned samples = isMuted ? 0 : driver->getSamples();
	unsigned frequency = driver->getFrequency();
	for (auto& m : msxMixers) {
		m->setMixerParams(samples, frequency);
	}

	if (isMuted) {
		driver->mute();
	} else {
		driver->unmute();
	}
}

void Mixer::uploadBuffer(MSXMixer& /*msxMixer*/, short* buffer, unsigned len)
{
	// can only handle one MSXMixer ATM
	assert(!msxMixers.empty());

	driver->uploadBuffer(buffer, len);
}

void Mixer::update(const Setting& setting)
{
	if (&setting == &muteSetting) {
		if (muteSetting.getBoolean()) {
			mute();
		} else {
			unmute();
		}
	} else if ((&setting == &samplesSetting) ||
	           (&setting == &soundDriverSetting) ||
	           (&setting == &frequencySetting)) {
		reloadDriver();
	} else {
		UNREACHABLE;
	}
}

} // namespace openmsx
