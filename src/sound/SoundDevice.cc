// $Id$

#include "SoundDevice.hh"
#include "MSXMixer.hh"
#include "XMLElement.hh"
#include "WavWriter.hh"
#include "StringOp.hh"
#include "HostCPU.hh"
#include "MemoryOps.hh"
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

int SoundDevice::getAmplificationFactor() const
{
	return 1;
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
	assert((long(dataOut) & 15) == 0); // must be 16-byte aligned
	assert(samples <= MAX_SAMPLES);
	if (samples == 0) return true;

	MemoryOps::MemSet<unsigned, false> mset;
	mset(reinterpret_cast<unsigned*>(dataOut), stereo * samples, 0);

	int* bufs[numChannels];
	unsigned separateChannels = 0;
	unsigned pitch = (samples * stereo + 3) & ~3; // align for SSE access
	for (unsigned i = 0; i < numChannels; ++i) {
		if (!channelMuted[i] && !writer[i].get()) {
			// no need to keep this channel separate
			bufs[i] = dataOut;
		} else {
			// muted or recorded channels must go separate
			bufs[i] = &mixBuffer[pitch * separateChannels];
			++separateChannels;
		}
	}
	mset(reinterpret_cast<unsigned*>(mixBuffer),
	     pitch * separateChannels, 0);

	// note: some SoundDevices (DACSound16S and CassettePlayer) replace the
	//       (single) channel data instead of adding to the exiting data.
	//       ATM that's ok because the existing data is anyway zero.
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
		if (writer[i].get()) {
			assert(bufs[i] != dataOut);
			if (stereo == 1) {
				writer[i]->write16mono(
					bufs[i] ? bufs[i] : silence,
					samples, getAmplificationFactor());
			} else {
				writer[i]->write16stereo(
					bufs[i] ? bufs[i] : silence,
					samples, getAmplificationFactor());
			}
		}
	}

	// remove muted channels (explictly by user or by device itself)
	bool anyUnmuted = false;
	unsigned numMix = 0;
	for (unsigned i = 0; i < numChannels; ++i) {
		if ((bufs[i] != 0) && !channelMuted[i]) {
			anyUnmuted = true;
			if (bufs[i] != dataOut) {
				bufs[numMix] = bufs[i];
				++numMix;
			}
		}
	}
	// also add output buffer
	bufs[numMix] = dataOut;
	++numMix;

	// actually mix channels
	switch (numMix) {
	case 0:
		assert(false);
	case 1:
		// all extra channels muted
		return anyUnmuted;
	default: {
		unsigned num = samples * stereo;

#ifdef __arm__
		unsigned dummy;
		asm volatile (
		"1:\n\t"
			"mov	%[j],%[unmuted1]\n\t"          // j = unmuted - 1
			"ldr	r10,[%[bufs],%[j],LSL #2]\n\t" // t = bufs[j* sizeof(int*)]
			"add	r10,r10,%[i],LSL #2\n\t"       // t += i * sizeof(int)
			"ldmdb	r10,{r3,r4,r5,r6}\n"           // a0..3 = *t
		"0:\n\t"
			"subs	%[j],%[j],#1\n\t"              // --j
			"ldr	r10,[%[bufs],%[j],LSL #2]\n\t" // t = bufs[j * sizeof(int*)]
			"add	r10,r10,%[i],LSL #2\n\t"       // t += i * sizeof(int)
			"ldmdb	r10,{r7,r8,r9,r10}\n\t"        // b0..3 = *t
			"add	r3,r3,r7\n\t"                  // a0 += b0
			"add	r4,r4,r8\n\t"                  // a1 += b1
			"add	r5,r5,r9\n\t"                  // a2 += b2
			"add	r6,r6,r10\n\t"                 // a3 += b3
			"bne	0b\n\t"                        // while (j != 0)
			"stmdb	%[out]!,{r3,r4,r5,r6}\n\t"     // out -= 4 * sizeof(int) ; *out = a0..a3
			"subs	%[i],%[i],#4\n\t"              // i -= 4
			"bne	1b\n\t"                        // while (i >= 0)

			: [j]        "=&r" (dummy)
			: [unmuted1] "r"   (numMix - 1)
			, [bufs]     "r"   (bufs)
			, [i]        "r"   ((num + 3) & ~3)
			, [out]      "r"   (&dataOut[(num + 3) & ~3])
			: "r3","r4","r5","r6","r7","r8","r9","r10"
		);
		return true;
#endif

		if (numMix & 1) {
			#ifdef ASM_X86
			const HostCPU& cpu = HostCPU::getInstance();
			if (cpu.hasSSE2()) {
				long dummy1;
				long dummy2;
				long zero = 0;
				asm volatile (
				"1:"
					"mov	%6,%0;"          // j = -numMix * sizeof(int*)
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

					"movdqa	%%xmm0,(%5,%2);"
					"add	$16,%2;"         // i += 4 * sizeof(int)
					"cmp    %7,%2;"          //
					"jb	1b;"             // while i < num

					: "=&r" (dummy1)                  // %0 = j
					, "=&r" (dummy2)                  // %1 = p
					: "r"   (zero)                    // %2 = i
					, "r"   (bufs + numMix)           // %3
					, "i"   (sizeof(int*))            // %4
					, "r"   (dataOut)                 // %5
					, "rm"  (-long(numMix) * sizeof(int*)) // %6
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
				} while (j < numMix);
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
					"mov	%6,%0;"          // j = -numMix * sizeof(int*)
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

					"movdqa	%%xmm0,(%5,%2);"
					"add	$16,%2;"         // i += 4 * sizeof(int)
					"cmp    %7,%2;"          //
					"jb	1b;"             // while i < num

					: "=&r" (dummy1)                  // %0 = j
					, "=&r" (dummy2)                  // %1 = p
					: "r"   (zero)                    // %2 = i
					, "r"   (bufs + numMix)           // %3
					, "i"   (sizeof(int*))            // %4
					, "r"   (dataOut)                 // %5
					, "rm"  (-long(numMix) * sizeof(int*)) // %6
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
				} while (j < numMix);
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
