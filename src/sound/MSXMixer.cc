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
#include "StringSetting.hh"
#include "BooleanSetting.hh"
#include "AviRecorder.hh"
#include "Math.hh"
#include <algorithm>
#include <cmath>
#include <cstring>
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

void MSXMixer::registerSound(SoundDevice& device, double volume,
                             int balance, unsigned numChannels)
{
	// TODO read volume/balance(mode) from config file
	const string& name = device.getName();
	SoundDeviceInfo info;
	info.defaultVolume = volume;
	info.volumeSetting = new IntegerSetting(msxCommandController,
		name + "_volume", "the volume of this sound chip", 75, 0, 100);
	info.balanceSetting = new IntegerSetting(msxCommandController,
		name + "_balance", "the balance of this sound chip",
		balance, -100, 100);

	info.volumeSetting->attach(*this);
	info.balanceSetting->attach(*this);

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
	device.setOutputRate(sampleRate);

	Infos::iterator it = infos.find(&device);
	assert(it != infos.end());
	updateVolumeParams(it);
}

void MSXMixer::unregisterSound(SoundDevice& device)
{
	Infos::iterator it = infos.find(&device);
	assert(it != infos.end());
	it->second.volumeSetting->detach(*this);
	delete it->second.volumeSetting;
	it->second.balanceSetting->detach(*this);
	delete it->second.balanceSetting;
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
	assert((!muteCount && fragmentSize) || synchronousCounter);
	generate(mixBuffer, count, prevTime, interval1);
	prevTime += interval1 * count;
	double factor = 1.0;
	if (!muteCount && fragmentSize) {
		factor = mixer.uploadBuffer(*this, mixBuffer, count);
	}
	if (synchronousCounter) {
		if (recorder) {
			recorder->addWave(count, mixBuffer);
		}
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
	assert((!muteCount && fragmentSize) || synchronousCounter);
	assert(samples <= 8192);

	int mixBuf[2 * samples + 3];
	int tmpBuf[2 * samples + 3];
	memset(mixBuf, 0, 2 * samples * sizeof(int));

	for (Infos::const_iterator it = infos.begin();
	     it != infos.end(); ++it) {
		SoundDevice& device = *it->first;
		if (device.updateBuffer(samples, tmpBuf,
					start, sampDur)) {
			if (!device.isStereo()) {
				int l1 = it->second.left1;
				int r1 = it->second.right1;
				for (unsigned i = 0; i < samples; ++i) {
					mixBuf[2 * i + 0] += (l1 * tmpBuf[i]) >> 8;
					mixBuf[2 * i + 1] += (r1 * tmpBuf[i]) >> 8;
				}
			} else {
				int l1 = it->second.left1;
				int r1 = it->second.right1;
				int l2 = it->second.left2;
				int r2 = it->second.right2;
				for (unsigned i = 0; i < samples; ++i) {
					int in1 = tmpBuf[2 * i + 0];
					int in2 = tmpBuf[2 * i + 1];
					mixBuf[2 * i + 0] += (l1 * in1 + l2 * in2) >> 8;
					mixBuf[2 * i + 1] += (r1 * in1 + r2 * in2) >> 8;
				}
			}
		}
	}

	for (unsigned j = 0; j < samples; ++j) {
		int left  = mixBuf[2 * j + 0];
		int right = mixBuf[2 * j + 1];

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

		output[2 * j + 0] = Math::clipIntToShort(outLeft);
		output[2 * j + 1] = Math::clipIntToShort(outRight);
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
		for (Infos::const_iterator it = infos.begin();
		     it != infos.end(); ++it) {
			it->first->setOutputRate(sampleRate);
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
		updateMasterVolume();
	} else if (&setting == &speedSetting) {
		reInit();
	} else if (dynamic_cast<const IntegerSetting*>(&setting)) {
		Infos::iterator it = infos.begin();
		while (it != infos.end() &&
		       it->second.volumeSetting != &setting &&
		       it->second.balanceSetting != &setting) {
			++it;
		}
		assert(it != infos.end());
		updateVolumeParams(it);
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

void MSXMixer::updateVolumeParams(Infos::iterator it)
{
	SoundDeviceInfo& info = it->second;
	int mVolume = masterVolume.getValue();
	int dVolume = info.volumeSetting->getValue();
	double volume = info.defaultVolume * mVolume * dVolume / (100.0 * 100.0);
	int balance = info.balanceSetting->getValue();
	double l1, r1, l2, r2;
	if (it->first->isStereo()) {
		if (balance < 0) {
			double b = (balance + 100.0) / 100.0;
			l1 = volume;
			r1 = 0.0;
			l2 = volume * sqrt(std::max(0.0, 1.0 - b));
			r2 = volume * sqrt(std::max(0.0,       b));
		} else {
			double b = balance / 100.0;
			l1 = volume * sqrt(std::max(0.0, 1.0 - b));
			r1 = volume * sqrt(std::max(0.0,       b));
			l2 = 0.0;
			r2 = volume;
		}
	} else {
		// make sure that in case of rounding errors
		// we don't take sqrt() of negative numbers
		double b = (balance + 100.0) / 200.0;
		l1 = volume * sqrt(std::max(0.0, 1.0 - b));
		r1 = volume * sqrt(std::max(0.0,       b));
		l2 = r2 = 0.0; // dummy
	}
	info.left1  = int(l1 * 256);
	info.right1 = int(r1 * 256);
	info.left2  = int(l2 * 256);
	info.right2 = int(r2 * 256);
}

void MSXMixer::updateMasterVolume()
{
	for (Infos::iterator it = infos.begin(); it != infos.end(); ++it) {
		updateVolumeParams(it);
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

SoundDevice* MSXMixer::findDevice(const string& name) const
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
		SoundDevice* device = mixer.findDevice(tokens[2]->getString());
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
