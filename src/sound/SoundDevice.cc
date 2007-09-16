// $Id$

#include "SoundDevice.hh"
#include "MSXMixer.hh"
#include "XMLElement.hh"
#include "WavWriter.hh"
#include "StringOp.hh"
#include "HostCPU.hh"
#include <cstring>
#include <cassert>

using std::string;

namespace openmsx {

static const unsigned MAX_FACTOR = 16; // 200kHz (PSG) -> 22kHz
static const unsigned MAX_SAMPLES = 8192 * MAX_FACTOR;
static int mixBuffer[SoundDevice::MAX_CHANNELS * MAX_SAMPLES * 2]
	__attribute__((aligned(16))); // align for SSE access
static int silence[MAX_SAMPLES * 2];

static string makeUnique(MSXMixer& mixer, const string& name)
{
	string result = name;
	if (mixer.findDevice(result)) {
		unsigned n = 0;
		do {
			result = name + " (" + StringOp::toString(++n) + ")";
		} while (mixer.findDevice(result));
	}
	return result;
}

SoundDevice::SoundDevice(MSXMixer& mixer_, const string& name_,
                         const string& description_,
                         unsigned numChannels_, bool stereo_)
	: mixer(mixer_)
	, name(makeUnique(mixer, name_))
	, description(description_)
	, numChannels(numChannels_)
	, stereo(stereo_ ? 2 : 1)
	, numRecordChannels(0)
{
	assert(numChannels <= MAX_CHANNELS);
	assert(stereo == 1 || stereo == 2);
	memset(silence, 0, sizeof(silence));

	// initially no channels are muted
	for (unsigned i = 0; i < numChannels; ++i) {
		channelMuted[i] = false;
	}
}

SoundDevice::~SoundDevice()
{
}

const std::string& SoundDevice::getName() const
{
	return name;
}

const std::string& SoundDevice::getDescription() const
{
	return description;
}

bool SoundDevice::isStereo() const
{
	return stereo == 2;
}

void SoundDevice::registerSound(const XMLElement& config)
{
	const XMLElement& soundConfig = config.getChild("sound");
	double volume = soundConfig.getChildDataAsInt("volume") / 32767.0;
	int balance = 0;
	string mode = soundConfig.getChildData("mode", "mono");
	if (mode == "mono") {
		balance = 0;
	} else if (mode == "left") {
		balance = -100;
	} else if (mode == "right") {
		balance = 100;
	}
	balance = soundConfig.getChildDataAsInt("balance", balance);

	mixer.registerSound(*this, volume, balance, numChannels);
}

void SoundDevice::unregisterSound()
{
	mixer.unregisterSound(*this);
}

void SoundDevice::updateStream(const EmuTime& time)
{
	mixer.updateStream(time);
}

void SoundDevice::setInputRate(unsigned sampleRate)
{
	inputSampleRate = sampleRate;
}

void SoundDevice::recordChannel(unsigned channel, const string& filename)
{
	assert(channel < numChannels);
	bool wasRecording = writer[channel].get();
	if (!filename.empty()) {
		writer[channel].reset(new WavWriter(
			filename, stereo, 16, inputSampleRate));
	} else {
		writer[channel].reset();
	}
	bool recording = writer[channel].get();
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
	assert(samples <= MAX_SAMPLES);
	assert(samples > 0);
	int* bufs[numChannels];
	unsigned pitch = (samples * stereo + 3) & ~3; // align for SSE access
	for (unsigned i = 0; i < numChannels; ++i) {
		bufs[i] = &mixBuffer[pitch * i];
	}
	generateChannels(bufs, samples);

	// record channels
	for (unsigned i = 0; i < numChannels; ++i) {
		if (writer[i].get()) {
			if (stereo == 1) {
				writer[i]->write16mono(
					bufs[i] ? bufs[i] : silence,
					samples);
			} else {
				writer[i]->write16stereo(
					bufs[i] ? bufs[i] : silence,
					samples);
			}
		}
	}

	// remove muted channels (explictly by user or by device itself)
	unsigned unmuted = 0;
	for (unsigned i = 0; i < numChannels; ++i) {
		if ((bufs[i] != 0) && !channelMuted[i]) {
			bufs[unmuted] = bufs[i];
			++unmuted;
		}
	}

	// actually mix channels
	switch (unmuted) {
	case 0:
		// all channels muted
		return false;
	case 1:
		memcpy(dataOut, bufs[0], samples * stereo * sizeof(int));
		return true;
	default: {
		unsigned num = samples * stereo;
		if (unmuted & 1) {
			#ifdef ASM_X86
			const HostCPU& cpu = HostCPU::getInstance();
			if (cpu.hasSSE2()) {
				long dummy1;
				long dummy2;
				long zero = 0;
				asm volatile (
				"1:"
					"mov	%6,%0;"          // j = -unmuted * sizeof(int*)
					"mov	(%3,%0),%1;"     // p = buf[0]
					"add	%4,%0;"          // j += sizeof(int*)
					"movdqa	(%1,%2),%%xmm0;" // acc = buf[0][i]
					".p2align 4,,15;"
				"0:"
					"mov	(%3,%0),%1;"     // p = buf[j + 0]
					"paddd	(%1,%2),%%xmm0;" // acc += buf[j + 0][i]
				#ifdef ASM_X86_64
					"mov	8(%3,%0),%1;"    // p = buf[j + 1]
				#else
					"mov	4(%3,%0),%1;"    // p = buf[j + 1]
				#endif
					"add	%8,%0;"          // j += 2 * sizeof(int*)
					"paddd	(%1,%2),%%xmm0;" // acc += buf[j + 1][i]
					"jnz	0b;"             // while j negative

					"movdqu	%%xmm0,(%5,%2);"
					"add	$16,%2;"         // i += 4 * sizeof(int)
					"cmp    %7,%2;"          //
					"jb	1b;"             // while i < num

					: "=&r" (dummy1)                  // %0 = j
					, "=&r" (dummy2)                  // %1 = p
					: "r"   (zero)                    // %2 = i
					, "r"   (bufs + unmuted)          // %3
					, "i"   (sizeof(int*))            // %4
					, "r"   (dataOut)                 // %5
					, "rm"  (-long(unmuted) * sizeof(int*)) // %6
					, "rm"  (num * sizeof(int))       // %7
					, "i"   (2 * sizeof(int*))        // %8
					#ifdef __SSE__
					: "xmm0"
					#endif
				);
				return true;
			}
			#endif

			unsigned i = 0;
			do {
				int out0 = bufs[0][i + 0];
				int out1 = bufs[0][i + 1];
				int out2 = bufs[0][i + 2];
				int out3 = bufs[0][i + 3];
				unsigned j = 1;
				do {
					out0 += bufs[j + 0][i + 0];
					out1 += bufs[j + 0][i + 1];
					out2 += bufs[j + 0][i + 2];
					out3 += bufs[j + 0][i + 3];
					out0 += bufs[j + 1][i + 0];
					out1 += bufs[j + 1][i + 1];
					out2 += bufs[j + 1][i + 2];
					out3 += bufs[j + 1][i + 3];
					j += 2;
				} while (j < unmuted);
				dataOut[i + 0] = out0;
				dataOut[i + 1] = out1;
				dataOut[i + 2] = out2;
				dataOut[i + 3] = out3;
				i += 4;
			} while (i < num);
		} else {
			#ifdef ASM_X86
			const HostCPU& cpu = HostCPU::getInstance();
			if (cpu.hasSSE2()) {
				long dummy1;
				long dummy2;
				long zero = 0;
				asm volatile (
				"1:"
					"mov	%6,%0;"          // j = -unmuted * sizeof(int*)
					"pxor	%%xmm0,%%xmm0;"  // acc = buf[0][i]
					".p2align 4,,15;"
				"0:"
					"mov	(%3,%0),%1;"     // p = buf[j + 0]
					"paddd	(%1,%2),%%xmm0;" // acc += buf[j + 0][i]
				#ifdef ASM_X86_64
					"mov	8(%3,%0),%1;"    // p = buf[j + 1]
				#else
					"mov	4(%3,%0),%1;"    // p = buf[j + 1]
				#endif
					"add	%8,%0;"          // j += 2 * sizeof(int*)
					"paddd	(%1,%2),%%xmm0;" // acc += buf[j + 1][i]
					"jnz	0b;"             // while j negative

					"movdqu	%%xmm0,(%5,%2);"
					"add	$16,%2;"         // i += 4 * sizeof(int)
					"cmp    %7,%2;"          //
					"jb	1b;"             // while i < num

					: "=&r" (dummy1)                  // %0 = j
					, "=&r" (dummy2)                  // %1 = p
					: "r"   (zero)                    // %2 = i
					, "r"   (bufs + unmuted)          // %3
					, "i"   (sizeof(int*))            // %4
					, "r"   (dataOut)                 // %5
					, "rm"  (-long(unmuted) * sizeof(int*)) // %6
					, "rm"  (num * sizeof(int))       // %7
					, "i"   (2 * sizeof(int*))        // %8
					#ifdef __SSE__
					: "xmm0"
					#endif
				);
				return true;
			}
			#endif

			unsigned i = 0;
			do {
				int out0 = 0;
				int out1 = 0;
				int out2 = 0;
				int out3 = 0;
				unsigned j = 0;
				do {
					out0 += bufs[j + 0][i + 0];
					out1 += bufs[j + 0][i + 1];
					out2 += bufs[j + 0][i + 2];
					out3 += bufs[j + 0][i + 3];
					out0 += bufs[j + 1][i + 0];
					out1 += bufs[j + 1][i + 1];
					out2 += bufs[j + 1][i + 2];
					out3 += bufs[j + 1][i + 3];
					j += 2;
				} while (j < unmuted);
				dataOut[i + 0] = out0;
				dataOut[i + 1] = out1;
				dataOut[i + 2] = out2;
				dataOut[i + 3] = out3;
				i += 4;
			} while (i < num);
		}
		return true;
	}
	}
}

} // namespace openmsx
