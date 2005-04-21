// $Id$

#include "SDLSoundDriver.hh"
#include "Mixer.hh"
#include "Scheduler.hh"
#include "GlobalSettings.hh"
#include "IntegerSetting.hh"
#include "BooleanSetting.hh"
#include "MSXException.hh"
#include "build-info.hh"
#include <algorithm>

using std::string;

namespace openmsx {

static int roundUpPower2(int a)
{
	int res = 1;
	while (a > res) res <<= 1;
	return res;
}

SDLSoundDriver::SDLSoundDriver(Mixer& mixer_,
                               unsigned frequency, unsigned samples)
	: mixer(mixer_)
	, muted(true)
	, speedSetting(GlobalSettings::instance().getSpeedSetting())
	, throttleSetting(GlobalSettings::instance().getThrottleSetting())
{
	SDL_AudioSpec desired;
	desired.freq     = frequency;
	desired.samples  = roundUpPower2(samples);
	desired.channels = 2; // stereo
	desired.format   = OPENMSX_BIGENDIAN ? AUDIO_S16MSB : AUDIO_S16LSB;
	desired.callback = audioCallbackHelper; // must be a static method
	desired.userdata = this;
	
	if (SDL_InitSubSystem(SDL_INIT_AUDIO) != 0) {
		throw MSXException("Unable to initialize SDL audio subsystem: " +
		                   string(SDL_GetError()));
	}
	if (SDL_OpenAudio(&desired, &audioSpec) != 0) {
		SDL_QuitSubSystem(SDL_INIT_AUDIO);
		throw MSXException("Unable to open SDL audio: " +
		                   string(SDL_GetError()));
	}

	bufferSize = 4 * audioSpec.size / (2 * sizeof(short));
	mixBuffer = new short[2 * bufferSize];
	memset(mixBuffer, 0, bufferSize * 2 * sizeof(short));
	readPtr = writePtr = 0;
	reInit();
	prevTime = Scheduler::instance().getCurrentTime();
	EmuDuration interval2 = interval1 * audioSpec.samples;
	Scheduler::instance().setSyncPoint(prevTime + interval2, this);
	
	speedSetting.addListener(this);
	throttleSetting.addListener(this);
}

SDLSoundDriver::~SDLSoundDriver()
{
	throttleSetting.removeListener(this);
	speedSetting.removeListener(this);
	
	Scheduler::instance().removeSyncPoint(this);
	
	SDL_CloseAudio();
	SDL_QuitSubSystem(SDL_INIT_AUDIO);
}

void SDLSoundDriver::lock()
{
	SDL_LockAudio();
}

void SDLSoundDriver::unlock()
{
	SDL_UnlockAudio();
}

void SDLSoundDriver::mute()
{
	if (!muted) {
		muted = true;
		SDL_PauseAudio(1);
	}
}

void SDLSoundDriver::unmute()
{
	if (muted) {
		muted = false;
		SDL_PauseAudio(0);
		reInit();
	}
}

unsigned SDLSoundDriver::getFrequency() const
{
	return audioSpec.freq;
}

unsigned SDLSoundDriver::getSamples() const
{
	return audioSpec.samples;
}

void SDLSoundDriver::audioCallbackHelper(void* userdata, Uint8* strm, int len)
{
	((SDLSoundDriver*)userdata)->
		audioCallback((short*)strm, len / (2 * sizeof(short)));
}

void SDLSoundDriver::audioCallback(short* stream, unsigned len)
{
	unsigned available = (readPtr <= writePtr)
	                   ? writePtr - readPtr
	                   : writePtr - readPtr + bufferSize;
	
	if (available < (3 * bufferSize / 8)) {
		int missing = len - available;
		if (missing <= 0) {
			// 1/4 full, speed up a little
			if (interval1.length() > 100) { // may not become 0
				interval1 /= 1.005;
			}
			//cout << "Mixer: low      " << available << '/' << len << ' '
			//     << 1.0 / interval1.toDouble() << endl;
		} else {
			// buffer underrun
			if (interval1.length() > 100) { // may not become 0
				interval1 /= 1.01;
			}
			//cout << "Mixer: underrun " << available << '/' << len << ' '
			//     << 1.0 / interval1.toDouble() << endl;
			updtStrm2(missing);
		}
		EmuDuration minDuration = (intervalAverage * 255) / 256;
		if (interval1 < minDuration) {
			interval1 = minDuration;
			//cout << "Mixer: clipped  " << available << '/' << len << ' '
			//     << 1.0 / interval1.toDouble() << endl;
		}
	}
	if ((readPtr + len) < bufferSize) {
		memcpy(stream, &mixBuffer[2 * readPtr], len * 2 * sizeof(short));
		readPtr += len;
	} else {
		unsigned len1 = bufferSize - readPtr;
		memcpy(stream, &mixBuffer[2 * readPtr], len1 * 2 * sizeof(short));
		unsigned len2 = len - len1;
		memcpy(&stream[2 * len1], mixBuffer, len2 * 2 * sizeof(short));
		readPtr = len2;
	}
	intervalAverage = (intervalAverage * 63 + interval1) / 64;
}


void SDLSoundDriver::updateStream(const EmuTime& time)
{
	assert(prevTime <= time);
	EmuDuration duration = time - prevTime;
	unsigned samples = duration / interval1;
	if (samples == 0) {
		return;
	}
	prevTime += interval1 * samples;
	
	lock();
	updtStrm(samples);
	unlock();
	
}

void SDLSoundDriver::updtStrm(unsigned samples)
{
	samples = std::min<unsigned>(samples, audioSpec.samples);
	
	unsigned available = (readPtr <= writePtr)
	                   ? writePtr - readPtr
	                   : writePtr - readPtr + bufferSize;
	available += samples;
	if (available > (7 * bufferSize / 8)) {
		int overflow = available - (bufferSize - 1);
		if (overflow <= 0) {
			// 7/8 full slow down a bit
			interval1 *= 1.005;
			//cout << "Mixer: high     " << available << '/' << bufferSize << ' '
			//     << 1.0 / interval1.toDouble() << endl;
		} else {
			// buffer overrun
			interval1 *= 1.01;
			//cout << "Mixer: overrun  " << available << '/' << bufferSize << ' '
			//     << 1.0 / interval1.toDouble() << endl;
			samples -= overflow;
		}
		EmuDuration maxDuration = (intervalAverage * 257) / 256;
		if (interval1 > maxDuration) {
			interval1 = maxDuration;
			//cout << "Mixer: clipped  " << available << '/' << bufferSize << ' '
			//     << 1.0 / interval1.toDouble() << endl;
		}
	}
	updtStrm2(samples);
}

void SDLSoundDriver::updtStrm2(unsigned samples)
{
	unsigned left = bufferSize - writePtr;
	if (samples < left) {
		mixer.generate(&mixBuffer[2 * writePtr], samples);
		writePtr += samples;
	} else {
		mixer.generate(&mixBuffer[2 * writePtr], left);
		writePtr = samples - left;
		if (writePtr > 0) {
			mixer.generate(mixBuffer, writePtr);
		}
	}
}

void SDLSoundDriver::reInit()
{
	double percent = speedSetting.getValue();
	interval1 = EmuDuration(percent / (audioSpec.freq * 100));
	intervalAverage = interval1; 
}



// Schedulable

void SDLSoundDriver::executeUntil(const EmuTime& time, int /*userData*/)
{
	if (!muted) {
		// TODO not schedule at all if muted
		updateStream(time);
	}
	EmuDuration interval2 = interval1 * audioSpec.samples;
	Scheduler::instance().setSyncPoint(time + interval2, this);
}

const string& SDLSoundDriver::schedName() const
{
	static const string name = "SDLSoundDriver";
	return name;
}


// SettingListener

void SDLSoundDriver::update(const Setting* setting)
{
	if (setting == &speedSetting) {
		reInit();
	} else if (setting == &throttleSetting) {
		if (throttleSetting.getValue()) {
			reInit();
		}
	} else {
		assert(false);
	}
}

} // namespace openmsx
