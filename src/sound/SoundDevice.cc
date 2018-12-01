#include "SoundDevice.hh"
#include "MSXMixer.hh"
#include "DeviceConfig.hh"
#include "XMLElement.hh"
#include "WavWriter.hh"
#include "Filename.hh"
#include "StringOp.hh"
#include "MemoryOps.hh"
#include "MemBuffer.hh"
#include "MSXException.hh"
#include "likely.hh"
#include "vla.hh"
#include <cassert>
#include <memory>

using std::string;

namespace openmsx {

static MemBuffer<int, SSE2_ALIGNMENT> mixBuffer;
static unsigned mixBufferSize = 0;

static void allocateMixBuffer(unsigned size)
{
	if (unlikely(mixBufferSize < size)) {
		mixBufferSize = size;
		mixBuffer.resize(mixBufferSize);
	}
}

static string makeUnique(MSXMixer& mixer, string_view name)
{
	string result = name.str();
	if (mixer.findDevice(result)) {
		unsigned n = 0;
		do {
			result = strCat(name, " (", ++n, ')');
		} while (mixer.findDevice(result));
	}
	return result;
}

void SoundDevice::addFill(int*& buf, int val, unsigned num)
{
	// Note: in the past we tried to optimize this by always producing
	// a multiple of 4 output values. In the general case a sounddevice is
	// allowed to do this, but only at the end of the soundbuffer. This
	// method can also be called in the middle of a buffer (so multiple
	// times per buffer), in such case it does go wrong.
	assert(num > 0);
	do {
		*buf++ += val;
	} while (--num);
}

SoundDevice::SoundDevice(MSXMixer& mixer_, string_view name_,
			 string_view description_,
			 unsigned numChannels_, bool stereo_)
	: mixer(mixer_)
	, name(makeUnique(mixer, name_))
	, description(description_.str())
	, numChannels(numChannels_)
	, stereo(stereo_ ? 2 : 1)
	, numRecordChannels(0)
	, balanceCenter(true)
{
	assert(numChannels <= MAX_CHANNELS);
	assert(stereo == 1 || stereo == 2);

	// initially no channels are muted
	for (unsigned i = 0; i < numChannels; ++i) {
		channelMuted[i] = false;
		channelBalance[i] = 0;
	}
}

SoundDevice::~SoundDevice() = default;

bool SoundDevice::isStereo() const
{
	return stereo == 2 || !balanceCenter;
}

int SoundDevice::getAmplificationFactorImpl() const
{
	return 1;
}

void SoundDevice::registerSound(const DeviceConfig& config)
{
	const XMLElement& soundConfig = config.getChild("sound");
	float volume = soundConfig.getChildDataAsInt("volume") / 32767.0f;
	int devBalance = 0;
	string_view mode = soundConfig.getChildData("mode", "mono");
	if (mode == "mono") {
		devBalance = 0;
	} else if (mode == "left") {
		devBalance = -100;
	} else if (mode == "right") {
		devBalance = 100;
	} else {
		throw MSXException("balance \"", mode, "\" illegal");
	}

	for (auto& b : soundConfig.getChildren("balance")) {
		int balance = StringOp::stringToInt(b->getData());

		if (!b->hasAttribute("channel")) {
			devBalance = balance;
			continue;
		}

		// TODO Support other balances
		if (balance != 0 && balance != -100 && balance != 100) {
			throw MSXException("balance ", balance, " illegal");
		}
		if (balance != 0) {
			balanceCenter = false;
		}

		const string& range = b->getAttribute("channel");
		for (unsigned c : StringOp::parseRange(range, 1, numChannels)) {
			channelBalance[c - 1] = balance;
		}
	}

	mixer.registerSound(*this, volume, devBalance, numChannels);
}

void SoundDevice::unregisterSound()
{
	mixer.unregisterSound(*this);
}

void SoundDevice::updateStream(EmuTime::param time)
{
	mixer.updateStream(time);
}

void SoundDevice::setSoftwareVolume(VolumeType volume, EmuTime::param time)
{
	setSoftwareVolume(volume, volume, time);
}

void SoundDevice::setSoftwareVolume(VolumeType left, VolumeType right, EmuTime::param time)
{
	updateStream(time);
	softwareVolumeLeft  = left;
	softwareVolumeRight = right;
	mixer.updateSoftwareVolume(*this);
}

void SoundDevice::recordChannel(unsigned channel, const Filename& filename)
{
	assert(channel < numChannels);
	bool wasRecording = writer[channel] != nullptr;
	if (!filename.empty()) {
		writer[channel] = std::make_unique<Wav16Writer>(
			filename, stereo, inputSampleRate);
	} else {
		writer[channel].reset();
	}
	bool recording = writer[channel] != nullptr;
	if (recording != wasRecording) {
		if (recording) {
			if (numRecordChannels == 0) {
				mixer.setSynchronousMode(true);
			}
			++numRecordChannels;
			assert(numRecordChannels <= numChannels);
		} else {
			assert(numRecordChannels > 0);
			--numRecordChannels;
			if (numRecordChannels == 0) {
				mixer.setSynchronousMode(false);
			}
		}
	}
}

void SoundDevice::muteChannel(unsigned channel, bool muted)
{
	assert(channel < numChannels);
	channelMuted[channel] = muted;
}

bool SoundDevice::mixChannels(int* dataOut, unsigned samples)
{
#ifdef __SSE2__
	assert((uintptr_t(dataOut) & 15) == 0); // must be 16-byte aligned
#endif
	if (samples == 0) return true;
	unsigned outputStereo = isStereo() ? 2 : 1;

	MemoryOps::MemSet<unsigned> mset;
	if (numChannels != 1) {
		// The generateChannels() method of SoundDevices with more than
		// one channel will _add_ the generated channel data in the
		// provided buffers. Those with only one channel will directly
		// replace the content of the buffer. For the former we must
		// start from a buffer containing all zeros.
		mset(reinterpret_cast<unsigned*>(dataOut), outputStereo * samples, 0);
	}

	VLA(int*, bufs, numChannels);
	unsigned separateChannels = 0;
	unsigned pitch = (samples * stereo + 3) & ~3; // align for SSE access
	// TODO optimization: All channels with the same balance (according to
	// channelBalance[]) could use the same buffer when balanceCenter is
	// false
	for (unsigned i = 0; i < numChannels; ++i) {
		if (!channelMuted[i] && !writer[i] && balanceCenter) {
			// no need to keep this channel separate
			bufs[i] = dataOut;
		} else {
			// muted or recorded channels must go separate
			//  cannot yet fill in bufs[i] here
			++separateChannels;
		}
	}
	if (separateChannels) {
		allocateMixBuffer(pitch * separateChannels);
		mset(reinterpret_cast<unsigned*>(mixBuffer.data()),
		     pitch * separateChannels, 0);
		// still need to fill in (some) bufs[i] pointers
		unsigned count = 0;
		for (unsigned i = 0; i < numChannels; ++i) {
			if (!(!channelMuted[i] && !writer[i] && balanceCenter)) {
				bufs[i] = &mixBuffer[pitch * count++];
			}
		}
		assert(count == separateChannels);
	}

	generateChannels(bufs, samples);

	if (separateChannels == 0) {
		for (unsigned i = 0; i < numChannels; ++i) {
			if (bufs[i]) {
				return true;
			}
		}
		return false;
	}

	// record channels
	for (unsigned i = 0; i < numChannels; ++i) {
		if (writer[i]) {
			assert(bufs[i] != dataOut);
			if (bufs[i]) {
				auto amp = getAmplificationFactor();
				writer[i]->write(
					bufs[i], stereo, samples,
					amp.first.toFloat(),
					amp.second.toFloat());
			} else {
				writer[i]->writeSilence(stereo, samples);
			}
		}
	}

	// remove muted channels (explictly by user or by device itself)
	bool anyUnmuted = false;
	unsigned numMix = 0;
	VLA(int, mixBalance, numChannels);
	for (unsigned i = 0; i < numChannels; ++i) {
		if (bufs[i] && !channelMuted[i]) {
			anyUnmuted = true;
			if (bufs[i] != dataOut) {
				bufs[numMix] = bufs[i];
				mixBalance[numMix] = channelBalance[i];
				++numMix;
			}
		}
	}

	if (numMix == 0) {
		// all extra channels muted
		return anyUnmuted;
	}

	// actually mix channels
	if (!balanceCenter) {
		unsigned i = 0;
		do {
			int left0  = 0;
			int right0 = 0;
			int left1  = 0;
			int right1 = 0;
			unsigned j = 0;
			do {
				if (mixBalance[j] <= 0) {
					left0  += bufs[j][i + 0];
					left1  += bufs[j][i + 1];
				}
				if (mixBalance[j] >= 0) {
					right0 += bufs[j][i + 0];
					right1 += bufs[j][i + 1];
				}
				j++;
			} while (j < numMix);
			dataOut[i * 2 + 0] = left0;
			dataOut[i * 2 + 1] = right0;
			dataOut[i * 2 + 2] = left1;
			dataOut[i * 2 + 3] = right1;
			i += 2;
		} while (i < samples);

		return true;
	}

	// In the past we had ARM and x86-SSE2 optimized assembly routines for
	// the stuff below. Currently this code is only rarely used anymore
	// (only when recording or muting individual soundchip channels), so
	// it's not worth the extra complexity anymore.
	unsigned num = samples * stereo;
	unsigned i = 0;
	do {
		int out0 = dataOut[i + 0];
		int out1 = dataOut[i + 1];
		int out2 = dataOut[i + 2];
		int out3 = dataOut[i + 3];
		unsigned j = 0;
		do {
			out0 += bufs[j][i + 0];
			out1 += bufs[j][i + 1];
			out2 += bufs[j][i + 2];
			out3 += bufs[j][i + 3];
			++j;
		} while (j < numMix);
		dataOut[i + 0] = out0;
		dataOut[i + 1] = out1;
		dataOut[i + 2] = out2;
		dataOut[i + 3] = out3;
		i += 4;
	} while (i < num);

	return true;
}

const DynamicClock& SoundDevice::getHostSampleClock() const
{
	return mixer.getHostSampleClock();
}
double SoundDevice::getEffectiveSpeed() const
{
	return mixer.getEffectiveSpeed();
}

} // namespace openmsx
