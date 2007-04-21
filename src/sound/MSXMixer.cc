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
#include "StringSetting.hh"
#include "BooleanSetting.hh"
#include "AviRecorder.hh"
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
	, recorder(0)
	, synchronousCounter(0)
{
	sampleRate = 0;
	fragmentSize = 0;

	muteCount = 1;
	unmute(); // calls Mixer::registerMixer()

	reInit();

	masterVolume.attach(*this);
	speedSetting.attach(*this);
	throttleManager.attach(*this);
}

MSXMixer::~MSXMixer()
{
	if (recorder) {
		recorder->stop();
	}
	assert(infos.empty());

	throttleManager.detach(*this);
	speedSetting.detach(*this);
	masterVolume.detach(*this);

	mute(); // calls Mixer::unregisterMixer()
}

void MSXMixer::registerSound(SoundDevice& device, short volume,
                             ChannelMode::Mode mode, unsigned numChannels)
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

	for (unsigned i = 0; i < numChannels; ++i) {
		SoundDeviceInfo::ChannelSettings channelSettings;
		string ch_name = name + "_ch" + StringOp::toString(i + 1); 

		channelSettings.recordSetting = new StringSetting(
			msxCommandController, ch_name + "_record",
			"filename to record this channel to",
			"", Setting::DONT_SAVE);
		channelSettings.recordSetting->attach(*this);

		channelSettings.muteSetting = new BooleanSetting(
			msxCommandController, ch_name + "_mute",
			"sets mute-status of individual sound channels",
			false, Setting::DONT_SAVE);
		channelSettings.muteSetting->attach(*this);

		info.channelSettings.push_back(channelSettings);
	}

	infos[&device] = info;
	devices[mode].push_back(&device);
	device.setOutputRate(sampleRate);
	device.setVolume((info.normalVolume * info.volumeSetting->getValue() *
	                   masterVolume.getValue()) / (100 * 100));

	unsigned required = infos.size() * 8192 * 2;
	if (mixBuffer.size() < required) {
		mixBuffer.resize(required);
	}
}

void MSXMixer::unregisterSound(SoundDevice& device)
{
	// note: no need to shrink mixBuffer

	Infos::iterator it = infos.find(&device);
	assert(it != infos.end());

	ChannelMode::Mode mode = it->second.mode;
	vector<SoundDevice*> &dev = devices[mode];
	dev.erase(remove(dev.begin(), dev.end(), &device), dev.end());
	it->second.volumeSetting->detach(*this);
	delete it->second.volumeSetting;
	it->second.modeSetting->detach(*this);
	delete it->second.modeSetting;
	for (vector<SoundDeviceInfo::ChannelSettings>::const_iterator it2 =
	            it->second.channelSettings.begin();
	     it2 != it->second.channelSettings.end(); ++it2) {
		it2->recordSetting->detach(*this);
		delete it2->recordSetting;
		it2->muteSetting->detach(*this);
		delete it2->muteSetting;
	}
	infos.erase(it);
}

void MSXMixer::setSynchronousMode(bool synchronous)
{
	if (synchronous) {
		++synchronousCounter;
	} else {
		assert(synchronousCounter > 0);
		--synchronousCounter;
	}
}

void MSXMixer::updateStream(const EmuTime& time)
{
	if ((!muteCount && fragmentSize) || synchronousCounter) {
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
	double factor;
	if (!muteCount && fragmentSize) {
		generate(mixBuffer, count, prevTime, interval1);
		factor = mixer.uploadBuffer(*this, mixBuffer, count);
	} else {
		assert(synchronousCounter);
		memset(mixBuffer, 0, count * 2 * sizeof(short));
		factor = 1.0;
	}
	prevTime += interval1 * count;
	if (recorder) {
		recorder->addWave(count, mixBuffer);
		factor = 1.0;
	}
	if (factor != 1.0) {
		// check for 1.0 to avoid accumulating rounding errors
		factor = (factor + 15.0) / 16.0; // don't adjust too quickly
		interval1 *= factor;
		interval1 = std::min(interval1, interval1max);
		interval1 = std::max(interval1, interval1min);
		//std::cerr << "DEBUG interval1: " << interval1.toDouble() << std::endl;
	}
}

void MSXMixer::generate(short* output, unsigned samples,
	const EmuTime& start, const EmuDuration& sampDur)
{
	assert(!muteCount && fragmentSize);
	assert(samples <= 8192);

	int modeOffset[ChannelMode::NB_MODES];
	int unmuted = 0;
	int* buffer = &mixBuffer[0];
	for (int mode = 0; mode < ChannelMode::NB_MODES -1; mode++) { // -1 for OFF mode
		modeOffset[mode] = unmuted;
		for (vector<SoundDevice*>::const_iterator it =
		           devices[mode].begin();
		     it != devices[mode].end(); ++it) {
			if (!(*it)->isMuted()) {
				(*it)->updateBuffer(samples, buffer,
				                    start, sampDur);
				++unmuted;
				buffer += 8192 * 2;
			}
		}
	}

	for (unsigned j = 0; j < samples; ++j) {
		buffer = &mixBuffer[0];
		int buf = 0;
		int both = 0;
		while (buf < modeOffset[ChannelMode::MONO + 1]) {
			both  += buffer[j];
			buffer += 8192 * 2;
			++buf;
		}
		int left = both;
		while (buf < modeOffset[ChannelMode::MONO_LEFT + 1]) {
			left  += buffer[j];
			buffer += 8192 * 2;
			++buf;
		}
		int right = both;
		while (buf < modeOffset[ChannelMode::MONO_RIGHT + 1]) {
			right += buffer[j];
			buffer += 8192 * 2;
			++buf;
		}
		while (buf < unmuted) {
			left  += buffer[2 * j + 0];
			right += buffer[2 * j + 1];
			buffer += 8192 * 2;
			++buf;
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

		output[2 * j + 0] = static_cast<short>(outLeft);
		output[2 * j + 1] = static_cast<short>(outRight);
	}
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
	if (synchronousCounter) {
		// do as if speed=100
		interval1 = EmuDuration(1.0 / sampleRate);
	} else {
		double percent = speedSetting.getValue();
		interval1 = EmuDuration(percent / (sampleRate * 100));
	}
	interval1max = interval1 * 4;
	interval1min = interval1 / 4;

	removeSyncPoints();
	if (fragmentSize || !muteCount) {
		prevTime = getScheduler().getCurrentTime();
		EmuDuration interval2 = interval1 * fragmentSize;
		setSyncPoint(prevTime + interval2);
	} else if (synchronousCounter) {
		prevTime = getScheduler().getCurrentTime();
		EmuDuration interval2 = interval1 * 512;
		setSyncPoint(prevTime + interval2);
	}
}

void MSXMixer::setMixerParams(unsigned newFragmentSize, unsigned newSampleRate)
{
	bool needReInit = false;

	if (sampleRate != newSampleRate) {
		sampleRate = newSampleRate;
		needReInit = true;
		for (int mode = 0; mode < ChannelMode::NB_MODES; ++mode) {
			for (vector<SoundDevice*>::const_iterator it =
				 devices[mode].begin();
			     it != devices[mode].end(); ++it) {
				(*it)->setOutputRate(sampleRate);
			}
		}
	}

	if (fragmentSize != newFragmentSize) {
		fragmentSize = newFragmentSize;
		needReInit = true;
	}

	if (needReInit) reInit();
}

void MSXMixer::setRecorder(AviRecorder* newRecorder)
{
	if (bool(recorder) != bool(newRecorder)) {
		setSynchronousMode(newRecorder);
	}
	recorder = newRecorder;
	reInit();
}

unsigned MSXMixer::getSampleRate() const
{
	return sampleRate;
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
		ChannelMode::Mode oldmode = info.mode;
		info.mode = info.modeSetting->getValue();
		vector<SoundDevice*> &dev = devices[oldmode];
		dev.erase(remove(dev.begin(), dev.end(), it->first), dev.end());
		devices[info.mode].push_back(it->first);
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
	} else if (dynamic_cast<const StringSetting*>(&setting)) {
		changeRecordSetting(setting);
	} else if (dynamic_cast<const BooleanSetting*>(&setting)) {
		changeMuteSetting(setting);
	} else {
		assert(false);
	}
}

void MSXMixer::changeRecordSetting(const Setting& setting)
{
	for (Infos::iterator it = infos.begin(); it != infos.end(); ++it) {
		unsigned channel = 0;
		for (vector<SoundDeviceInfo::ChannelSettings>::const_iterator it2 =
			    it->second.channelSettings.begin();
		     it2 != it->second.channelSettings.end();
		     ++it2, ++channel) {
			if (it2->recordSetting == &setting) {
				it->first->recordChannel(
					channel, it2->recordSetting->getValue());
				return;
			}
		}
	}
	assert(false);
}

void MSXMixer::changeMuteSetting(const Setting& setting)
{
	for (Infos::iterator it = infos.begin(); it != infos.end(); ++it) {
		unsigned channel = 0;
		for (vector<SoundDeviceInfo::ChannelSettings>::const_iterator it2 =
			    it->second.channelSettings.begin();
		     it2 != it->second.channelSettings.end();
		     ++it2, ++channel) {
			if (it2->muteSetting == &setting) {
				it->first->muteChannel(
					channel, it2->muteSetting->getValue());
				return;
			}
		}
	}
	assert(false);
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
	if (!muteCount && fragmentSize) {
		updateStream2(time);
		EmuDuration interval2 = interval1 * fragmentSize;
		setSyncPoint(time + interval2);
	} else if (synchronousCounter) {
		updateStream2(time);
		EmuDuration interval2 = interval1 * 512;
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
