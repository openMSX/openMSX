#include "MSXMixer.hh"
#include "Mixer.hh"
#include "SoundDevice.hh"
#include "MSXMotherBoard.hh"
#include "MSXCommandController.hh"
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
#include "stl.hh"
#include "aligned.hh"
#include "outer.hh"
#include "unreachable.hh"
#include "vla.hh"
#include <algorithm>
#include <cassert>
#include <cmath>
#include <cstring>
#include <memory>
#include <tuple>

#ifdef __SSE2__
#include "emmintrin.h"
#endif

using std::string;
using std::vector;

namespace openmsx {

MSXMixer::MSXMixer(Mixer& mixer_, MSXMotherBoard& motherBoard_,
                   GlobalSettings& globalSettings)
	: Schedulable(motherBoard_.getScheduler())
	, mixer(mixer_)
	, motherBoard(motherBoard_)
	, commandController(motherBoard.getMSXCommandController())
	, masterVolume(mixer.getMasterVolume())
	, speedSetting(globalSettings.getSpeedSetting())
	, throttleManager(globalSettings.getThrottleManager())
	, prevTime(getCurrentTime(), 44100)
	, soundDeviceInfo(commandController.getMachineInfoCommand())
	, recorder(nullptr)
	, synchronousCounter(0)
{
	hostSampleRate = 44100;
	fragmentSize = 0;

	muteCount = 1;
	unmute(); // calls Mixer::registerMixer()

	reschedule2();

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

void MSXMixer::registerSound(SoundDevice& device, float volume,
                             int balance, unsigned numChannels)
{
	// TODO read volume/balance(mode) from config file
	const string& name = device.getName();
	SoundDeviceInfo info;
	info.device = &device;
	info.defaultVolume = volume;
	info.volumeSetting = std::make_unique<IntegerSetting>(
		commandController, name + "_volume",
		"the volume of this sound chip", 75, 0, 100);
	info.balanceSetting = std::make_unique<IntegerSetting>(
		commandController, name + "_balance",
		"the balance of this sound chip", balance, -100, 100);

	info.volumeSetting->attach(*this);
	info.balanceSetting->attach(*this);

	for (unsigned i = 0; i < numChannels; ++i) {
		SoundDeviceInfo::ChannelSettings channelSettings;
		string ch_name = strCat(name, "_ch", i + 1);

		channelSettings.recordSetting = std::make_unique<StringSetting>(
			commandController, ch_name + "_record",
			"filename to record this channel to",
			string_view{}, Setting::DONT_SAVE);
		channelSettings.recordSetting->attach(*this);

		channelSettings.muteSetting = std::make_unique<BooleanSetting>(
			commandController, ch_name + "_mute",
			"sets mute-status of individual sound channels",
			false, Setting::DONT_SAVE);
		channelSettings.muteSetting->attach(*this);

		info.channelSettings.push_back(std::move(channelSettings));
	}

	device.setOutputRate(getSampleRate());
	infos.push_back(std::move(info));
	updateVolumeParams(infos.back());

	commandController.getCliComm().update(CliComm::SOUNDDEVICE, device.getName(), "add");
}

void MSXMixer::unregisterSound(SoundDevice& device)
{
	auto it = rfind_if_unguarded(infos,
		[&](const SoundDeviceInfo& i) { return i.device == &device; });
	it->volumeSetting->detach(*this);
	it->balanceSetting->detach(*this);
	for (auto& s : it->channelSettings) {
		s.recordSetting->detach(*this);
		s.muteSetting->detach(*this);
	}
	move_pop_back(infos, it);
	commandController.getCliComm().update(CliComm::SOUNDDEVICE, device.getName(), "remove");
}

void MSXMixer::setSynchronousMode(bool synchronous)
{
	// TODO ATM synchronous is not used anymore
	if (synchronous) {
		++synchronousCounter;
		if (synchronousCounter == 1) {
			setMixerParams(fragmentSize, hostSampleRate);
		}
	} else {
		assert(synchronousCounter > 0);
		--synchronousCounter;
		if (synchronousCounter == 0) {
			setMixerParams(fragmentSize, hostSampleRate);
		}
	}
}

double MSXMixer::getEffectiveSpeed() const
{
	return synchronousCounter
	     ? 1.0
	     : speedSetting.getInt() / 100.0;
}

void MSXMixer::updateStream(EmuTime::param time)
{
	union {
		int16_t mixBuffer[8192 * 2];
		int32_t dummy1; // make sure mixBuffer is also 32-bit aligned
#ifdef __SSE2__
		__m128i dummy2; // and optionally also 128-bit
#endif
	};

	unsigned count = prevTime.getTicksTill(time);
	assert(count <= 8192);

	// call generate() even if count==0 and even if muted
	generate(mixBuffer, time, count);

	if (!muteCount && fragmentSize) {
		mixer.uploadBuffer(*this, mixBuffer, count);
	}

	if (recorder) {
		recorder->addWave(count, mixBuffer);
	}

	prevTime += count;
}


// Various (inner) loops that multiply one buffer by a constant and add the
// result to a second buffer. Either buffer can be mono or stereo, so if
// necessary the mono buffer is expanded to stereo. It's possible the
// accumulation buffer is still empty (as-if it contains zeros), in that case
// we skip the accumulation step.

// buf[0:n] *= f
static inline void mul(int32_t* buf, int n, int f)
{
	// C++ version, unrolled 4x,
	//   this allows gcc/clang to do much better auto-vectorization
	// Note that this can process upto 3 samples too many, but that's OK.
	assume_SSE_aligned(buf);
	int i = 0;
	do {
		buf[i + 0] *= f;
		buf[i + 1] *= f;
		buf[i + 2] *= f;
		buf[i + 3] *= f;
		i += 4;
	} while (i < n);
}

// acc[0:n] += mul[0:n] * f
static inline void mulAcc(
	int32_t* __restrict acc, const int32_t* __restrict mul, int n, int f)
{
	// C++ version, unrolled 4x, see comments above.
	assume_SSE_aligned(acc);
	assume_SSE_aligned(mul);
	int i = 0;
	do {
		acc[i + 0] += mul[i + 0] * f;
		acc[i + 1] += mul[i + 1] * f;
		acc[i + 2] += mul[i + 2] * f;
		acc[i + 3] += mul[i + 3] * f;
		i += 4;
	} while (i < n);
}

// buf[0:2n+0:2] = buf[0:n] * l
// buf[1:2n+1:2] = buf[0:n] * r
static inline void mulExpand(int32_t* buf, int n, int l, int r)
{
	int i = n;
	do {
		--i; // back-to-front
		auto t = buf[i];
		buf[2 * i + 0] = l * t;
		buf[2 * i + 1] = r * t;
	} while (i != 0);
}

// acc[0:2n+0:2] += mul[0:n] * l
// acc[1:2n+1:2] += mul[0:n] * r
static inline void mulExpandAcc(
	int32_t* __restrict acc, const int32_t* __restrict mul, int n,
	int l, int r)
{
	int i = 0;
	do {
		auto t = mul[i];
		acc[2 * i + 0] += l * t;
		acc[2 * i + 1] += r * t;
	} while (++i < n);
}

// buf[0:2n+0:2] = buf[0:2n+0:2] * l1 + buf[1:2n+1:2] * l2
// buf[1:2n+1:2] = buf[0:2n+0:2] * r1 + buf[1:2n+1:2] * r2
static inline void mulMix2(int32_t* buf, int n, int l1, int l2, int r1, int r2)
{
	int i = 0;
	do {
		auto t1 = buf[2 * i + 0];
		auto t2 = buf[2 * i + 1];
		buf[2 * i + 0] = l1 * t1 + l2 * t2;
		buf[2 * i + 1] = r1 * t1 + r2 * t2;
	} while (++i < n);
}

// acc[0:2n+0:2] += mul[0:2n+0:2] * l1 + mul[1:2n+1:2] * l2
// acc[1:2n+1:2] += mul[0:2n+0:2] * r1 + mul[1:2n+1:2] * r2
static inline void mulMix2Acc(
	int32_t* __restrict acc, const int32_t* __restrict mul, int n,
	int l1, int l2, int r1, int r2)
{
	int i = 0;
	do {
		auto t1 = mul[2 * i + 0];
		auto t2 = mul[2 * i + 1];
		acc[2 * i + 0] += l1 * t1 + l2 * t2;
		acc[2 * i + 1] += r1 * t1 + r2 * t2;
	} while (++i < n);
}


// DC removal filter routines:
//
//  formula:
//     y(n) = x(n) - x(n-1) + R * y(n-1)
//  implemented as:
//     t1 = R * t0 + x(n)    mathematically equivalent, has
//     y(n) = t1 - t0        the same number of operations but
//     t0 = t1               requires only one state variable
//    see: http://en.wikipedia.org/wiki/Digital_filter#Direct_Form_I
//  with:
//     R = 1 - (2*pi * cut-off-frequency / samplerate)
//  we take R = 511/512
//   44100Hz --> cutt-off freq = 14Hz
//   22050Hz                     7Hz
// Note: the input still needs to be divided by 512 (because of balance-
//       multiplication), can be done together with the above division.

// No new input, previous output was (non-zero) mono.
static inline int32_t filterMonoNull(int32_t t0, int16_t* out, int n)
{
	assert(n > 0);
	int i = 0;
	do {
		int32_t t1 = (511 * int64_t(t0)) >> 9;
		auto s = Math::clipIntToShort(t1 - t0);
		t0 = t1;
		out[2 * i + 0] = s;
		out[2 * i + 1] = s;
	} while (++i < n);
	return t0;
}

// No new input, previous output was (non-zero) stereo.
static inline std::tuple<int32_t, int32_t> filterStereoNull(
	int32_t tl0, int32_t tr0, int16_t* out, int n)
{
	assert(n > 0);
	int i = 0;
	do {
		int32_t tl1 = (511 * int64_t(tl0)) >> 9;
		int32_t tr1 = (511 * int64_t(tr0)) >> 9;
		out[2 * i + 0] = Math::clipIntToShort(tl1 - tl0);
		out[2 * i + 1] = Math::clipIntToShort(tr1 - tr0);
		tl0 = tl1;
		tr0 = tr1;
	} while (++i < n);
	return std::make_tuple(tl0, tr0);
}

// New input is mono, previous output was also mono.
static inline int32_t filterMonoMono(int32_t t0, void* buf, int n)
{
	assert(n > 0);
	const auto* in  = static_cast<const int32_t*>(buf);
	      auto* out = static_cast<      int16_t*>(buf);
	int i = 0;
	do {
		int32_t t1 = (511 * int64_t(t0) + in[i]) >> 9;
		auto s = Math::clipIntToShort(t1 - t0);
		t0 = t1;
		out[2 * i + 0] = s;
		out[2 * i + 1] = s;
	} while (++i < n);
	return t0;
}

// New input is mono, previous output was stereo
static inline std::tuple<int32_t, int32_t> filterStereoMono(
	int32_t tl0, int32_t tr0, void* buf, int n)
{
	assert(n > 0);
	const auto* in  = static_cast<const int32_t*>(buf);
	      auto* out = static_cast<      int16_t*>(buf);
	int i = 0;
	do {
		auto x = in[i];
		int32_t tl1 = (511 * int64_t(tl0) + x) >> 9;
		int32_t tr1 = (511 * int64_t(tr0) + x) >> 9;
		out[2 * i + 0] = Math::clipIntToShort(tl1 - tl0);
		out[2 * i + 1] = Math::clipIntToShort(tr1 - tr0);
		tl0 = tl1;
		tr0 = tr1;
	} while (++i < n);
	return std::make_tuple(tl0, tr0);
}

// New input is stereo, (previous output either mono/stereo)
static inline std::tuple<int32_t, int32_t> filterStereoStereo(
	int32_t tl0, int32_t tr0, const int32_t* in, int16_t* out, int n)
{
	assert(n > 0);
	int i = 0;
	do {
		int32_t tl1 = (511 * int64_t(tl0) + in[2 * i + 0]) >> 9;
		int32_t tr1 = (511 * int64_t(tr0) + in[2 * i + 1]) >> 9;
		out[2 * i + 0] = Math::clipIntToShort(tl1 - tl0);
		out[2 * i + 1] = Math::clipIntToShort(tr1 - tr0);
		tl0 = tl1;
		tr0 = tr1;
	} while (++i < n);
	return std::make_tuple(tl0, tr0);
}

// We have both mono and stereo input (and produce stereo output)
static inline std::tuple<int32_t, int32_t> filterBothStereo(
	int32_t tl0, int32_t tr0, const int32_t* inS, void* buf, int n)
{
	assert(n > 0);
	const auto* inM = static_cast<const int32_t*>(buf);
	      auto* out = static_cast<      int16_t*>(buf);
	int i = 0;
	do {
		auto m = inM[i];
		int32_t tl1 = (511 * int64_t(tl0) + inS[2 * i + 0] + m) >> 9;
		int32_t tr1 = (511 * int64_t(tr0) + inS[2 * i + 1] + m) >> 9;
		out[2 * i + 0] = Math::clipIntToShort(tl1 - tl0);
		out[2 * i + 1] = Math::clipIntToShort(tr1 - tr0);
		tl0 = tl1;
		tr0 = tr1;
	} while (++i < n);
	return std::make_tuple(tl0, tr0);
}


void MSXMixer::generate(int16_t* output, EmuTime::param time, unsigned samples)
{
	// The code below is specialized for a lot of cases (before this
	// routine was _much_ shorter). This is done because this routine
	// ends up relatively high (top 5) in a profile run.
	// After these specialization this routine runs about two times
	// faster for the common cases (mono output or no sound at all).
	// In total emulation time this gave a speedup of about 2%.

	// When samples==0, call updateBuffer() but skip all further processing
	// (handling this as a special case allows to simplify the code below).
	if (samples == 0) {
		SSE_ALIGNED(int32_t dummyBuf[4]);
		for (auto& info : infos) {
			info.device->updateBuffer(0, dummyBuf, time);
		}
		return;
	}

	// +3 to allow processing samples in groups of 4 (and upto 3 samples
	// more than requested).
	VLA_SSE_ALIGNED(int32_t, stereoBuf, 2 * samples + 3);
	VLA_SSE_ALIGNED(int32_t, tmpBuf,    2 * samples + 3);
	// reuse 'output' as temporary storage
	auto* monoBuf = reinterpret_cast<int32_t*>(output);

	static const unsigned HAS_MONO_FLAG = 1;
	static const unsigned HAS_STEREO_FLAG = 2;
	unsigned usedBuffers = 0;

	// FIXME: The Infos should be ordered such that all the mono
	// devices are handled first
	for (auto& info : infos) {
		SoundDevice& device = *info.device;
		int l1 = info.left1;
		int r1 = info.right1;
		if (!device.isStereo()) {
			if (l1 == r1) {
				if (!(usedBuffers & HAS_MONO_FLAG)) {
					if (device.updateBuffer(samples, monoBuf, time)) {
						usedBuffers |= HAS_MONO_FLAG;
						mul(monoBuf, samples, l1);
					}
				} else {
					if (device.updateBuffer(samples, tmpBuf, time)) {
						mulAcc(monoBuf, tmpBuf, samples, l1);
					}
				}
			} else {
				if (!(usedBuffers & HAS_STEREO_FLAG)) {
					if (device.updateBuffer(samples, stereoBuf, time)) {
						usedBuffers |= HAS_STEREO_FLAG;
						mulExpand(stereoBuf, samples, l1, r1);
					}
				} else {
					if (device.updateBuffer(samples, tmpBuf, time)) {
						mulExpandAcc(stereoBuf, tmpBuf, samples, l1, r1);
					}
				}
			}
		} else {
			int l2 = info.left2;
			int r2 = info.right2;
			if (l1 == r2) {
				assert(l2 == 0);
				assert(r1 == 0);
				if (!(usedBuffers & HAS_STEREO_FLAG)) {
					if (device.updateBuffer(samples, stereoBuf, time)) {
						usedBuffers |= HAS_STEREO_FLAG;
						mul(stereoBuf, 2 * samples, l1);
					}
				} else {
					if (device.updateBuffer(samples, tmpBuf, time)) {
						mulAcc(stereoBuf, tmpBuf, 2 * samples, l1);
					}
				}
			} else {
				if (!(usedBuffers & HAS_STEREO_FLAG)) {
					if (device.updateBuffer(samples, stereoBuf, time)) {
						usedBuffers |= HAS_STEREO_FLAG;
						mulMix2(stereoBuf, samples, l1, l2, r1, r2);
					}
				} else {
					if (device.updateBuffer(samples, tmpBuf, time)) {
						mulMix2Acc(stereoBuf, tmpBuf, samples, l1, l2, r1, r2);
					}
				}
			}
		}
	}

	// DC removal filter
	switch (usedBuffers) {
	case 0: // no new input
		if (tl0 == tr0) {
			if ((-511 <= tl0) && (tl0 <= 0)) {
				// Output was zero, new input is zero,
				// after DC-filter output will still be zero.
				memset(output, 0, 2 * samples * sizeof(int16_t));
				tl0 = tr0 = 0;
			} else {
				// Output was not zero, but it was the same left and right.
				tl0 = filterMonoNull(tl0, output, samples);
				tr0 = tl0;
			}
		} else {
			std::tie(tl0, tr0) = filterStereoNull(tl0, tr0, output, samples);
		}
		break;

	case HAS_MONO_FLAG: // only mono
		assert(static_cast<void*>(monoBuf) == static_cast<void*>(output));
		if (tl0 == tr0) {
			// previous output was also mono
			tl0 = filterMonoMono(tl0, output, samples);
			tr0 = tl0;
		} else {
			// previous output was stereo, rarely triggers but needed for correctness
			std::tie(tl0, tr0) = filterStereoMono(tl0, tr0, output, samples);
		}
		break;

	case HAS_STEREO_FLAG: // only stereo
		std::tie(tl0, tr0) = filterStereoStereo(tl0, tr0, stereoBuf, output, samples);
		break;

	default: // mono + stereo
		assert(static_cast<void*>(monoBuf) == static_cast<void*>(output));
		std::tie(tl0, tr0) = filterBothStereo(tl0, tr0, stereoBuf, output, samples);
	}
}

bool MSXMixer::needStereoRecording() const
{
	return any_of(begin(infos), end(infos),
		[](const SoundDeviceInfo& info) {
			return info.device->isStereo() ||
			       info.balanceSetting->getInt() != 0; });
}

void MSXMixer::mute()
{
	if (muteCount == 0) {
		mixer.unregisterMixer(*this);
	}
	++muteCount;
}

void MSXMixer::unmute()
{
	--muteCount;
	if (muteCount == 0) {
		tl0 = tr0 = 0;
		mixer.registerMixer(*this);
	}
}

void MSXMixer::reInit()
{
	prevTime.reset(getCurrentTime());
	prevTime.setFreq(hostSampleRate / getEffectiveSpeed());
	reschedule();
}
void MSXMixer::reschedule()
{
	removeSyncPoints();
	reschedule2();
}
void MSXMixer::reschedule2()
{
	unsigned size = (!muteCount && fragmentSize) ? fragmentSize : 512;
	setSyncPoint(prevTime.getFastAdd(size));
}

void MSXMixer::setMixerParams(unsigned newFragmentSize, unsigned newSampleRate)
{
	// TODO old code checked that values did actually change,
	//      investigate if this optimization is worth it
	hostSampleRate = newSampleRate;
	fragmentSize = newFragmentSize;

	reInit(); // must come before call to setOutputRate()

	for (auto& info : infos) {
		info.device->setOutputRate(newSampleRate);
	}
}

void MSXMixer::setRecorder(AviRecorder* newRecorder)
{
	if ((recorder != nullptr) != (newRecorder != nullptr)) {
		setSynchronousMode(newRecorder != nullptr);
	}
	recorder = newRecorder;
}

void MSXMixer::update(const Setting& setting)
{
	if (&setting == &masterVolume) {
		updateMasterVolume();
	} else if (&setting == &speedSetting) {
		if (synchronousCounter == 0) {
			setMixerParams(fragmentSize, hostSampleRate);
		} else {
			// Avoid calling reInit() while recording because
			// each call causes a small hiccup in the sound (and
			// while recording this call anyway has no effect).
			// This was noticable while sliding the speed slider
			// in catapult (becuase this causes many changes in
			// the speed setting).
		}
	} else if (dynamic_cast<const IntegerSetting*>(&setting)) {
		auto it = find_if_unguarded(infos,
			[&](const SoundDeviceInfo& i) {
				return (i.volumeSetting .get() == &setting) ||
				       (i.balanceSetting.get() == &setting); });
		updateVolumeParams(*it);
	} else if (dynamic_cast<const StringSetting*>(&setting)) {
		changeRecordSetting(setting);
	} else if (dynamic_cast<const BooleanSetting*>(&setting)) {
		changeMuteSetting(setting);
	} else {
		UNREACHABLE;
	}
}

void MSXMixer::changeRecordSetting(const Setting& setting)
{
	for (auto& info : infos) {
		unsigned channel = 0;
		for (auto& s : info.channelSettings) {
			if (s.recordSetting.get() == &setting) {
				info.device->recordChannel(
					channel,
					Filename(s.recordSetting->getString().str()));
				return;
			}
			++channel;
		}
	}
	UNREACHABLE;
}

void MSXMixer::changeMuteSetting(const Setting& setting)
{
	for (auto& info : infos) {
		unsigned channel = 0;
		for (auto& s : info.channelSettings) {
			if (s.muteSetting.get() == &setting) {
				info.device->muteChannel(
					channel, s.muteSetting->getBoolean());
				return;
			}
			++channel;
		}
	}
	UNREACHABLE;
}

void MSXMixer::update(const ThrottleManager& /*throttleManager*/)
{
	//reInit();
	// TODO Should this be removed?
}

void MSXMixer::updateVolumeParams(SoundDeviceInfo& info)
{
	int mVolume = masterVolume.getInt();
	int dVolume = info.volumeSetting->getInt();
	float volume = info.defaultVolume * mVolume * dVolume / (100.0f * 100.0f);
	int balance = info.balanceSetting->getInt();
	float l1, r1, l2, r2;
	if (info.device->isStereo()) {
		if (balance < 0) {
			float b = (balance + 100.0f) / 100.0f;
			l1 = volume;
			r1 = 0.0f;
			l2 = volume * sqrtf(std::max(0.0f, 1.0f - b));
			r2 = volume * sqrtf(std::max(0.0f,        b));
		} else {
			float b = balance / 100.0f;
			l1 = volume * sqrtf(std::max(0.0f, 1.0f - b));
			r1 = volume * sqrtf(std::max(0.0f,        b));
			l2 = 0.0f;
			r2 = volume;
		}
	} else {
		// make sure that in case of rounding errors
		// we don't take sqrt() of negative numbers
		float b = (balance + 100.0f) / 200.0f;
		l1 = volume * sqrtf(std::max(0.0f, 1.0f - b));
		r1 = volume * sqrtf(std::max(0.0f,        b));
		l2 = r2 = 0.0f; // dummy
	}
	// 512 (9 bits) because in the DC filter we also have a factor 512, and
	// using the same allows to fold both (later) divisions into one.
	static_assert((1 << AMP_BITS) == 512, "");
	auto amp = info.device->getAmplificationFactor();
	auto ampL = amp.first .getRawValue();
	auto ampR = amp.second.getRawValue();
	info.left1  = lrintf(l1 * ampL);
	info.right1 = lrintf(r1 * ampR);
	info.left2  = lrintf(l2 * ampL);
	info.right2 = lrintf(r2 * ampR);
}

void MSXMixer::updateMasterVolume()
{
	for (auto& p : infos) {
		updateVolumeParams(p);
	}
}

void MSXMixer::updateSoftwareVolume(SoundDevice& device)
{
	auto it = find_if_unguarded(infos,
		[&](auto& i) { return i.device == &device; });
	updateVolumeParams(*it);
}

void MSXMixer::executeUntil(EmuTime::param time)
{
	updateStream(time);
	reschedule2();

	// This method gets called very regularly, typically 44100/512 = 86x
	// per second (even if sound is muted and even with sound_driver=null).
	// This rate is constant in real-time (compared to e.g. the VDP sync
	// points that are constant in emutime). So we can use this to
	// regularly exit from the main CPU emulation loop. Without this there
	// were problems like described in 'bug#563 Console very slow when
	// setting speed to low values like 1'.
	motherBoard.exitCPULoopSync();
}


// Sound device info

SoundDevice* MSXMixer::findDevice(string_view name) const
{
	auto it = find_if(begin(infos), end(infos),
		[&](const SoundDeviceInfo& i) {
			return i.device->getName() == name; });
	return (it != end(infos)) ? it->device : nullptr;
}

MSXMixer::SoundDeviceInfoTopic::SoundDeviceInfoTopic(
		InfoCommand& machineInfoCommand)
	: InfoTopic(machineInfoCommand, "sounddevice")
{
}

void MSXMixer::SoundDeviceInfoTopic::execute(
	array_ref<TclObject> tokens, TclObject& result) const
{
	auto& msxMixer = OUTER(MSXMixer, soundDeviceInfo);
	switch (tokens.size()) {
	case 2:
		for (auto& info : msxMixer.infos) {
			result.addListElement(info.device->getName());
		}
		break;
	case 3: {
		SoundDevice* device = msxMixer.findDevice(tokens[2].getString());
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

string MSXMixer::SoundDeviceInfoTopic::help(const vector<string>& /*tokens*/) const
{
	return "Shows a list of available sound devices.\n";
}

void MSXMixer::SoundDeviceInfoTopic::tabCompletion(vector<string>& tokens) const
{
	if (tokens.size() == 3) {
		vector<string_view> devices;
		auto& msxMixer = OUTER(MSXMixer, soundDeviceInfo);
		for (auto& info : msxMixer.infos) {
			devices.emplace_back(info.device->getName());
		}
		completeString(tokens, devices);
	}
}

} // namespace openmsx
