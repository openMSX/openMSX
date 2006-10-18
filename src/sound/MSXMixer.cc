// $Id$

#include "MSXMixer.hh"
#include "Mixer.hh"
#include "SoundDevice.hh"
#include "MSXCommandController.hh"
#include "Scheduler.hh"
#include "InfoTopic.hh"
#include "TclObject.hh"
#include "ThrottleManager.hh"
#include "GlobalSettings.hh"
#include "IntegerSetting.hh"
#include "EnumSetting.hh"
#include <algorithm>
#include <cassert>

using std::remove;
using std::set;
using std::string;
using std::vector;

namespace openmsx {

class SoundDeviceInfoTopic : public InfoTopic
{
public:
	SoundDeviceInfoTopic(InfoCommand& machineInfoCommand, MSXMixer& mixer);
	virtual void execute(const vector<TclObject*>& tokens,
	                     TclObject& result) const;
	virtual string help(const vector<string>& tokens) const;
	virtual void tabCompletion(vector<string>& tokens) const;
private:
	MSXMixer& mixer;
};


MSXMixer::MSXMixer(Mixer& mixer_, Scheduler& scheduler,
                   MSXCommandController& msxCommandController_)
	: Schedulable(scheduler)
	, mixer(mixer_)
	, msxCommandController(msxCommandController_)
	, masterVolume(mixer.getMasterVolume())
	, speedSetting(
		msxCommandController.getGlobalSettings().getSpeedSetting())
	, throttleManager(
		msxCommandController.getGlobalSettings().getThrottleManager())
	, prevTime(EmuTime::zero)
	, soundDeviceInfo(new SoundDeviceInfoTopic(
	              msxCommandController.getMachineInfoCommand(), *this))
{
	sampleRate = 0;
	bufferSize = 0;

	muteCount = 1;
	unmute(); // calls Mixer::registerMixer()

	reInit();

	masterVolume.attach(*this);
	speedSetting.attach(*this);
	throttleManager.attach(*this);
}

MSXMixer::~MSXMixer()
{
	assert(buffers.empty());

	throttleManager.detach(*this);
	speedSetting.detach(*this);
	masterVolume.detach(*this);

	mute(); // calls Mixer::unregisterMixer()
}

void MSXMixer::registerSound(SoundDevice& device, short volume,
                             ChannelMode::Mode mode)
{
	const string& name = device.getName();
	SoundDeviceInfo info;
	info.volumeSetting = new IntegerSetting(msxCommandController,
		name + "_volume", "the volume of this sound chip", 75, 0, 100);

	// once we're stereo, stay stereo. Once mono, stay mono.
	// we could also choose not to offer any modeSetting in case we have
	// a stereo mode initially. You can't query the mode then, though.
	string defaultMode;
	EnumSetting<ChannelMode::Mode>::Map modeMap;
	if (mode == ChannelMode::STEREO) {
		defaultMode = "stereo";
		modeMap[defaultMode] = ChannelMode::STEREO;
	} else {
		defaultMode = "mono";
		modeMap[defaultMode] = ChannelMode::MONO;
		modeMap["left"] = ChannelMode::MONO_LEFT;
		modeMap["right"] = ChannelMode::MONO_RIGHT;
	}
	modeMap["off"] = ChannelMode::OFF;
	info.modeSetting = new EnumSetting<ChannelMode::Mode>(msxCommandController,
		name + "_mode", "the channel mode of this sound chip",
		modeMap[defaultMode], modeMap, Setting::DONT_SAVE);
	info.modeSetting->setValue(mode);

	info.mode = mode;
	info.normalVolume = (volume * 100 * 100) / (75 * 75);
	info.modeSetting->attach(*this);
	info.volumeSetting->attach(*this);
	infos[&device] = info;

	lock();
	devices[mode].push_back(&device);
	device.setSampleRate(sampleRate);
	device.setVolume((info.normalVolume * info.volumeSetting->getValue() *
	                   masterVolume.getValue()) / (100 * 100));
	buffers.push_back(bufferSize ? new int[2 * bufferSize] : NULL); // left + right
	unlock();
}

void MSXMixer::unregisterSound(SoundDevice& device)
{
	Infos::iterator it = infos.find(&device);
	if (it == infos.end()) {
		return;
	}

	lock();

	delete[] buffers.back();
	buffers.pop_back();

	ChannelMode::Mode mode = it->second.mode;
	vector<SoundDevice*> &dev = devices[mode];
	dev.erase(remove(dev.begin(), dev.end(), &device), dev.end());
	it->second.volumeSetting->detach(*this);
	delete it->second.volumeSetting;
	it->second.modeSetting->detach(*this);
	delete it->second.modeSetting;
	infos.erase(it);

	unlock();
}

void MSXMixer::updateStream(const EmuTime& time)
{
	if (!muteCount && bufferSize) {
		updateStream2(time);
	}
}

void MSXMixer::updateStream2(const EmuTime& time)
{
	assert(prevTime <= time);
	EmuDuration duration = time - prevTime;
	unsigned count = duration / interval1;
	if (count == 0) return;

	short mixBuffer[8192 * 2];
	count = std::min(8192u, count);
	lock();
	generate(mixBuffer, count, prevTime, interval1);
	double factor = mixer.uploadBuffer(*this, mixBuffer, count);
	unlock();
	prevTime += interval1 * count;

	factor = (factor + 31.0) / 32.0; // don't adjust too quickly
	interval1 *= factor;
	EmuDuration minDuration = (intervalAverage * 255) / 256;
	EmuDuration maxDuration = (intervalAverage * 257) / 256;
	if (interval1 < minDuration) {
		interval1 = minDuration;
	} else if (interval1 > maxDuration) {
		interval1 = maxDuration;
	}
	intervalAverage = (intervalAverage * 63 + interval1) / 64;
}

void MSXMixer::bufferUnderRun(short* buffer, unsigned samples)
{
	// Note: runs in audio thread
	if (!muteCount && bufferSize) {
		generate(buffer, samples, prevTime, interval1);
	} else {
		// muted
		memset(buffer, 0, samples * 2 * sizeof(short));
	}
}

void MSXMixer::generate(short* buffer, unsigned samples,
	const EmuTime& start, const EmuDuration& sampDur)
{
	assert(!muteCount && bufferSize);

	int modeOffset[ChannelMode::NB_MODES];
	int unmuted = 0;
	for (int mode = 0; mode < ChannelMode::NB_MODES -1; mode++) { // -1 for OFF mode
		modeOffset[mode] = unmuted;
		for (vector<SoundDevice*>::const_iterator it =
		           devices[mode].begin();
		     it != devices[mode].end(); ++it) {
			if (!(*it)->isMuted()) {
				(*it)->updateBuffer(samples, buffers[unmuted++],
				                    start, sampDur);
			}
		}
	}

	for (unsigned j = 0; j < samples; ++j) {
		int buf = 0;
		int both = 0;
		while (buf < modeOffset[ChannelMode::MONO+1]) {
			both  += buffers[buf++][j];
		}
		int left = both;
		while (buf < modeOffset[ChannelMode::MONO_LEFT+1]) {
			left  += buffers[buf++][j];
		}
		int right = both;
		while (buf < modeOffset[ChannelMode::MONO_RIGHT+1]) {
			right += buffers[buf++][j];
		}
		while (buf < unmuted) {
			left  += buffers[buf]  [2*j+0];
			right += buffers[buf++][2*j+1];
		}

		// DC removal filter
		//   y(n) = x(n) - x(n-1) + R * y(n-1)
		//   R = 1 - (pi*2 * cut-off-frequency / samplerate)
		// take R = 1022/1024
		//   44100Hz --> cutt-off freq = 14Hz
		//   22050Hz                     7Hz
		outLeft   =  left -  prevLeft + ((1022 *  outLeft) >> 10);
		prevLeft  =  left;
		outRight  = right - prevRight + ((1022 * outRight) >> 10);
		prevRight = right;

		// clip
		if      (outLeft  > 32767)  outLeft  =  32767;
		else if (outLeft  < -32768) outLeft  = -32768;
		if      (outRight > 32767)  outRight =  32767;
		else if (outRight < -32768) outRight = -32768;

		buffer[2 * j + 0] = static_cast<short>(outLeft);
		buffer[2 * j + 1] = static_cast<short>(outRight);
	}
}

void MSXMixer::lock()
{
	mixer.lock();
}

void MSXMixer::unlock()
{
	mixer.unlock();
}

void MSXMixer::mute()
{
	if (muteCount == 0) {
		mixer.unregisterMixer(*this);
		reInit();
	}
	++muteCount;
}

void MSXMixer::unmute()
{
	--muteCount;
	if (muteCount == 0) {
		prevLeft = outLeft = 0;
		prevRight = outRight = 0;
		mixer.registerMixer(*this);
		reInit();
	}
}

void MSXMixer::reInit()
{
	double percent = speedSetting.getValue();
	interval1 = EmuDuration(percent / (sampleRate * 100));
	intervalAverage = interval1;

	removeSyncPoints();
	if (bufferSize || !muteCount) {
		prevTime = getScheduler().getCurrentTime();
		EmuDuration interval2 = interval1 * bufferSize;
		setSyncPoint(prevTime + interval2);
	}
}

void MSXMixer::setMixerParams(unsigned newBufferSize, unsigned newSampleRate)
{
	bool needReInit = false;

	if (sampleRate != newSampleRate) {
		sampleRate = newSampleRate;
		needReInit = true;
		for (int mode = 0; mode < ChannelMode::NB_MODES; ++mode) {
			for (vector<SoundDevice*>::const_iterator it =
				 devices[mode].begin();
			     it != devices[mode].end(); ++it) {
				(*it)->setSampleRate(sampleRate);
			}
		}
	}

	if (bufferSize != newBufferSize) {
		bufferSize = newBufferSize;
		needReInit = true;
		int numBuffers = buffers.size();
		for (int i = 0; i < numBuffers; ++i) {
			delete[] buffers[i];
			buffers[i] = bufferSize ? new int[2 * bufferSize] // left + right
			                        : NULL;
		}               
	}

	if (needReInit) reInit();
}

void MSXMixer::update(const Setting& setting)
{
	if (&setting == &masterVolume) {
		updateMasterVolume(masterVolume.getValue());
	} else if (&setting == &speedSetting) {
		reInit();
	} else if (dynamic_cast<const EnumSetting<ChannelMode::Mode>* >(&setting)) {
		Infos::iterator it = infos.begin();
		while (it != infos.end() && it->second.modeSetting != &setting) {
			++it;
		}
		assert(it != infos.end());
		SoundDeviceInfo &info = it->second;
		lock();
		ChannelMode::Mode oldmode = info.mode;
		info.mode = info.modeSetting->getValue();
		vector<SoundDevice*> &dev = devices[oldmode];
		dev.erase(remove(dev.begin(), dev.end(), it->first), dev.end());
		devices[info.mode].push_back(it->first);
		unlock();
	} else if (dynamic_cast<const IntegerSetting*>(&setting)) {
		Infos::iterator it = infos.begin();
		while (it != infos.end() && it->second.volumeSetting != &setting) {
			++it;
		}
		assert(it != infos.end());
		const SoundDeviceInfo& info = it->second;
		it->first->setVolume(
		     (masterVolume.getValue() * info.volumeSetting->getValue() *
		      info.normalVolume) / (100 * 100));
	} else {
		assert(false);
	}
}

void MSXMixer::update(const ThrottleManager& /*throttleManager*/)
{
	reInit();
}

// 0 <= mastervolume <= 100
void MSXMixer::updateMasterVolume(int masterVolume)
{
	for (Infos::const_iterator it = infos.begin();
	     it != infos.end(); ++it) {
		const SoundDeviceInfo& info = it->second;
		it->first->setVolume(
		     (info.normalVolume * info.volumeSetting->getValue() *
		      masterVolume) / (100 * 100));
	}
}

void MSXMixer::executeUntil(const EmuTime& time, int /*userData*/)
{
	if (!muteCount && bufferSize) {
		updateStream2(time);
		EmuDuration interval2 = interval1 * bufferSize;
		setSyncPoint(time + interval2);
	}
}

const std::string& MSXMixer::schedName() const
{
	static const string name = "MSXMixer";
	return name;
}


// Sound device info

SoundDevice* MSXMixer::getSoundDevice(const string& name)
{
	for (Infos::const_iterator it = infos.begin();
	     it != infos.end(); ++it) {
		if (it->first->getName() == name) {
			return it->first;
		}
	}
	return NULL;
}

SoundDeviceInfoTopic::SoundDeviceInfoTopic(
		InfoCommand& machineInfoCommand, MSXMixer& mixer_)
	: InfoTopic(machineInfoCommand, "sounddevice")
	, mixer(mixer_)
{
}

void SoundDeviceInfoTopic::execute(const vector<TclObject*>& tokens,
	TclObject& result) const
{
	switch (tokens.size()) {
	case 2:
		for (MSXMixer::Infos::const_iterator it = mixer.infos.begin();
		     it != mixer.infos.end(); ++it) {
			result.addListElement(it->first->getName());
		}
		break;
	case 3: {
		SoundDevice* device = mixer.getSoundDevice(tokens[2]->getString());
		if (!device) {
			throw CommandException("Unknown sound device");
		}
		result.setString(device->getDescription());
		break;
	}
	default:
		throw CommandException("Too many parameters");
	}
}

string SoundDeviceInfoTopic::help(const vector<string>& /*tokens*/) const
{
	return "Shows a list of available sound devices.\n";
}

void SoundDeviceInfoTopic::tabCompletion(vector<string>& tokens) const
{
	if (tokens.size() == 3) {
		set<string> devices;
		for (MSXMixer::Infos::const_iterator it =
		       mixer.infos.begin(); it != mixer.infos.end(); ++it) {
			devices.insert(it->first->getName());
		}
		completeString(tokens, devices);
	}
}

} // namespace openmsx
