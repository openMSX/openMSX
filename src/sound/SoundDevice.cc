#include "SoundDevice.hh"

#include "MSXMixer.hh"
#include "Mixer.hh"

#include "DeviceConfig.hh"
#include "Filename.hh"
#include "MSXException.hh"
#include "XMLElement.hh"

#include "StringOp.hh"
#include "inplace_buffer.hh"
#include "narrow.hh"
#include "xrange.hh"

#include <algorithm>
#include <cassert>
#include <cmath>

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
			 unsigned numChannels_, unsigned inputRate, Output output_)
	: mixer(mixer_)
	, name(makeUnique(mixer, name_))
	, description(description_)
	, numChannels(numChannels_)
	, outputMode(output_)
{
	assert(numChannels <= MAX_CHANNELS);

	setInputRate(inputRate);

	// initially no channels are muted
	std::ranges::fill(channelMuted, false);
	std::ranges::fill(channelBalance, Balance{.left = 1.0f, .right = 1.0f});
	updateChannelGroups();
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
		if ((*balance < -100) || (balance > 100)) {
			throw MSXException("balance must be between -100...100: ", *balance);
		}

		const auto* channel = b->findAttribute("channel");
		if (!channel) {
			devBalance = *balance;
			continue;
		}

		// A per-channel balance turns a mono device into a stereo
		// device. That's still possible here: the device isn't yet
		// registered with the mixer (so e.g. no resampler yet).
		auto channels = StringOp::parseRange(channel->getValue(), 1, numChannels); // may throw
		if (*balance != 0) outputMode = Output::STEREO;
		if (outputMode == Output::MONO) continue; // balance=0 is the default

		channels.foreachSetBit([&](size_t c) {
			setBalance(unsigned(c - 1), narrow_cast<float>(*balance) * (1.0f / 100.0f));
		});
	}

	mixer.registerSound(*this, volume, devBalance, numChannels);
}

void SoundDevice::setBalance(unsigned channel, float balance)
{
	assert(channel < numChannels);
	assert(-1.0f <= balance);
	assert(balance <= 1.0f);
	assert(outputMode == Output::STEREO);

	auto left  = std::sqrt(1.0f - balance);
	auto right = std::sqrt(1.0f + balance);
	channelBalance[channel] = Balance{.left = left, .right = right};
	updateChannelGroups();
}

void SoundDevice::updateChannelGroups()
{
	// Channels with the same balance are put in the same group.
	numGroups = 0;
	for (auto channel : xrange(numChannels)) {
		auto groups = std::span{groupBalance}.first(numGroups);
		auto it = std::ranges::find(groups, channelBalance[channel]);
		if (it == groups.end()) {
			groupBalance[numGroups++] = channelBalance[channel];
		}
		channelGroup[channel] = narrow<uint8_t>(std::distance(groups.begin(), it));
	}
}

void SoundDevice::setChannelFactors(unsigned channel, float left, float right, EmuTime time)
{
	ChannelFactors factors{.channel = channel, .left = left, .right = right};
	setChannelFactors(std::span{&factors, 1}, time);
}

void SoundDevice::setChannelFactors(std::span<const ChannelFactors> factors, EmuTime time)
{
	assert(outputMode == Output::STEREO);
	assert(std::ranges::all_of(factors, [&](const auto& f) { return f.channel < numChannels; }));
	auto changed = [&](const ChannelFactors& f) {
		return channelBalance[f.channel] != Balance{.left = f.left, .right = f.right};
	};
	if (std::ranges::none_of(factors, changed)) return;

	mixer.updateStream(time);
	for (const auto& f : factors) {
		channelBalance[f.channel] = Balance{.left = f.left, .right = f.right};
	}
	updateChannelGroups();
}

void SoundDevice::unregisterSound()
{
	mixer.unregisterSound(*this);
}

void SoundDevice::updateStream(EmuTime time)
{
	mixer.updateStream(time);
}

void SoundDevice::setSoftwareVolume(float volume, EmuTime time)
{
	setSoftwareVolume(volume, volume, time);
}

void SoundDevice::setSoftwareVolume(float left, float right, EmuTime time)
{
	updateStream(time);
	softwareVolumeLeft  = left;
	softwareVolumeRight = right;
	mixer.updateSoftwareVolume(*this);
}

void SoundDevice::recordChannel(unsigned channel, const std::string& filename)
{
	assert(channel < numChannels);
	bool wasRecording = writer[channel].has_value();
	if (!filename.empty()) {
		writer[channel].emplace(
			filename, 1, inputSampleRate);
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
	if (samples == 0) return true;
	bool mono = !isStereo();

	inplace_buffer<float*, MAX_CHANNELS> bufs(uninitialized_tag{}, numChannels);

	// By default channels share a buffer (generateChannels() then calculates
	// the sum of those channels):
	// - For a mono device all channels are generated in 'dataOut'.
	// - For a stereo device all channels with the same balance (the same
	//   group) are generated in the same buffer in 'groupBuffers'.
	// But these channels need their own buffer:
	auto needSeparateBuffer = [&](unsigned channel) {
		return channelBuffers[channel].requestCounter != 0
		    || channelMuted[channel]
		    || writer[channel];
	};
	static_assert(MAX_CHANNELS <= 32);
	uint32_t separateChannels = 0; // bit-mask of channels
	uint32_t usedGroups = 0;       // bit-mask of groups
	auto size = narrow<unsigned>(samples);
	auto padded = (size + 3) & ~3; // round up to multiple of 4
	// Distance between the group buffers. Not simply 'padded': when that's
	// a multiple of 4kB all buffers map to the same cache sets (measured:
	// with 16 groups of 1024 samples that made this method 1.6x slower).
	size_t groupStride = padded + 16;
	if (!mono) {
		if (auto needed = numGroups * groupStride;
		    groupBuffers.size() < needed) [[unlikely]] {
			groupBuffers.resize(needed);
		}
	}
	auto groupBuffer = [&](unsigned group) {
		return &groupBuffers[group * groupStride];
	};
	for (auto i : xrange(numChannels)) {
		auto& cb = channelBuffers[i];
		if (!needSeparateBuffer(i)) {
			// no need to keep this channel separate
			if (mono) {
				bufs[i] = dataOut;
			} else {
				auto group = channelGroup[i];
				bufs[i] = groupBuffer(group);
				usedGroups |= 1u << group;
			}
			cb.stopIdx = 0; // no valid last data
		} else {
			separateChannels |= 1u << i;
			cb.requestCounter = (cb.requestCounter < samples) ? 0 : unsigned(cb.requestCounter - samples);

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
			std::ranges::fill(std::span{ptr, size}, 0.0f);
			cb.stopIdx += size;
		}
	}

	// The generateChannels() method of SoundDevices with more than one
	// channel will _add_ the generated channel data in the provided
	// buffers. Those with only one channel will directly replace the
	// content of the buffer. For the former we must start from a buffer
	// containing all zeros.
	if (mono) {
		if ((numChannels != 1) || separateChannels) {
			std::ranges::fill(std::span{dataOut, samples}, 0.0f);
		}
	} else {
		for (auto group : xrange(numGroups)) {
			if (usedGroups & (1u << group)) {
				std::ranges::fill(std::span{groupBuffer(group), size}, 0.0f);
			}
		}
	}

	generateChannels(bufs, narrow<unsigned>(samples));

	// A channel with both factors zero isn't audible. Handle it like a
	// channel that was muted by the device itself: then it's also silent
	// when that channel is recorded or shown in the GUI.
	for (auto i : xrange(numChannels)) {
		if (isChannelOff(i)) {
			bufs[i] = nullptr;
		}
	}

	if (mono && !separateChannels) {
		return std::ranges::any_of(xrange(numChannels),
		                      [&](auto i) { return bufs[i]; });
	}

	if (separateChannels) {
		for (auto i : xrange(numChannels)) {
			// record channels
			if (writer[i]) {
				assert(bufs[i] != dataOut);
				if (bufs[i]) {
					writer[i]->write(std::span{bufs[i], samples},
					                 getChannelAmplificationFactor());
				} else {
					writer[i]->writeSilence(narrow<unsigned>(samples));
				}
			}
			// update silent info in channelBuffers
			auto& cb = channelBuffers[i];
			if (bufs[i]) {
				cb.silent = 0;
			} else {
				cb.silent += narrow<unsigned>(samples);
			}
		}
	}

	if (!mono) {
		// Collect the buffers that contribute to the output: the
		// non-silent group buffers and the non-silent, non-muted
		// separate channels. Each with their balance.
		inplace_buffer<const float*, MAX_CHANNELS> mixBufs   (uninitialized_tag{}, numChannels);
		inplace_buffer<Balance,      MAX_CHANNELS> mixBalance(uninitialized_tag{}, numChannels);
		unsigned numMix = 0;
		auto add = [&](const float* buf, const Balance& balance) {
			if ((balance.left == 0.0f) && (balance.right == 0.0f)) return;
			mixBufs[numMix] = buf;
			mixBalance[numMix] = balance;
			++numMix;
		};
		uint32_t activeGroups = 0;
		for (auto i : xrange(numChannels)) {
			if (!bufs[i]) continue; // muted by the device itself
			if (separateChannels & (1u << i)) {
				if (!channelMuted[i]) {
					add(bufs[i], channelBalance[i]);
				}
			} else {
				activeGroups |= 1u << channelGroup[i];
			}
		}
		for (auto group : xrange(numGroups)) {
			if (activeGroups & (1u << group)) {
				add(groupBuffer(group), groupBalance[group]);
			}
		}
		if (numMix == 0) return false;

		// Apply the balance and mix to a stereo output.
		{
			const float* in = mixBufs[0];
			auto l = mixBalance[0].left;
			auto r = mixBalance[0].right;
			for (size_t i = 0; i < samples; ++i) {
				dataOut[2 * i + 0] = in[i] * l;
				dataOut[2 * i + 1] = in[i] * r;
			}
		}
		for (unsigned j = 1; j < numMix; ++j) {
			const float* __restrict in = mixBufs[j];
			float* __restrict out = dataOut;
			auto l = mixBalance[j].left;
			auto r = mixBalance[j].right;
			for (size_t i = 0; i < samples; ++i) {
				out[2 * i + 0] += in[i] * l;
				out[2 * i + 1] += in[i] * r;
			}
		}

		return true;
	}

	// remove muted channels (explicitly by user or by device itself)
	bool anyUnmuted = false;
	unsigned numMix = 0;
	for (auto i : xrange(numChannels)) {
		if (bufs[i] && !channelMuted[i]) {
			anyUnmuted = true;
			if (bufs[i] != dataOut) {
				bufs[numMix] = bufs[i];
				++numMix;
			}
		}
	}

	if (numMix == 0) {
		// all extra channels muted
		return anyUnmuted;
	}

	// In the past we had ARM and x86-SSE2 optimized assembly routines for
	// the stuff below. Currently this code is only rarely used anymore
	// (only when recording or muting individual sound chip channels), so
	// it's not worth the extra complexity anymore.
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
	} while (i < samples);

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
