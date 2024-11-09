#include "SoundDevice.hh"

#include "MSXMixer.hh"
#include "DeviceConfig.hh"
#include "Mixer.hh"
#include "XMLElement.hh"
#include "Filename.hh"
#include "StringOp.hh"
#include "MSXException.hh"

#include "inplace_buffer.hh"
#include "narrow.hh"
#include "ranges.hh"
#include "one_of.hh"
#include "xrange.hh"

#include <bit>
#include <cassert>

namespace openmsx {

[[nodiscard]] static std::string makeUnique(const MSXMixer& mixer, std::string_view name)
{
	std::string result(name);
	if (mixer.findDevice(result)) {
		unsigned n = 0;
		do {
			result = strCat(name, " (", ++n, ')');
		} while (mixer.findDevice(result));
	}
	return result;
}

void SoundDevice::addFill(float*& buf, float val, unsigned num)
{
	// Note: in the past we tried to optimize this by always producing
	// a multiple of 4 output values. In the general case a SoundDevice is
	// allowed to do this, but only at the end of the sound buffer. This
	// method can also be called in the middle of a buffer (so multiple
	// times per buffer), in such case it does go wrong.
	assert(num > 0);
	do {
		*buf++ += val;
	} while (--num);
}

SoundDevice::SoundDevice(MSXMixer& mixer_, std::string_view name_, static_string_view description_,
			 unsigned numChannels_, unsigned inputRate, bool stereo_)
	: mixer(mixer_)
	, name(makeUnique(mixer, name_))
	, description(description_)
	, numChannels(numChannels_)
	, stereo(stereo_ ? 2 : 1)
{
	assert(numChannels <= MAX_CHANNELS);
	assert(stereo == one_of(1u, 2u));

	setInputRate(inputRate);

	// initially no channels are muted
	ranges::fill(channelMuted, false);
	ranges::fill(channelBalance, 0);
}

SoundDevice::~SoundDevice() = default;

float SoundDevice::getAmplificationFactorImpl() const
{
	return 1.0f / 32768.0f;
}

void SoundDevice::registerSound(const DeviceConfig& config)
{
	const auto& soundConfig = config.getChild("sound");
	float volume = narrow<float>(soundConfig.getChildDataAsInt("volume", 0)) * (1.0f / 32767.0f);
	int devBalance = 0;
	std::string_view mode = soundConfig.getChildData("mode", "mono");
	if (mode == "mono") {
		devBalance = 0;
	} else if (mode == "left") {
		devBalance = -100;
	} else if (mode == "right") {
		devBalance = 100;
	} else {
		throw MSXException("balance \"", mode, "\" illegal");
	}

	for (const auto* b : soundConfig.getChildren("balance")) {
		auto balance = StringOp::stringTo<int>(b->getData());
		if (!balance) {
			throw MSXException("balance ", b->getData(), " illegal");
		}

		const auto* channel = b->findAttribute("channel");
		if (!channel) {
			devBalance = *balance;
			continue;
		}

		// TODO Support other balances
		if (*balance != one_of(0, -100, 100)) {
			throw MSXException("balance ", *balance, " illegal");
		}
		if (*balance != 0) {
			balanceCenter = false;
		}

		auto channels = StringOp::parseRange(channel->getValue(), 1, numChannels);
		channels.foreachSetBit([&](size_t c) {
			channelBalance[c - 1] = *balance;
		});
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

void SoundDevice::setSoftwareVolume(float volume, EmuTime::param time)
{
	setSoftwareVolume(volume, volume, time);
}

void SoundDevice::setSoftwareVolume(float left, float right, EmuTime::param time)
{
	updateStream(time);
	softwareVolumeLeft  = left;
	softwareVolumeRight = right;
	mixer.updateSoftwareVolume(*this);
}

void SoundDevice::recordChannel(unsigned channel, const Filename& filename)
{
	assert(channel < numChannels);
	bool wasRecording = writer[channel].has_value();
	if (!filename.empty()) {
		writer[channel].emplace(
			filename, stereo, inputSampleRate);
	} else {
		writer[channel].reset();
	}
	bool recording = writer[channel].has_value();
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

std::span<const float> SoundDevice::getLastBuffer(unsigned channel)
{
	assert(channel < numChannels);
	auto& buf = channelBuffers[channel];

	buf.requestCounter = inputSampleRate; // active for ~1 second

	unsigned requestedSize = getLastBufferSize();
	if (buf.stopIdx < requestedSize) return {}; // not enough samples (yet)
	if (buf.silent >= requestedSize) return {}; // silent
	return {&buf.buffer[buf.stopIdx - requestedSize], requestedSize};
}

bool SoundDevice::mixChannels(float* dataOut, size_t samples)
{
#ifdef __SSE2__
	assert((uintptr_t(dataOut) & 15) == 0); // must be 16-byte aligned
#endif
	if (samples == 0) return true;
	size_t outputStereo = isStereo() ? 2 : 1;

	inplace_buffer<float*, MAX_CHANNELS> bufs(uninitialized_tag{}, numChannels);

	// TODO optimization: All channels with the same balance (according to
	// channelBalance[]) could use the same buffer when balanceCenter is
	// false
	auto needSeparateBuffer = [&](unsigned channel) {
		return channelBuffers[channel].requestCounter != 0
		    || channelMuted[channel]
		    || writer[channel]
		    || !balanceCenter;
	};
	bool anySeparateChannel = false;
	auto size = narrow<unsigned>(samples * stereo);
	auto padded = (size + 3) & ~3; // round up to multiple of 4
	for (auto i : xrange(numChannels)) {
		auto& cb = channelBuffers[i];
		if (!needSeparateBuffer(i)) {
			// no need to keep this channel separate
			bufs[i] = dataOut;
			cb.stopIdx = 0; // no valid last data
		} else {
			anySeparateChannel = true;
			cb.requestCounter = (cb.requestCounter < samples) ? 0 : (cb.requestCounter - samples);

			if (auto remainingSize = narrow<unsigned>(cb.buffer.size() - cb.stopIdx);
			    remainingSize < padded) {
				// not enough space at the tail of the buffer
				unsigned lastBufferSize = getLastBufferSize();
				if (auto allocateSize = 2 * std::max(lastBufferSize, padded);
				    cb.buffer.size() < allocateSize) [[unlikely]] {
					// increase buffer size
					cb.buffer.resize(allocateSize);
				}
				unsigned reuse = lastBufferSize >= size ? lastBufferSize - size : 0;
				if (cb.stopIdx > reuse) {
					// move existing data to front
					memmove(&cb.buffer[0], &cb.buffer[cb.stopIdx - reuse], reuse * sizeof(float));
				}
				cb.stopIdx = reuse;
			}
			auto* ptr = &cb.buffer[cb.stopIdx];
			bufs[i] = ptr;
			ranges::fill(std::span{ptr, size}, 0.0f);
			cb.stopIdx += size;
		}
	}

	static_assert(sizeof(float) == sizeof(uint32_t));
	if ((numChannels != 1) || anySeparateChannel) {
		// The generateChannels() method of SoundDevices with more than
		// one channel will _add_ the generated channel data in the
		// provided buffers. Those with only one channel will directly
		// replace the content of the buffer. For the former we must
		// start from a buffer containing all zeros.
		ranges::fill(std::span{dataOut, outputStereo * samples}, 0.0f);
	}

	generateChannels(bufs, narrow<unsigned>(samples));

	if (!anySeparateChannel) {
		return ranges::any_of(xrange(numChannels),
		                      [&](auto i) { return bufs[i]; });
	}

	for (auto i : xrange(numChannels)) {
		// record channels
		if (writer[i]) {
			assert(bufs[i] != dataOut);
			if (bufs[i]) {
				auto amp = getAmplificationFactor();
				if (stereo == 1) {
					writer[i]->write(
						std::span{bufs[i], samples},
						amp.left);
				} else {
					writer[i]->write(
						std::span{std::bit_cast<const StereoFloat*>(bufs[i]), samples},
						amp.left, amp.right);
				}
			} else {
				writer[i]->writeSilence(narrow<unsigned>(stereo * samples));
			}
		}
		// update silent info in channelBuffers
		auto& cb = channelBuffers[i];
		if (bufs[i]) {
			cb.silent = 0;
		} else {
			cb.silent += samples;
		}
	}

	// remove muted channels (explicitly by user or by device itself)
	bool anyUnmuted = false;
	unsigned numMix = 0;
	inplace_buffer<int, MAX_CHANNELS> mixBalance(uninitialized_tag{}, numChannels);
	for (auto i : xrange(numChannels)) {
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
		size_t i = 0;
		do {
			float left0  = 0.0f;
			float right0 = 0.0f;
			float left1  = 0.0f;
			float right1 = 0.0f;
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
	// (only when recording or muting individual sound chip channels), so
	// it's not worth the extra complexity anymore.
	size_t num = samples * stereo;
	size_t i = 0;
	do {
		auto out0 = dataOut[i + 0];
		auto out1 = dataOut[i + 1];
		auto out2 = dataOut[i + 2];
		auto out3 = dataOut[i + 3];
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
