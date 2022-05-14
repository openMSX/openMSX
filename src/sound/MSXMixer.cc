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
#include "FileOperations.hh"
#include "CliComm.hh"
#include "stl.hh"
#include "aligned.hh"
#include "enumerate.hh"
#include "one_of.hh"
#include "outer.hh"
#include "ranges.hh"
#include "unreachable.hh"
#include "view.hh"
#include "vla.hh"
#include "xrange.hh"
#include <cassert>
#include <cmath>
#include <cstring>
#include <memory>
#include <tuple>

#ifdef __SSE2__
#include "emmintrin.h"
#endif

namespace openmsx {

MSXMixer::MSXMixer(Mixer& mixer_, MSXMotherBoard& motherBoard_,
                   GlobalSettings& globalSettings)
	: Schedulable(motherBoard_.getScheduler())
	, mixer(mixer_)
	, motherBoard(motherBoard_)
	, commandController(motherBoard.getMSXCommandController())
	, masterVolume(mixer.getMasterVolume())
	, speedManager(globalSettings.getSpeedManager())
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
	speedManager.attach(*this);
	throttleManager.attach(*this);
}

MSXMixer::~MSXMixer()
{
	if (recorder) {
		recorder->stop();
	}
	assert(infos.empty());

	throttleManager.detach(*this);
	speedManager.detach(*this);
	masterVolume.detach(*this);

	mute(); // calls Mixer::unregisterMixer()
}

MSXMixer::SoundDeviceInfo::SoundDeviceInfo(unsigned numChannels)
	: channelSettings(numChannels)
{
}

void MSXMixer::registerSound(SoundDevice& device, float volume,
                             int balance, unsigned numChannels)
{
	// TODO read volume/balance(mode) from config file
	const std::string& name = device.getName();
	SoundDeviceInfo info(numChannels);
	info.device = &device;
	info.defaultVolume = volume;
	info.volumeSetting = std::make_unique<IntegerSetting>(
		commandController, tmpStrCat(name, "_volume"),
		"the volume of this sound chip", 75, 0, 100);
	info.balanceSetting = std::make_unique<IntegerSetting>(
		commandController, tmpStrCat(name, "_balance"),
		"the balance of this sound chip", balance, -100, 100);

	info.volumeSetting->attach(*this);
	info.balanceSetting->attach(*this);

	for (auto&& [i, channelSettings] : enumerate(info.channelSettings)) {
		auto ch_name = tmpStrCat(name, "_ch", i + 1);

		channelSettings.record = std::make_unique<StringSetting>(
			commandController, tmpStrCat(ch_name, "_record"),
			"filename to record this channel to",
			std::string_view{}, Setting::DONT_SAVE);
		channelSettings.record->attach(*this);

		channelSettings.mute = std::make_unique<BooleanSetting>(
			commandController, tmpStrCat(ch_name, "_mute"),
			"sets mute-status of individual sound channels",
			false, Setting::DONT_SAVE);
		channelSettings.mute->attach(*this);
	}

	device.setOutputRate(getSampleRate(), speedManager.getSpeed());
	auto& i = infos.emplace_back(std::move(info));
	updateVolumeParams(i);

	commandController.getCliComm().update(CliComm::SOUNDDEVICE, device.getName(), "add");
}

void MSXMixer::unregisterSound(SoundDevice& device)
{
	auto it = rfind_unguarded(infos, &device, &SoundDeviceInfo::device);
	it->volumeSetting->detach(*this);
	it->balanceSetting->detach(*this);
	for (auto& s : it->channelSettings) {
		s.record->detach(*this);
		s.mute->detach(*this);
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
	return synchronousCounter ? 1.0 : speedManager.getSpeed();
}

void MSXMixer::updateStream(EmuTime::param time)
{
	union {
		float mixBuffer[8192 * 2]; // make sure buffer is 32-bit aligned
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
static inline void mul(float* buf, int n, float f)
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
	float* __restrict acc, const float* __restrict mul, int n, float f)
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
static inline void mulExpand(float* buf, int n, float l, float r)
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
	float* __restrict acc, const float* __restrict mul, int n,
	float l, float r)
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
static inline void mulMix2(float* buf, int n, float l1, float l2, float r1, float r2)
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
	float* __restrict acc, const float* __restrict mul, int n,
	float l1, float l2, float r1, float r2)
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
//   44100Hz --> cutoff freq = 14Hz
//   22050Hz                     7Hz
constexpr auto R = 511.0f / 512.0f;

// No new input, previous output was (non-zero) mono.
static inline float filterMonoNull(float t0, float* __restrict out, int n)
{
	assert(n > 0);
	int i = 0;
	do {
		auto t1 = R * t0;
		auto s = t1 - t0;
		out[2 * i + 0] = s;
		out[2 * i + 1] = s;
		t0 = t1;
	} while (++i < n);
	return t0;
}

// No new input, previous output was (non-zero) stereo.
static inline std::tuple<float, float> filterStereoNull(
	float tl0, float tr0, float* __restrict out, int n)
{
	assert(n > 0);
	int i = 0;
	do {
		float tl1 = R * tl0;
		float tr1 = R * tr0;
		out[2 * i + 0] = tl1 - tl0;
		out[2 * i + 1] = tr1 - tr0;
		tl0 = tl1;
		tr0 = tr1;
	} while (++i < n);
	return {tl0, tr0};
}

// New input is mono, previous output was also mono.
static inline float filterMonoMono(float t0, const float* __restrict in,
                                   float* __restrict out, int n)
{
	assert(n > 0);
	int i = 0;
	do {
		auto t1 = R * t0 + in[i];
		auto s = t1 - t0;
		out[2 * i + 0] = s;
		out[2 * i + 1] = s;
		t0 = t1;
	} while (++i < n);
	return t0;
}

// New input is mono, previous output was stereo
static inline std::tuple<float, float>
filterStereoMono(float tl0, float tr0, const float* __restrict in,
                 float* __restrict out, int n)
{
	assert(n > 0);
	int i = 0;
	do {
		auto x = in[i];
		auto tl1 = R * tl0 + x;
		auto tr1 = R * tr0 + x;
		out[2 * i + 0] = tl1 - tl0;
		out[2 * i + 1] = tr1 - tr0;
		tl0 = tl1;
		tr0 = tr1;
	} while (++i < n);
	return {tl0, tr0};
}

// New input is stereo, (previous output either mono/stereo)
static inline std::tuple<float, float>
filterStereoStereo(float tl0, float tr0, const float* __restrict in,
                   float* __restrict out, int n)
{
	assert(n > 0);
	int i = 0;
	do {
		auto tl1 = R * tl0 + in[2 * i + 0];
		auto tr1 = R * tr0 + in[2 * i + 1];
		out[2 * i + 0] = tl1 - tl0;
		out[2 * i + 1] = tr1 - tr0;
		tl0 = tl1;
		tr0 = tr1;
	} while (++i < n);
	return {tl0, tr0};
}

// We have both mono and stereo input (and produce stereo output)
static inline std::tuple<float, float>
filterBothStereo(float tl0, float tr0, const float* __restrict inM,
                 const float* __restrict inS, float* __restrict out, int n)
{
	assert(n > 0);
	int i = 0;
	do {
		auto m = inM[i];
		auto tl1 = R * tl0 + inS[2 * i + 0] + m;
		auto tr1 = R * tr0 + inS[2 * i + 1] + m;
		out[2 * i + 0] = tl1 - tl0;
		out[2 * i + 1] = tr1 - tr0;
		tl0 = tl1;
		tr0 = tr1;
	} while (++i < n);
	return {tl0, tr0};
}

static bool approxEqual(float x, float y)
{
	constexpr float threshold = 1.0f / 32768;
	return std::abs(x - y) < threshold;
}

void MSXMixer::generate(float* output, EmuTime::param time, unsigned samples)
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
		ALIGNAS_SSE float dummyBuf[4];
		for (auto& info : infos) {
			bool ignore = info.device->updateBuffer(0, dummyBuf, time);
			(void)ignore;
		}
		return;
	}

	// +3 to allow processing samples in groups of 4 (and upto 3 samples
	// more than requested).
	VLA_SSE_ALIGNED(float, monoBuf,       samples + 3);
	VLA_SSE_ALIGNED(float, stereoBuf, 2 * samples + 3);
	VLA_SSE_ALIGNED(float, tmpBuf,    2 * samples + 3);

	constexpr unsigned HAS_MONO_FLAG = 1;
	constexpr unsigned HAS_STEREO_FLAG = 2;
	unsigned usedBuffers = 0;

	// FIXME: The Infos should be ordered such that all the mono
	// devices are handled first
	for (auto& info : infos) {
		SoundDevice& device = *info.device;
		auto l1 = info.left1;
		auto r1 = info.right1;
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
			auto l2 = info.left2;
			auto r2 = info.right2;
			if (l1 == r2) {
				assert(l2 == 0.0f);
				assert(r1 == 0.0f);
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
		if (approxEqual(tl0, tr0)) {
			if (approxEqual(tl0, 0.0f)) {
				// Output was zero, new input is zero,
				// after DC-filter output will still be zero.
				memset(output, 0, 2 * samples * sizeof(float));
				tl0 = tr0 = 0.0f;
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
		if (approxEqual(tl0, tr0)) {
			// previous output was also mono
			tl0 = filterMonoMono(tl0, monoBuf, output, samples);
			tr0 = tl0;
		} else {
			// previous output was stereo, rarely triggers but needed for correctness
			std::tie(tl0, tr0) = filterStereoMono(tl0, tr0, monoBuf, output, samples);
		}
		break;

	case HAS_STEREO_FLAG: // only stereo
		std::tie(tl0, tr0) = filterStereoStereo(tl0, tr0, stereoBuf, output, samples);
		break;

	default: // mono + stereo
		std::tie(tl0, tr0) = filterBothStereo(tl0, tr0, monoBuf, stereoBuf, output, samples);
	}
}

bool MSXMixer::needStereoRecording() const
{
	return ranges::any_of(infos, [](auto& info) {
		return info.device->isStereo() ||
		       info.balanceSetting->getInt() != 0;
	});
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
		tl0 = tr0 = 0.0f;
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
		info.device->setOutputRate(newSampleRate, speedManager.getSpeed());
	}
}

void MSXMixer::setRecorder(AviRecorder* newRecorder)
{
	if ((recorder != nullptr) != (newRecorder != nullptr)) {
		setSynchronousMode(newRecorder != nullptr);
	}
	recorder = newRecorder;
}

void MSXMixer::update(const Setting& setting) noexcept
{
	if (&setting == &masterVolume) {
		updateMasterVolume();
	} else if (dynamic_cast<const IntegerSetting*>(&setting)) {
		auto it = find_if_unguarded(infos,
			[&](const SoundDeviceInfo& i) {
				return &setting == one_of(i.volumeSetting .get(),
				                          i.balanceSetting.get()); });
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
		for (auto&& [channel, settings] : enumerate(info.channelSettings)) {
			if (settings.record.get() == &setting) {
				info.device->recordChannel(
					unsigned(channel),
					Filename(FileOperations::expandTilde(std::string(
						 settings.record->getString()))));
				return;
			}
		}
	}
	UNREACHABLE;
}

void MSXMixer::changeMuteSetting(const Setting& setting)
{
	for (auto& info : infos) {
		for (auto&& [channel, settings] : enumerate(info.channelSettings)) {
			if (settings.mute.get() == &setting) {
				info.device->muteChannel(
					unsigned(channel), settings.mute->getBoolean());
				return;
			}
		}
	}
	UNREACHABLE;
}

void MSXMixer::update(const SpeedManager& /*speedManager*/) noexcept
{
	if (synchronousCounter == 0) {
		setMixerParams(fragmentSize, hostSampleRate);
	} else {
		// Avoid calling reInit() while recording because
		// each call causes a small hiccup in the sound (and
		// while recording this call anyway has no effect).
		// This was noticeable while sliding the speed slider
		// in catapult (because this causes many changes in
		// the speed setting).
	}
}

void MSXMixer::update(const ThrottleManager& /*throttleManager*/) noexcept
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
	auto [l1, r1, l2, r2] = [&] {
		if (info.device->isStereo()) {
			if (balance < 0) {
				float b = (balance + 100.0f) / 100.0f;
				return std::tuple{
					/*l1 =*/ volume,
					/*r1 =*/ 0.0f,
					/*l2 =*/ volume * sqrtf(std::max(0.0f, 1.0f - b)),
					/*r2 =*/ volume * sqrtf(std::max(0.0f,        b))
				};
			} else {
				float b = balance / 100.0f;
				return std::tuple{
					/*l1 =*/ volume * sqrtf(std::max(0.0f, 1.0f - b)),
					/*r1 =*/ volume * sqrtf(std::max(0.0f,        b)),
					/*l2 =*/ 0.0f,
					/*r2 =*/ volume
				};
			}
		} else {
			// make sure that in case of rounding errors
			// we don't take sqrt() of negative numbers
			float b = (balance + 100.0f) / 200.0f;
			return std::tuple{
				/*l1 =*/ volume * sqrtf(std::max(0.0f, 1.0f - b)),
				/*r1 =*/ volume * sqrtf(std::max(0.0f,        b)),
				/*l2 =*/ 0.0f, // dummy
				/*r2 =*/ 0.0f  // dummy
			};
		}
	}();
	auto [ampL, ampR] = info.device->getAmplificationFactor();
	info.left1  = l1 * ampL;
	info.right1 = r1 * ampR;
	info.left2  = l2 * ampL;
	info.right2 = r2 * ampR;
}

void MSXMixer::updateMasterVolume()
{
	for (auto& p : infos) {
		updateVolumeParams(p);
	}
}

void MSXMixer::updateSoftwareVolume(SoundDevice& device)
{
	auto it = find_unguarded(infos, &device, &SoundDeviceInfo::device);
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

SoundDevice* MSXMixer::findDevice(std::string_view name) const
{
	auto it = ranges::find(infos, name,
		[](auto& i) { return i.device->getName(); });
	return (it != end(infos)) ? it->device : nullptr;
}

MSXMixer::SoundDeviceInfoTopic::SoundDeviceInfoTopic(
		InfoCommand& machineInfoCommand)
	: InfoTopic(machineInfoCommand, "sounddevice")
{
}

void MSXMixer::SoundDeviceInfoTopic::execute(
	span<const TclObject> tokens, TclObject& result) const
{
	auto& msxMixer = OUTER(MSXMixer, soundDeviceInfo);
	switch (tokens.size()) {
	case 2:
		result.addListElements(view::transform(
			msxMixer.infos,
			[](auto& info) { return info.device->getName(); }));
		break;
	case 3: {
		SoundDevice* device = msxMixer.findDevice(tokens[2].getString());
		if (!device) {
			throw CommandException("Unknown sound device");
		}
		result = device->getDescription();
		break;
	}
	default:
		throw CommandException("Too many parameters");
	}
}

std::string MSXMixer::SoundDeviceInfoTopic::help(span<const TclObject> /*tokens*/) const
{
	return "Shows a list of available sound devices.\n";
}

void MSXMixer::SoundDeviceInfoTopic::tabCompletion(std::vector<std::string>& tokens) const
{
	if (tokens.size() == 3) {
		completeString(tokens, view::transform(
			OUTER(MSXMixer, soundDeviceInfo).infos,
			[](auto& info) -> std::string_view { return info.device->getName(); }));
	}
}

} // namespace openmsx
