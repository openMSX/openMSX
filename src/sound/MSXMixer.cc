// $Id$

#include "MSXMixer.hh"
#include "Mixer.hh"
#include "SoundDevice.hh"
#include "MSXCommandController.hh"
#include "InfoTopic.hh"
#include "TclObject.hh"
#include "ThrottleManager.hh"
#include "GlobalSettings.hh"
#include "IntegerSetting.hh"
#include "StringSetting.hh"
#include "BooleanSetting.hh"
#include "CommandException.hh"
#include "AviRecorder.hh"
#include "Filename.hh"
#include "CliComm.hh"
#include "Math.hh"
#include "StringOp.hh"
#include "vla.hh"
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
	, commandController(msxCommandController_)
	, masterVolume(mixer.getMasterVolume())
	, speedSetting(
		commandController.getGlobalSettings().getSpeedSetting())
	, throttleManager(
		commandController.getGlobalSettings().getThrottleManager())
	, prevTime(EmuTime::zero)
	, soundDeviceInfo(new SoundDeviceInfoTopic(
	              msxCommandController_.getMachineInfoCommand(), *this))
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
	info.volumeSetting = new IntegerSetting(commandController,
		name + "_volume", "the volume of this sound chip", 75, 0, 100);
	info.balanceSetting = new IntegerSetting(commandController,
		name + "_balance", "the balance of this sound chip",
		balance, -100, 100);

	info.volumeSetting->attach(*this);
	info.balanceSetting->attach(*this);

	for (unsigned i = 0; i < numChannels; ++i) {
		SoundDeviceInfo::ChannelSettings channelSettings;
		string ch_name = name + "_ch" + StringOp::toString(i + 1);

		channelSettings.recordSetting = new StringSetting(
			commandController, ch_name + "_record",
			"filename to record this channel to",
			"", Setting::DONT_SAVE);
		channelSettings.recordSetting->attach(*this);

		channelSettings.muteSetting = new BooleanSetting(
			commandController, ch_name + "_mute",
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

	commandController.getCliComm().update(CliComm::SOUNDDEVICE, device.getName(), "add");
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
	commandController.getCliComm().update(CliComm::SOUNDDEVICE, device.getName(), "remove");
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

void MSXMixer::updateStream(EmuTime::param time)
{
	if ((!muteCount && fragmentSize) || synchronousCounter) {
		updateStream2(time);
	}
}

void MSXMixer::updateStream2(EmuTime::param time)
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
	EmuTime::param start, EmuDuration::param sampDur)
{
	// The code below is specialized for a lot of cases (before this
	// routine was _much_ shorter). This is done because this routine
	// ends up relatively high (top 5) in a profile run.
	// After these specialization this routine runs about two times
	// faster for the common cases (mono output or no sound at all).
	// In total emulation time this gave a speedup of about 2%.

	assert((!muteCount && fragmentSize) || synchronousCounter);
	assert(samples <= 8192);

	if (samples == 0) return;

	VLA(int, stereoBuf, 2 * samples + 3);
	VLA(int, monoBuf, samples + 3);
	VLA_ALIGNED(int, tmpBuf, 2 * samples + 3, 16);

	static const unsigned HAS_MONO_FLAG = 1;
	static const unsigned HAS_STEREO_FLAG = 2;
	unsigned usedBuffers = 0;

	for (Infos::const_iterator it = infos.begin();
	     it != infos.end(); ++it) {
		SoundDevice& device = *it->first;
		if (device.updateBuffer(samples, tmpBuf,
					start, sampDur)) {
			if (!device.isStereo()) {
				int l1 = it->second.left1;
				int r1 = it->second.right1;
				if (l1 == r1) {
					if (!(usedBuffers & HAS_MONO_FLAG)) {
						usedBuffers |= HAS_MONO_FLAG;
#ifdef __arm__
						asm volatile (
						"0:\n\t"
							"ldmia	%[in]!,{r4-r7}\n\t"
							"mul	r4,%[f],r4\n\t"
							"mul	r5,%[f],r5\n\t"
							"mul	r6,%[f],r6\n\t"
							"mul	r7,%[f],r7\n\t"
							"stmia	%[out]!,{r4-r7}\n\t"
							"subs	%[n],%[n],#4\n\t"
							"bgt	0b\n\t"
							: // no output
							: [in]  "r" (tmpBuf)
							, [out] "r" (monoBuf)
							, [f]   "r" (l1)
							, [n]   "r" (samples)
							: "r4","r5","r6","r7"
						);
#else
						for (unsigned i = 0; i < samples; ++i) {
							int tmp = l1 * tmpBuf[i];
							monoBuf[i] = tmp;
						}
#endif
					} else {
#ifdef __arm__
						asm volatile (
						"0:\n\t"
							"ldmia	%[in]!,{r3-r6}\n\t"
							"ldmia	%[out],{r7-r10}\n\t"
							"mla	r3,%[f],r3,r7\n\t"
							"mla	r4,%[f],r4,r8\n\t"
							"mla	r5,%[f],r5,r9\n\t"
							"mla	r6,%[f],r6,r10\n\t"
							"stmia	%[out]!,{r3-r6}\n\t"
							"subs	%[n],%[n],#4\n\t"
							"bgt	0b\n\t"
							: // no output
							: [in]  "r" (tmpBuf)
							, [out] "r" (monoBuf)
							, [f]   "r" (l1)
							, [n]   "r" (samples)
							: "r3","r4","r5","r6"
							, "r7","r8","r9","r10"
						);
#else
						for (unsigned i = 0; i < samples; ++i) {
							int tmp = l1 * tmpBuf[i];
							monoBuf[i] += tmp;
						}
#endif
					}
				} else {
					if (!(usedBuffers & HAS_STEREO_FLAG)) {
						usedBuffers |= HAS_STEREO_FLAG;
						for (unsigned i = 0; i < samples; ++i) {
							int l = l1 * tmpBuf[i];
							int r = r1 * tmpBuf[i];
							stereoBuf[2 * i + 0] = l;
							stereoBuf[2 * i + 1] = r;
						}
					} else {
						for (unsigned i = 0; i < samples; ++i) {
							int l = l1 * tmpBuf[i];
							int r = r1 * tmpBuf[i];
							stereoBuf[2 * i + 0] += l;
							stereoBuf[2 * i + 1] += r;
						}
					}
				}
			} else {
				int l1 = it->second.left1;
				int r1 = it->second.right1;
				int l2 = it->second.left2;
				int r2 = it->second.right2;
				if (!(usedBuffers & HAS_STEREO_FLAG)) {
					usedBuffers |= HAS_STEREO_FLAG;
					for (unsigned i = 0; i < samples; ++i) {
						int in1 = tmpBuf[2 * i + 0];
						int in2 = tmpBuf[2 * i + 1];
						int l = l1 * in1 + l2 * in2;
						int r = r1 * in1 + r2 * in2;
						stereoBuf[2 * i + 0] = l;
						stereoBuf[2 * i + 1] = r;
					}
				} else {
					for (unsigned i = 0; i < samples; ++i) {
						int in1 = tmpBuf[2 * i + 0];
						int in2 = tmpBuf[2 * i + 1];
						int l = l1 * in1 + l2 * in2;
						int r = r1 * in1 + r2 * in2;
						stereoBuf[2 * i + 0] += l;
						stereoBuf[2 * i + 1] += r;
					}
				}
			}
		}
	}

	// DC removal filter
	//   y(n) = x(n) - x(n-1) + R * y(n-1)
	//   R = 1 - (pi*2 * cut-off-frequency / samplerate)
	// take R = 511/512
	//   44100Hz --> cutt-off freq = 14Hz
	//   22050Hz                     7Hz
	// Note: we divide by 512 iso shift-right by 9 because we want
	//       to round towards zero.
	switch (usedBuffers) {
	case 0:
		// no new input
		if ((outLeft == outRight) && (prevLeft == prevRight)) {
			if ((outLeft == 0) && (prevLeft == 0)) {
				// output was already zero, after DC-filter
				// it will still be zero
				memset(output, 0, 2 * samples * sizeof(short));
			} else {
				// output was not zero, but it was the same
				// left and right
				assert(samples > 0);
				outLeft  = -prevLeft + ((511 * outLeft) / 512);
				prevLeft = 0;
				short out = Math::clipIntToShort(outLeft);
				output[0] = out;
				output[1] = out;
				for (unsigned j = 1; j < samples; ++j) {
					outLeft = ((511 * outLeft) / 512);
					out = Math::clipIntToShort(outLeft);
					output[2 * j + 0] = out;
					output[2 * j + 1] = out;
				}
			}
			outRight = outLeft;
			prevRight = prevLeft;
		} else {
			assert(samples > 0);
			outLeft   = -prevLeft  + ((511 * outLeft ) / 512);
			outRight  = -prevRight + ((511 * outRight) / 512);
			prevLeft  = 0;
			prevRight = 0;
			output[0] = Math::clipIntToShort(outLeft);
			output[1] = Math::clipIntToShort(outRight);
			for (unsigned j = 1; j < samples; ++j) {
				outLeft   = ((511 *  outLeft) / 512);
				outRight  = ((511 * outRight) / 512);
				output[2 * j + 0] = Math::clipIntToShort(outLeft);
				output[2 * j + 1] = Math::clipIntToShort(outRight);
			}
		}
		break;

	case HAS_MONO_FLAG:
		// only mono
		if ((outLeft == outRight) && (prevLeft == prevRight)) {
			// previous output was also mono
#ifdef __arm__
			// Note: there are two functional differences in the
			//       asm and c++ code below:
			//  - divide by 512 is replaced by ASR #9
			//    (different for negative numbers)
			//  - the outLeft variable is set to the clipped value
			// Though this difference is very small, and we need
			// the extra speed.
			unsigned dummy;
			asm volatile (
			"0:\n\t"
				"rsb	%[o],%[o],%[o],LSL #9\n\t"
				"rsb	%[o],%[p],%[o],ASR #9\n\t"
				"ldr	%[p],[%[in]],#4\n\t"
				"mov	%[p],%[p],ASR #8\n\t"
				"add	%[o],%[o],%[p]\n\t"
				"mov	%[t],%[o],LSL #16\n\t"
				"cmp	%[o],%[t],ASR #16\n\t"
				"subne	%[o],%[m],%[o],ASR #31\n\t"
				"strh	%[o],[%[out]],#2\n\t"
				"strh	%[o],[%[out]],#2\n\t"
				"subs	%[n],%[n],#1\n\t"
				"bne	0b\n\t"
				: [o]   "=r"  (outLeft)
				, [p]   "=r"  (prevLeft)
				, [t]   "=&r" (dummy)
				:       "[o]" (outLeft)
				,       "[p]" (prevLeft)
				, [in]  "r"   (monoBuf)
				, [out] "r"   (output)
				, [m]   "r"   (0x7FFF)
				, [n]   "r"   (samples)
			);
#else
			for (unsigned j = 0; j < samples; ++j) {
				int mono = monoBuf[j] >> 8;
				outLeft   = mono -  prevLeft + ((511 *  outLeft) / 512);
				prevLeft  = mono;
				short out = Math::clipIntToShort(outLeft);
				output[2 * j + 0] = out;
				output[2 * j + 1] = out;
			}
#endif
			outRight = outLeft;
			prevRight = prevLeft;
		} else {
			for (unsigned j = 0; j < samples; ++j) {
				int mono = monoBuf[j] >> 8;
				outLeft   = mono -  prevLeft + ((511 *  outLeft) / 512);
				prevLeft  = mono;
				outRight  = mono - prevRight + ((511 * outRight) / 512);
				prevRight = mono;
				output[2 * j + 0] = Math::clipIntToShort(outLeft);
				output[2 * j + 1] = Math::clipIntToShort(outRight);
			}
		}
		break;

	case HAS_STEREO_FLAG:
		// only stereo
		for (unsigned j = 0; j < samples; ++j) {
			int left  = stereoBuf[2 * j + 0] >> 8;
			int right = stereoBuf[2 * j + 1] >> 8;
			outLeft   =  left -  prevLeft + ((511 *  outLeft) / 512);
			prevLeft  =  left;
			outRight  = right - prevRight + ((511 * outRight) / 512);
			prevRight = right;
			output[2 * j + 0] = Math::clipIntToShort(outLeft);
			output[2 * j + 1] = Math::clipIntToShort(outRight);
		}
		break;

	default:
		// mono + stereo
		for (unsigned j = 0; j < samples; ++j) {
			int mono = monoBuf[j] >> 8;
			int left  = (stereoBuf[2 * j + 0] >> 8) + mono;
			int right = (stereoBuf[2 * j + 1] >> 8) + mono;
			outLeft   =  left -  prevLeft + ((511 *  outLeft) / 512);
			prevLeft  =  left;
			outRight  = right - prevRight + ((511 * outRight) / 512);
			prevRight = right;
			output[2 * j + 0] = Math::clipIntToShort(outLeft);
			output[2 * j + 1] = Math::clipIntToShort(outRight);
		}
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

void MSXMixer::reschedule()
{
	reInit();
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
		prevTime = getCurrentTime();
		EmuDuration interval2 = interval1 * fragmentSize;
		setSyncPoint(prevTime + interval2);
	} else if (synchronousCounter) {
		prevTime = getCurrentTime();
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
					channel,
					Filename(it2->recordSetting->getValue()));
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
	int amp = 256 * it->first->getAmplificationFactor();
	info.left1  = int(l1 * amp);
	info.right1 = int(r1 * amp);
	info.left2  = int(l2 * amp);
	info.right2 = int(r2 * amp);
}

void MSXMixer::updateMasterVolume()
{
	for (Infos::iterator it = infos.begin(); it != infos.end(); ++it) {
		updateVolumeParams(it);
	}
}

void MSXMixer::executeUntil(EmuTime::param time, int /*userData*/)
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
